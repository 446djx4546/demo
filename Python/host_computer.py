import sys
import time
import socket
import pyqtgraph as pg
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QPushButton, QLabel, QLineEdit, 
                             QGroupBox, QRadioButton, QSpinBox, QDoubleSpinBox, 
                             QSlider, QGridLayout, QMessageBox, QCheckBox)  # 【修改1：增加了 QCheckBox】
from PyQt5.QtCore import QThread, pyqtSignal, Qt

# ==========================================
# TCP 通信子线程
# ==========================================
class TCPClientThread(QThread):
    data_received = pyqtSignal(float, float, int, int)
    status_updated = pyqtSignal(str, bool)
    
    # 【修改2】：同步数据信号增加到 10 个参数，最后一个代表 led_switch 状态
    sync_received = pyqtSignal(int, int, int, int, int, int, int, int, int, int)

    def __init__(self):
        super().__init__()
        self.is_running = False
        self.socket = None
        self.ip = ""
        self.port = 0

    def start_connection(self, ip, port):
        self.ip = ip
        self.port = port
        self.is_running = True
        self.start()

    def run(self):
        self.status_updated.emit(f"正在连接 {self.ip}:{self.port} ...", False)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.settimeout(3)
        
        try:
            self.socket.connect((self.ip, self.port))
            self.status_updated.emit("✅ 连接成功，正在同步数据...", True)
            self.socket.settimeout(None) 
            
            time.sleep(0.5) 
            self.send_command("GET_SYNC\n")
            
            buffer = ""
            while self.is_running:
                recv_data = self.socket.recv(1024)
                if not recv_data:
                    self.status_updated.emit("❌ 服务器(ESP8266)已断开连接", False)
                    break
                    
                buffer += recv_data.decode('utf-8', errors='ignore')
                
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    line = line.strip()
                    if not line:
                        continue
                    
                    # 【修改3】：拦截并解析包含 10 个参数的同步数据包
                    if line.startswith("SYNC:"):
                        try:
                            vals = line[5:].split(',')
                            if len(vals) == 10:
                                t1, t2, h, m, l, mode, led, servo, motor, led_sw = map(int, vals)
                                self.sync_received.emit(t1, t2, h, m, l, mode, led, servo, motor, led_sw)
                            elif len(vals) == 9:
                                # 兼容单片机尚未更新代码的旧情况，默认开关为开(1)
                                t1, t2, h, m, l, mode, led, servo, motor = map(int, vals)
                                self.sync_received.emit(t1, t2, h, m, l, mode, led, servo, motor, 1)
                        except Exception as e:
                            print(f"解析同步包失败: {e}")
                        continue 
                    
                    # 原本的波形数据解析
                    vals = line.split(',')
                    if len(vals) == 4:
                        try:
                            t = float(vals[0])
                            m = float(vals[1])
                            h = int(vals[2])
                            l = int(vals[3])
                            self.data_received.emit(t, m, h, l)
                        except ValueError:
                            pass

        except Exception as e:
            self.status_updated.emit(f"❌ 连接异常/超时", False)
        finally:
            self.stop()

    def send_command(self, cmd_str):
        if self.socket and self.is_running:
            try:
                self.socket.send(cmd_str.encode('utf-8'))
                print(f"已发送指令: {cmd_str.strip()}")
            except Exception as e:
                print(f"发送失败: {e}")

    def stop(self):
        self.is_running = False
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None

# ==========================================
# 上位机主界面
# ==========================================
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("仓储环境多传感器智能控制系统")
        self.resize(1200, 800)

        self.max_points = 200
        self.data_t = []; self.data_m = []; self.data_h = []; self.data_l = []

        self.tcp_thread = TCPClientThread()
        self.tcp_thread.data_received.connect(self.update_charts)
        self.tcp_thread.status_updated.connect(self.update_connection_status)
        self.tcp_thread.sync_received.connect(self.sync_ui_from_mcu)

        self.init_ui()

    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QHBoxLayout(central_widget)

        left_panel = QVBoxLayout()
        left_panel.setContentsMargins(10, 10, 10, 10)
        
        group_conn = QGroupBox("ESP8266 网络连接")
        layout_conn = QGridLayout()
        self.input_ip = QLineEdit("192.168.4.1")
        self.input_port = QLineEdit("8080")
        self.btn_connect = QPushButton("连接设备")
        self.btn_connect.setMinimumHeight(35)
        self.btn_connect.clicked.connect(self.toggle_connection)
        self.label_status = QLabel("等待连接...")
        self.label_status.setStyleSheet("color: gray; font-weight: bold;")
        
        layout_conn.addWidget(QLabel("IP地址:"), 0, 0); layout_conn.addWidget(self.input_ip, 0, 1)
        layout_conn.addWidget(QLabel("端口号:"), 1, 0); layout_conn.addWidget(self.input_port, 1, 1)
        layout_conn.addWidget(self.btn_connect, 2, 0, 1, 2)
        layout_conn.addWidget(self.label_status, 3, 0, 1, 2)
        group_conn.setLayout(layout_conn)
        left_panel.addWidget(group_conn)

        group_mode = QGroupBox("系统控制设置")
        layout_mode = QHBoxLayout()
        self.radio_auto = QRadioButton("自动模式 (Auto)")
        self.radio_manual = QRadioButton("手动模式 (Manual)")
        self.radio_auto.setChecked(True)
        self.radio_auto.toggled.connect(self.on_mode_changed)
        
        # 【修改4】：新增 LED 总开关 CheckBox
        self.checkbox_led_switch = QCheckBox("LED 总开关 (允许点亮)")
        self.checkbox_led_switch.setChecked(True)
        self.checkbox_led_switch.setStyleSheet("font-weight: bold; color: #d35400;")
        self.checkbox_led_switch.toggled.connect(self.on_led_switch_changed)
        
        layout_mode.addWidget(self.radio_auto)
        layout_mode.addWidget(self.radio_manual)
        layout_mode.addWidget(self.checkbox_led_switch)
        group_mode.setLayout(layout_mode)
        left_panel.addWidget(group_mode)

        self.group_auto_settings = QGroupBox("自动模式 - 报警阈值设置")
        layout_thres = QGridLayout()
        self.spin_t_max = QDoubleSpinBox(); self.spin_t_max.setRange(-20, 100); self.spin_t_max.setValue(30.0)
        self.spin_t2_max = QDoubleSpinBox(); self.spin_t2_max.setRange(-20, 100); self.spin_t2_max.setValue(35.0)
        self.spin_h_max = QSpinBox(); self.spin_h_max.setRange(0, 100); self.spin_h_max.setValue(70)
        self.spin_m_max = QDoubleSpinBox(); self.spin_m_max.setRange(0, 9999); self.spin_m_max.setValue(1000.0)
        self.spin_l_max = QSpinBox(); self.spin_l_max.setRange(0, 100); self.spin_l_max.setValue(50)
        
        layout_thres.addWidget(QLabel("Temp Thres 1(℃):"), 0, 0); layout_thres.addWidget(self.spin_t_max, 0, 1)
        layout_thres.addWidget(QLabel("Temp Thres 2(℃):"), 1, 0); layout_thres.addWidget(self.spin_t2_max, 1, 1)
        layout_thres.addWidget(QLabel("Humi Thres(%):"),   2, 0); layout_thres.addWidget(self.spin_h_max, 2, 1)
        layout_thres.addWidget(QLabel("Gas Thres(PPM):"),  3, 0); layout_thres.addWidget(self.spin_m_max, 3, 1)
        layout_thres.addWidget(QLabel("Target Light(%):"), 4, 0); layout_thres.addWidget(self.spin_l_max, 4, 1)
        
        self.btn_set_thres = QPushButton("下发阈值至单片机")
        self.btn_set_thres.clicked.connect(self.send_thresholds)
        layout_thres.addWidget(self.btn_set_thres, 5, 0, 1, 2)
        self.group_auto_settings.setLayout(layout_thres)
        left_panel.addWidget(self.group_auto_settings)

        self.group_manual_settings = QGroupBox("手动模式 - 执行机构控制")
        layout_ctrl = QGridLayout()
        
        self.slider_led = QSlider(Qt.Horizontal); self.slider_led.setRange(0, 100)
        self.label_led_val = QLabel("0%")
        self.slider_led.valueChanged.connect(lambda v: self.label_led_val.setText(f"{v}%"))
        
        self.slider_servo = QSlider(Qt.Horizontal); self.slider_servo.setRange(0, 180)
        self.label_servo_val = QLabel("0°")
        self.slider_servo.valueChanged.connect(lambda v: self.label_servo_val.setText(f"{v}°"))
        
        self.slider_motor = QSlider(Qt.Horizontal); self.slider_motor.setRange(-100, 100)
        self.label_motor_val = QLabel("0%")
        self.slider_motor.valueChanged.connect(lambda v: self.label_motor_val.setText(f"{v}%"))

        layout_ctrl.addWidget(QLabel("LED 预设亮度:"), 0, 0); layout_ctrl.addWidget(self.slider_led, 0, 1); layout_ctrl.addWidget(self.label_led_val, 0, 2)
        layout_ctrl.addWidget(QLabel("舵机 角度:"), 1, 0); layout_ctrl.addWidget(self.slider_servo, 1, 1); layout_ctrl.addWidget(self.label_servo_val, 1, 2)
        layout_ctrl.addWidget(QLabel("排风机速度:"), 2, 0); layout_ctrl.addWidget(self.slider_motor, 2, 1); layout_ctrl.addWidget(self.label_motor_val, 2, 2)
        
        self.btn_set_ctrl = QPushButton("立即执行控制")
        self.btn_set_ctrl.clicked.connect(self.send_manual_controls)
        layout_ctrl.addWidget(self.btn_set_ctrl, 3, 0, 1, 3)
        self.group_manual_settings.setLayout(layout_ctrl)
        left_panel.addWidget(self.group_manual_settings)

        left_panel.addStretch()

        right_panel = QVBoxLayout()
        pg.setConfigOptions(antialias=True)
        self.graph_layout = pg.GraphicsLayoutWidget()
        right_panel.addWidget(self.graph_layout)

        self.p_t = self.graph_layout.addPlot(title="温度监控 (℃)")
        self.p_t.showGrid(x=True, y=True, alpha=0.3)
        self.curve_t = self.p_t.plot(pen=pg.mkPen('r', width=2))
        
        self.p_h = self.graph_layout.addPlot(title="湿度监控 (%)")
        self.p_h.showGrid(x=True, y=True, alpha=0.3)
        self.p_h.setYRange(0, 100)
        self.curve_h = self.p_h.plot(pen=pg.mkPen('b', width=2))
        
        self.graph_layout.nextRow()
        
        self.p_m = self.graph_layout.addPlot(title="可燃气体 MQ2 (PPM)")
        self.p_m.showGrid(x=True, y=True, alpha=0.3)
        self.curve_m = self.p_m.plot(pen=pg.mkPen('y', width=2))
        
        self.p_l = self.graph_layout.addPlot(title="光照强度 (0-100)")
        self.p_l.showGrid(x=True, y=True, alpha=0.3)
        self.p_l.setYRange(0, 100)
        self.curve_l = self.p_l.plot(pen=pg.mkPen('g', width=2))

        main_layout.addLayout(left_panel, stretch=1)
        main_layout.addLayout(right_panel, stretch=3)
        self.on_mode_changed()

    # 【修改5】：增加参数 led_sw，并在 UI 上同步
    def sync_ui_from_mcu(self, t1, t2, h, m, l, mode, led, servo, motor, led_sw):
        self.spin_t_max.setValue(t1)
        self.spin_t2_max.setValue(t2)
        self.spin_h_max.setValue(h)
        self.spin_m_max.setValue(m)
        self.spin_l_max.setValue(l)

        self.radio_auto.blockSignals(True)
        self.radio_manual.blockSignals(True)
        if mode == 0:
            self.radio_auto.setChecked(True)
        else:
            self.radio_manual.setChecked(True)
        self.radio_auto.blockSignals(False)
        self.radio_manual.blockSignals(False)

        # 同步 LED 总开关
        self.checkbox_led_switch.blockSignals(True)
        self.checkbox_led_switch.setChecked(led_sw == 1)
        self.checkbox_led_switch.blockSignals(False)

        self.slider_led.blockSignals(True)
        self.slider_servo.blockSignals(True)
        self.slider_motor.blockSignals(True)
        
        self.slider_led.setValue(led)
        self.label_led_val.setText(f"{led}%")
        
        servo_angle = 90 if servo == 1 else 0
        self.slider_servo.setValue(servo_angle)
        self.label_servo_val.setText(f"{servo_angle}°")
        
        self.slider_motor.setValue(motor)
        self.label_motor_val.setText(f"{motor}%")

        self.slider_led.blockSignals(False)
        self.slider_servo.blockSignals(False)
        self.slider_motor.blockSignals(False)
        
        is_auto = self.radio_auto.isChecked()
        self.group_auto_settings.setEnabled(True)
        self.group_manual_settings.setEnabled(not is_auto)
        
        self.label_status.setText("✅ 参数同步完成，正在监控数据...")

    def toggle_connection(self):
        if not self.tcp_thread.is_running:
            ip = self.input_ip.text()
            try:
                port = int(self.input_port.text())
            except:
                QMessageBox.warning(self, "错误", "端口号必须是数字！")
                return
            self.tcp_thread.start_connection(ip, port)
            self.btn_connect.setText("断开连接")
            self.btn_connect.setStyleSheet("color: red; font-weight: bold;")
        else:
            self.tcp_thread.stop()
            self.btn_connect.setText("连接设备")
            self.btn_connect.setStyleSheet("color: black;")
            self.label_status.setText("已手动断开")
            self.label_status.setStyleSheet("color: gray; font-weight: bold;")

    def update_connection_status(self, msg, is_connected):
        self.label_status.setText(msg)
        if "❌" in msg:
            self.label_status.setStyleSheet("color: red; font-weight: bold;")
            self.btn_connect.setText("连接设备")
            self.btn_connect.setStyleSheet("color: black;")
        elif "✅" in msg:
            self.label_status.setStyleSheet("color: green; font-weight: bold;")

    def on_mode_changed(self):
        is_auto = self.radio_auto.isChecked()
        self.group_auto_settings.setEnabled(True)
        self.group_manual_settings.setEnabled(not is_auto)
        cmd = "MODE:AUTO\n" if is_auto else "MODE:MANUAL\n"
        self.tcp_thread.send_command(cmd)

    # 【修改6】：增加总开关向单片机发送指令的逻辑
    def on_led_switch_changed(self, checked):
        if not self.tcp_thread.is_running:
            return # 未连接时不提示，因为可能只是在操作UI
        state = 1 if checked else 0
        cmd = f"SWITCH:{state}\n"
        self.tcp_thread.send_command(cmd)

    def send_thresholds(self):
        if not self.tcp_thread.is_running:
            QMessageBox.warning(self, "提示", "请先连接 ESP8266！")
            return
        t1 = self.spin_t_max.value()
        t2 = self.spin_t2_max.value()
        h = self.spin_h_max.value()
        m = self.spin_m_max.value()
        l = self.spin_l_max.value()
        cmd = f"THRES:{int(t1)},{int(t2)},{int(h)},{int(m)},{int(l)}\n"
        self.tcp_thread.send_command(cmd)
        QMessageBox.information(self, "成功", "全部 5 个阈值已下发并保存！")

    def send_manual_controls(self):
        if not self.tcp_thread.is_running:
            QMessageBox.warning(self, "提示", "请先连接 ESP8266！")
            return
        led = self.slider_led.value()
        servo = self.slider_servo.value()
        motor = self.slider_motor.value()
        cmd = f"CTRL:{led},{servo},{motor}\n"
        self.tcp_thread.send_command(cmd)

    def update_charts(self, t, m, h, l):
        self.data_t.append(t); self.data_m.append(m)
        self.data_h.append(h); self.data_l.append(l)

        if len(self.data_t) > self.max_points:
            self.data_t.pop(0); self.data_m.pop(0)
            self.data_h.pop(0); self.data_l.pop(0)

        self.curve_t.setData(self.data_t)
        self.curve_m.setData(self.data_m)
        self.curve_h.setData(self.data_h)
        self.curve_l.setData(self.data_l)

        self.adjust_y_axis(self.p_t, self.data_t, 2.0)
        self.adjust_y_axis(self.p_m, self.data_m, 20.0)

    def adjust_y_axis(self, plot_widget, data_list, min_range):
        if not data_list: return
        d_min, d_max = min(data_list), max(data_list)
        if (d_max - d_min) < min_range:
            center = (d_max + d_min) / 2.0
            plot_widget.setYRange(center - min_range/2, center + min_range/2)
        else:
            plot_widget.enableAutoRange(axis='y')

    def closeEvent(self, event):
        self.tcp_thread.stop()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())