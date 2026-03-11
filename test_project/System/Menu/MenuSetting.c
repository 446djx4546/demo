#include "MenuSetting.h"
#include "OLED.h"       
#include "Buzzer.h"     
#include "Thermal.h"    
#include "LightSensor.h"
#include "DHT11.h"      
#include "MQ2.h"
#include "Key.h"
#include "Delay.h"
#include <stdio.h>

#include "led_pwm.h"
#include "servo.h"
#include "motor.h"
#include "Store.h"
#include "ESP8266.h"

extern uint8_t ESP8266_RX_FLAG;
extern void ESP8266_ParseCommand(void);

Node_t* rootNode;

/* ================== 全局变量：阈值与模式 ================== */
int temp_limit_1 = 30;     
int temp_limit_2 = 35;     
int humi_limit = 70;       
int gas_limit  = 1000;   
int target_light = 50;     // 新增：目标光照恒定值 (0~100)
uint8_t sys_mode = 0;      

/* ================== 全局传感器缓存 ================== */
float current_temp = 0.0f;
uint8_t current_humi = 0;
float current_ppm = 0.0f;
uint8_t current_light = 0;

/* ================== 核心：后台监控系统 (包含极速PID) ================== */
void Run_Background_Task(void) {
    static uint16_t loop_tick = 0;
    
    // PID 控制器静态变量
    static float integral = 0.0f;
    static int last_error = 0;
    static float last_pid_out = 0.0f;

    loop_tick++;

    // =========================================================================
    // 接收串口信息并触发解析（每次调用都执行，确保快速响应）
    // =========================================================================
    if (ESP8266_RX_FLAG == 1) {
        ESP8266_ParseCommand();
        ESP8266_RX_FLAG = 0; // 执行完毕，放下旗帜
    }

    // =========================================================================
    // 1. 光照 PID 控制区 (每次调用都执行，约 50ms)
    // =========================================================================
    
    // 获取光照原始值
    current_light = LightSensor_GetIntensity();
    
    if (sys_mode == 0) {
        float Kp = 0.6f;   // 稍微提升一点 P，让跟踪更快 
        float Ki = 0.1f;   
        float Kd = 0.0f;   

        int error = target_light - current_light; 
        float pid_out;

        // 【神技 1：误差反转（越过目标）时，瞬间大砍积分旧账】
        // 当你捏住又松手时，光线瞬间变亮，误差从 正(+25) 瞬间变成 负(-3)
        // 此时之前的“要变亮”的积分完全成了累赘，直接将其削减 50% 实现“极速退饱和”
        if ((error < 0 && last_error > 0) || (error > 0 && last_error < 0)) {
            integral *= 0.5f; 
        }

        // 【修改 2：放宽积分分离区间，让它能顺滑接管】
        if (error > -35 && error < 35) {
            integral += (float)error;
        }
        
        if (integral > 1000.0f) integral = 1000.0f; 
        if (integral < 0.0f)    integral = 0.0f; 

        // 【修改 3：彻底移除导致卡死的“死区冻结(Deadzone)”逻辑】
        // 光照不需要死区，直接让公式时刻计算，微小的底噪靠浮点转整型就自然过滤了
        pid_out = Kp * error + Ki * integral;
        last_error = error;
        
        // 输出限幅
        if (pid_out > 100.0f) pid_out = 100.0f;
        if (pid_out < 0.0f)   pid_out = 0.0f;

        last_pid_out = pid_out;

        LED_SetBrightness((uint8_t)pid_out);
    } else {
        // 手动模式：同步必须与上面的 Ki 保持绝对一致
        float Ki = 0.1f; 
        integral = (float)LED_GetBrightness() / Ki; 
        last_error = 0;
        last_pid_out = (float)LED_GetBrightness();
    }

    // =========================================================================
    // 2. ESP8266 数据下发区 (每 2 次执行，约 100ms)
    // =========================================================================
    if (loop_tick % 2 == 0) {
        char sendBuffer[64];
        float t_f = Thermal_GetTemp();
        float m_f = MQ2_GetPPM();
        // 格式: temp, mq2, humi, light
        sprintf(sendBuffer, "%.2f,%.2f,%d,%d\n", t_f, m_f, current_humi, current_light);

        ESP8266_SendData(sendBuffer);
    }

    // =========================================================================
    // 3. 传感器读取与报警仲裁区 (每 10 次执行，约 500ms)
    // =========================================================================
    if (loop_tick % 5 == 0) {

        // 1. 读取温湿度和气体传感器
        current_temp = Thermal_GetTemp();
        current_ppm = MQ2_GetPPM();

        // 2. 综合报警仲裁
        uint8_t need_alarm = 0;         
        int target_motor = 0;           
        float target_servo = 0.0f;      

        if (current_temp >= temp_limit_2) {
            need_alarm = 1; target_motor = 10; target_servo = 90.0f;
        } else if (current_temp >= temp_limit_1) {
            need_alarm = 1; target_motor = 10;
        }
        if (current_humi >= humi_limit) {
            need_alarm = 1; target_motor = 10;
        }
        if (current_ppm >= gas_limit) {
            need_alarm = 1; target_motor = 10; target_servo = 90.0f;
        }

        // 3. 统一执行慢速外设控制 (仅在 AUTO 模式下)
        if (sys_mode == 0) {
            Motor_SetSpeed(target_motor);
            Servo_SetAngle(target_servo);
        } 

        // 4. 报警控制
        if (need_alarm) {
            Buzzer_Sound(50); 
        }
    }

    // =========================================================================
    // 4. DHT11 慢速读取区 (每 20 次执行，约 1000ms)
    // =========================================================================
    if (loop_tick % 20 == 0) {
        uint8_t dht_t = 0;
        DHT11_Read_Data(&dht_t, &current_humi);
    }
}

/* ================== 读取存储设置的函数 ================== */
void Load_Settings_From_Flash(void) {
    Store_Init(); 
    if (Store_Data[1] == 0 && Store_Data[2] == 0) {
        Store_Data[1] = 30;     
        Store_Data[2] = 70;     
        Store_Data[3] = 1000;   
        Store_Data[4] = 0;      
        Store_Data[5] = 0;      
        Store_Data[6] = 0;      
        Store_Data[7] = 100;    
        Store_Data[8] = 35;     
        Store_Data[9] = 50;     // 默认光照目标恒定为 50%
        Store_Save();
    }
    
    temp_limit_1 = Store_Data[1];
    humi_limit   = Store_Data[2];
    gas_limit    = Store_Data[3];
    sys_mode     = Store_Data[4];
    temp_limit_2 = Store_Data[8];
    target_light = Store_Data[9]; 
    
    LED_SetBrightness((uint8_t)Store_Data[5]);
    if(Store_Data[6]) Servo_SetAngle(90.0f); else Servo_SetAngle(0.0f);
    Motor_SetSpeed((int)Store_Data[7] - 100); 
}

/* ================== 菜单执行函数定义 ================== */
void Show_InformationFunc(void){
    OLED_Clear(); 
    u8 current_page = 0; 
    u8 refresh_flag = 1; 

    while(1){
        Run_Background_Task(); 

        if (current_page == 0) {
            if (refresh_flag) { OLED_Clear(); refresh_flag = 0; }
            int temp_int = (int)current_temp;                            
            int temp_frac = (int)((current_temp - temp_int) * 100);      
            OLED_ShowString(1, 1, "Temp: ");
            if(current_temp < 0) { OLED_ShowChar(1, 7, '-'); temp_int = -temp_int; temp_frac = -temp_frac; } else { OLED_ShowChar(1, 7, '+'); }
            OLED_ShowNum(1, 8, temp_int, 2); OLED_ShowChar(1, 10, '.'); OLED_ShowNum(1, 11, temp_frac, 2);     
            OLED_ShowString(2, 1, "Humi: "); OLED_ShowNum(2, 7, current_humi, 2); OLED_ShowString(2, 9, " %  "); 
            OLED_ShowString(3, 1, "Light:"); OLED_ShowNum(3, 8, current_light, 3); OLED_ShowString(3, 11, "%  ");
            OLED_ShowString(4, 1, "Gas:  "); OLED_ShowNum(4, 7, (int)current_ppm, 4); OLED_ShowString(4, 11, " PPM");
        } else {
            if (refresh_flag) { OLED_Clear(); refresh_flag = 0; }
            OLED_ShowString(1, 1, "Led_PWM: "); OLED_ShowNum(1, 10, LED_GetBrightness(), 3); OLED_ShowString(1, 13, "   ");
            OLED_ShowString(2, 1, "Servo: ");
            if (Servo_GetState()) OLED_ShowString(2, 8, "ON      "); else OLED_ShowString(2, 8, "OFF     ");
            int motor_speed = Motor_GetSpeed(); 
            OLED_ShowString(3, 1, "Motor: ");
            if(motor_speed < 0) { OLED_ShowChar(3, 8, '-'); motor_speed = -motor_speed; } else { OLED_ShowChar(3, 8, '+'); }
            OLED_ShowNum(3, 9, motor_speed, 3); OLED_ShowString(3, 12, "   "); 
            OLED_ShowString(4, 1, "    <Page 2>    ");
        }

        uint8_t key = Key_GetNum();
        if (key == 1) { OLED_Clear(); break; } 
        else if (key == 2 || key == 4) { current_page = !current_page; refresh_flag = 1; }
        Delay_ms(100);
    }
}

/* ================== 设置函数 ================== */

// 新增：光照目标设置UI
void Set_Target_Light_Func(void) {
    OLED_Clear();
    while(1) {
        Run_Background_Task(); 
        OLED_ShowString(1, 1, "Set Target Light");
        OLED_ShowNum(2, 1, target_light, 3);
        OLED_ShowString(2, 4, "%  ");
        
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { Store_Data[9] = target_light; Store_Save(); OLED_Clear(); break; }
        if (key == 2 && target_light < 100) target_light++;
        if (key == 4 && target_light > 0)   target_light--;
        Delay_ms(50);
    }
}

void Set_Temp_Limit_1_Func(void) {
    OLED_Clear();
    while(1) {
        Run_Background_Task();
        OLED_ShowString(1, 1, "Set Temp Thres1:");
        OLED_ShowNum(2, 1, temp_limit_1, 3);
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { Store_Data[1] = temp_limit_1; Store_Save(); OLED_Clear(); break; } 
        if (key == 2) temp_limit_1++;                        
        if (key == 4) temp_limit_1--;                        
        Delay_ms(50);
    }
}

void Set_Temp_Limit_2_Func(void) {
    OLED_Clear();
    while(1) {
        Run_Background_Task(); 
        OLED_ShowString(1, 1, "Set Temp Thres2:");
        OLED_ShowNum(2, 1, temp_limit_2, 3);
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { Store_Data[8] = temp_limit_2; Store_Save(); OLED_Clear(); break; } 
        if (key == 2) temp_limit_2++;                        
        if (key == 4) temp_limit_2--;                        
        Delay_ms(50);
    }
}

void Set_Humi_Limit_Func(void) {
    OLED_Clear();
    while(1) {
        Run_Background_Task(); 
        OLED_ShowString(1, 1, "Set Humi Thres:");
        OLED_ShowNum(2, 1, humi_limit, 3);
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { Store_Data[2] = humi_limit; Store_Save(); OLED_Clear(); break; }
        if (key == 2 && humi_limit < 100) humi_limit++;
        if (key == 4 && humi_limit > 0) humi_limit--;
        Delay_ms(50);
    }
}

void Set_Gas_Limit_Func(void) {
    OLED_Clear();
    while(1) {
        Run_Background_Task(); 
        OLED_ShowString(1, 1, "Set Gas Thres:");
        OLED_ShowNum(2, 1, gas_limit, 4);
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { Store_Data[3] = gas_limit; Store_Save(); OLED_Clear(); break; }
        if (key == 2) gas_limit += 10;
        if (key == 4 && gas_limit >= 10) gas_limit -= 10;
        Delay_ms(50);
    }
}

/* ================== 模式与手动控制函数 ================== */
void Sys_Mode_Func(void) {
    OLED_Clear();
    while(1) {
        Run_Background_Task(); 
        OLED_ShowString(1, 1, "Mode Select:");
        if (sys_mode == 0)      OLED_ShowString(2, 1, "> AUTO    ");
        else if (sys_mode == 1) OLED_ShowString(2, 1, "> MANUAL  ");
        
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { Store_Data[4] = sys_mode; Store_Save(); OLED_Clear(); break; }
        if (key == 2 || key == 4) sys_mode = !sys_mode; 
        Delay_ms(50);
    }
}

void Manual_LED_Func(void) {
    if (sys_mode == 0) { OLED_Clear(); OLED_ShowString(2, 1, "MANUAL MODE ONLY"); Delay_ms(1000); OLED_Clear(); return; }
    uint8_t led_val = LED_GetBrightness();
    OLED_Clear();
    while(1) {
        Run_Background_Task(); 
        OLED_ShowString(1, 1, "Manual LED PWM:");
        OLED_ShowNum(2, 1, led_val, 3);
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { Store_Data[5] = led_val; Store_Save(); OLED_Clear(); break; }
        if (key == 2 && led_val <= 90) { led_val += 10; LED_SetBrightness(led_val); }
        if (key == 4 && led_val >= 10) { led_val -= 10; LED_SetBrightness(led_val); }
        Delay_ms(50);
    }
}

void Manual_Servo_Func(void) {
    if (sys_mode == 0) { OLED_Clear(); OLED_ShowString(2, 1, "MANUAL MODE ONLY"); Delay_ms(1000); OLED_Clear(); return; }
    uint8_t state = Servo_GetState();
    OLED_Clear();
    while(1) {
        Run_Background_Task(); 
        OLED_ShowString(1, 1, "Manual Servo:");
        if (state) OLED_ShowString(2, 1, "ON (90 deg) ");
        else       OLED_ShowString(2, 1, "OFF (0 deg) ");
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { Store_Data[6] = state; Store_Save(); OLED_Clear(); break; }
        if (key == 2 || key == 4) { 
            state = !state; 
            if(state) Servo_SetAngle(90.0f); else Servo_SetAngle(0.0f);
        }
        Delay_ms(50);
    }
}

void Manual_Motor_Func(void) {
    if (sys_mode == 0) { OLED_Clear(); OLED_ShowString(2, 1, "MANUAL MODE ONLY"); Delay_ms(1000); OLED_Clear(); return; }
    int speed = Motor_GetSpeed();
    OLED_Clear();
    while(1) {
        Run_Background_Task(); 
        OLED_ShowString(1, 1, "Manual Motor:");
        if(speed < 0) { OLED_ShowChar(2, 1, '-'); OLED_ShowNum(2, 2, -speed, 3); OLED_ShowString(2, 5, "   "); }
        else { OLED_ShowChar(2, 1, '+'); OLED_ShowNum(2, 2, speed, 3); OLED_ShowString(2, 5, "   "); }
        
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { Store_Data[7] = speed + 100; Store_Save(); OLED_Clear(); break; }
        if (key == 2 && speed <= 90) { speed += 10; Motor_SetSpeed(speed); }
        if (key == 4 && speed >= -90) { speed -= 10; Motor_SetSpeed(speed); }
        Delay_ms(50);
    }
}

/* ================== 菜单树初始化 ================== */
void MenuInit() {
    Load_Settings_From_Flash();

    rootNode = 
    SetNode(DIR_type, "Main Menu", SetBranch(
        SetNode(EXE_type, "View Info", Show_InformationFunc), 
        
        SetNode(DIR_type, "Sensor Thres", SetBranch(
            SetNode(EXE_type, "Temp Thres 1", Set_Temp_Limit_1_Func), 
            SetNode(EXE_type, "Temp Thres 2", Set_Temp_Limit_2_Func), 
            SetNode(EXE_type, "Humi Thres",   Set_Humi_Limit_Func),             
            SetNode(EXE_type, "Gas Thres",    Set_Gas_Limit_Func),
            SetNode(EXE_type, "Target Light", Set_Target_Light_Func) // 将光照目标放入设置菜单
        )),
        
        SetNode(DIR_type, "Mode Setting", SetBranch(
            SetNode(EXE_type, "Auto/Manual",  Sys_Mode_Func),
            SetNode(EXE_type, "Manual LED",   Manual_LED_Func),
            SetNode(EXE_type, "Manual Servo", Manual_Servo_Func),
            SetNode(EXE_type, "Manual Motor", Manual_Motor_Func)
        ))
    ));

    Selector = rootNode->pointer;
}

/* ================== OLED 打印接口对接 ================== */
void myOLEDPrintStringLine(int line, const char* str) {
    char buf[17]; 
    snprintf(buf, sizeof(buf), "%-16s", str); 
    OLED_ShowString(line + 1, 1, buf);
}
