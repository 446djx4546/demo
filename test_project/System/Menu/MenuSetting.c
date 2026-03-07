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

Node_t* rootNode;

/* ================== 全局变量：阈值与模式 ================== */
int temp_limit = 35;       // 温度报警阈值
int humi_limit = 70;       // 湿度报警阈值
int gas_limit  = 1000;     // 气体报警阈值
uint8_t sys_mode = 0;      // 系统模式：0为Auto(自动)，1为Manual(手动)

/* ================== 菜单执行函数定义 ================== */

// 1. 实时显示信息的页面 (包含报警逻辑)
void Show_InformationFunc(void){
    OLED_Clear(); 
    static u8 dht_update_cnt = 0; 
    static u8 last_humi = 0;
    u8 current_page = 0; 
    u8 refresh_flag = 1; 

    while(1){
        // 读取 DHT11 (非阻塞)
        if (dht_update_cnt == 0) {
            u8 dht_temp = 0;
            DHT11_Read_Data(&dht_temp, &last_humi);
        }
        dht_update_cnt++;
        if (dht_update_cnt >= 15) dht_update_cnt = 0;

        float temp = Thermal_GetTemp();
        float ppm = MQ2_GetPPM();
        uint8_t light_percent = LightSensor_GetIntensity();

        // === 报警逻辑：如果超过阈值，蜂鸣器报警 ===
        if (temp > temp_limit || last_humi > humi_limit || ppm > gas_limit) {
            Buzzer_Sound(50); // 触发短促报警
        }

        // 分页显示
        if (current_page == 0) {
            if (refresh_flag) { OLED_Clear(); refresh_flag = 0; }
            int temp_int = (int)temp;                            
            int temp_frac = (int)((temp - temp_int) * 100);      
            OLED_ShowString(1, 1, "Temp: ");
            if(temp < 0) { OLED_ShowChar(1, 7, '-'); temp_int = -temp_int; temp_frac = -temp_frac; } else { OLED_ShowChar(1, 7, '+'); }
            OLED_ShowNum(1, 8, temp_int, 2); OLED_ShowChar(1, 10, '.'); OLED_ShowNum(1, 11, temp_frac, 2);     
            OLED_ShowString(2, 1, "Humi: "); OLED_ShowNum(2, 7, last_humi, 2); OLED_ShowString(2, 9, " %  "); 
            OLED_ShowString(3, 1, "Light:"); OLED_ShowNum(3, 8, light_percent, 3); OLED_ShowString(3, 11, "%  ");
            OLED_ShowString(4, 1, "Gas:  "); OLED_ShowNum(4, 7, (int)ppm, 4); OLED_ShowString(4, 11, " PPM");
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

        // 按键检测
        uint8_t key = Key_GetNum();
        if (key == 1) { OLED_Clear(); break; } 
        else if (key == 2 || key == 4) { current_page = !current_page; refresh_flag = 1; }
        Delay_ms(100);
    }
}

/* ================== 阈值设置函数 ================== */
void Set_Temp_Limit_Func(void) {
    OLED_Clear();
    while(1) {
        OLED_ShowString(1, 1, "Set Temp Thres:");
        OLED_ShowNum(2, 1, temp_limit, 3);
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { OLED_Clear(); break; } // 返回或确认
        if (key == 2) temp_limit++;                        // 增加
        if (key == 4) temp_limit--;                        // 减少
        Delay_ms(50);
    }
}

void Set_Humi_Limit_Func(void) {
    OLED_Clear();
    while(1) {
        OLED_ShowString(1, 1, "Set Humi Thres:");
        OLED_ShowNum(2, 1, humi_limit, 3);
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { OLED_Clear(); break; }
        if (key == 2 && humi_limit < 100) humi_limit++;
        if (key == 4 && humi_limit > 0) humi_limit--;
        Delay_ms(50);
    }
}

void Set_Gas_Limit_Func(void) {
    OLED_Clear();
    while(1) {
        OLED_ShowString(1, 1, "Set Gas Thres:");
        OLED_ShowNum(2, 1, gas_limit, 4);
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { OLED_Clear(); break; }
        if (key == 2) gas_limit += 10;
        if (key == 4 && gas_limit >= 10) gas_limit -= 10;
        Delay_ms(50);
    }
}

/* ================== 模式与手动控制函数 ================== */
void Sys_Mode_Func(void) {
    OLED_Clear();
    while(1) {
        OLED_ShowString(1, 1, "Mode Select:");
        if (sys_mode == 0)      OLED_ShowString(2, 1, "> AUTO    ");
        else if (sys_mode == 1) OLED_ShowString(2, 1, "> MANUAL  ");
        
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { OLED_Clear(); break; }
        if (key == 2 || key == 4) sys_mode = !sys_mode; // 切换模式
        Delay_ms(50);
    }
}

void Manual_LED_Func(void) {
    if (sys_mode == 0) { OLED_Clear(); OLED_ShowString(2, 1, "MANUAL MODE ONLY"); Delay_ms(1000); OLED_Clear(); return; }
    uint8_t led_val = LED_GetBrightness();
    OLED_Clear();
    while(1) {
        OLED_ShowString(1, 1, "Manual LED PWM:");
        OLED_ShowNum(2, 1, led_val, 3);
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { OLED_Clear(); break; }
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
        OLED_ShowString(1, 1, "Manual Servo:");
        if (state) OLED_ShowString(2, 1, "ON (90 deg) ");
        else       OLED_ShowString(2, 1, "OFF (0 deg) ");
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { OLED_Clear(); break; }
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
        OLED_ShowString(1, 1, "Manual Motor:");
        if(speed < 0) { OLED_ShowChar(2, 1, '-'); OLED_ShowNum(2, 2, -speed, 3); OLED_ShowString(2, 5, "   "); }
        else { OLED_ShowChar(2, 1, '+'); OLED_ShowNum(2, 2, speed, 3); OLED_ShowString(2, 5, "   "); }
        
        uint8_t key = Key_GetNum();
        if (key == 1 || key == 3) { OLED_Clear(); break; }
        if (key == 2 && speed <= 90) { speed += 10; Motor_SetSpeed(speed); }
        if (key == 4 && speed >= -90) { speed -= 10; Motor_SetSpeed(speed); }
        Delay_ms(50);
    }
}

/* ================== 菜单树初始化 ================== */
void MenuInit() {
    rootNode = 
    SetNode(DIR_type, "Main Menu", SetBranch(
        SetNode(EXE_type, "View Info", Show_InformationFunc), 
        
        // 传感器阈值设置
        SetNode(DIR_type, "Sensor Thres", SetBranch(
            SetNode(EXE_type, "Temp Thres", Set_Temp_Limit_Func), 
            SetNode(EXE_type, "Humi Thres", Set_Humi_Limit_Func),             
            SetNode(EXE_type, "Gas Thres",  Set_Gas_Limit_Func)             
        )),
        
        // 自动/手动模式设置
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
