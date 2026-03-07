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

// ====== 增加你外设模块的头文件 ======
#include "led_pwm.h"
#include "servo.h"
#include "motor.h"

Node_t* rootNode;

/* ================== 菜单执行函数定义 ================== */

// 测试蜂鸣器的函数
void Buzzer_TestFunc(void){
    Buzzer_Sound(100); 
}

// 实时显示信息的页面 (按 SW1 返回，SW2/SW4 翻页)
void Show_InformationFunc(void){
    OLED_Clear(); // 进入页面前清屏
    
    // 静态变量，用于控制 DHT11 的读取频率
    static u8 dht_update_cnt = 0; 
    static u8 last_humi = 0;
    
    u8 current_page = 0; // 0: 第1页(温湿度、光照、气体), 1: 第2页(LED、舵机、电机)
    u8 refresh_flag = 1; // 翻页全屏刷新标记，防止清屏导致的闪屏

    while(1){
        // === 1. 非阻塞式读取 DHT11 (每 15*100ms = 1.5s 读一次) ===
        if (dht_update_cnt == 0) {
            u8 dht_temp = 0;
            DHT11_Read_Data(&dht_temp, &last_humi);
        }
        dht_update_cnt++;
        if (dht_update_cnt >= 15) dht_update_cnt = 0;

        // === 2. 分页显示逻辑 ===
        if (current_page == 0) {
            if (refresh_flag) { OLED_Clear(); refresh_flag = 0; }
            
            // --- 第1页：传感器数据 ---
            float temp = Thermal_GetTemp();
            int temp_int = (int)temp;                            
            int temp_frac = (int)((temp - temp_int) * 100);      
            OLED_ShowString(1, 1, "Temp: ");
            if(temp < 0) { OLED_ShowChar(1, 7, '-'); temp_int = -temp_int; temp_frac = -temp_frac; } 
            else { OLED_ShowChar(1, 7, '+'); }
            OLED_ShowNum(1, 8, temp_int, 2);       
            OLED_ShowChar(1, 10, '.');
            OLED_ShowNum(1, 11, temp_frac, 2);     

            OLED_ShowString(2, 1, "Humi: ");
            OLED_ShowNum(2, 7, last_humi, 2);
            OLED_ShowString(2, 9, " %  "); 

            uint8_t light_percent = LightSensor_GetIntensity();
            OLED_ShowString(3, 1, "Light: ");
            OLED_ShowNum(3, 8, light_percent, 3);
            OLED_ShowString(3, 11, "%  ");

            float ppm = MQ2_GetPPM();
            OLED_ShowString(4, 1, "Gas:  ");
            OLED_ShowNum(4, 7, (int)ppm, 4); 
            OLED_ShowString(4, 11, " PPM");
            
        } else {
            if (refresh_flag) { OLED_Clear(); refresh_flag = 0; }
            
            // --- 第2页：外设状态 ---
            
            // LED 亮度
            uint8_t led_val = LED_GetBrightness(); 
            OLED_ShowString(1, 1, "Led_PWM: ");
            OLED_ShowNum(1, 10, led_val, 3);
            OLED_ShowString(1, 13, "   "); // 清除尾部可能残留的字符

            // 舵机开关状态
            uint8_t servo_state = Servo_GetState(); 
            OLED_ShowString(2, 1, "Servo: ");
            if (servo_state) OLED_ShowString(2, 8, "ON      ");
            else             OLED_ShowString(2, 8, "OFF     ");

            // 电机转速
            int motor_speed = Motor_GetSpeed(); 
            OLED_ShowString(3, 1, "Motor: ");
            if(motor_speed < 0) { OLED_ShowChar(3, 8, '-'); motor_speed = -motor_speed; }
            else { OLED_ShowChar(3, 8, '+'); }
            OLED_ShowNum(3, 9, motor_speed, 3);
            OLED_ShowString(3, 12, "   "); 
            
        }

        // === 3. 检测按键控制 ===
        uint8_t key = Key_GetNum();
        if (key == 1) { // 按 SW1 退出
            OLED_Clear(); 
            break;
        } else if (key == 2 || key == 4) { // 按 SW2 或 SW4 切换页面
            current_page = !current_page;
            refresh_flag = 1; // 触发翻页清屏
        }
        
        // 100ms 刷新率
        Delay_ms(100);
    }
}

/* ================== 菜单树初始化 ================== */
void MenuInit() {
    // 将原先的 Show_SensorFunc 替换为 Show_InformationFunc，文本改为 View Information
    rootNode = 
    SetNode(DIR_type, "Main Menu", SetBranch(
        SetNode(EXE_type, "View Info", Show_InformationFunc), 
        SetNode(DIR_type, "Settings", SetBranch(
            SetNode(EXE_type, "Buzzer Test", Buzzer_TestFunc), 
            SetNode(EXE_type, "Temp Limit", NULL),             
            SetNode(EXE_type, "Light Limit", NULL)             
        )),
        SetNode(EXE_type, "About", NULL)
    ));

    Selector = rootNode->pointer;
}

/* ================== OLED 打印接口对接 ================== */
void myOLEDPrintStringLine(int line, const char* str) {
    char buf[17]; 
    snprintf(buf, sizeof(buf), "%-16s", str); 
    OLED_ShowString(line + 1, 1, buf);
}
