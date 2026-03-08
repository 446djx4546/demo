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

Node_t* rootNode;

/* ================== 全局变量：阈值与模式 ================== */
int temp_limit_1 = 30;     
int temp_limit_2 = 35;     
int humi_limit = 70;       
int gas_limit  = 1000;     
uint8_t sys_mode = 0;      

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
        Store_Save();
    }
    
    temp_limit_1 = Store_Data[1];
    humi_limit   = Store_Data[2];
    gas_limit    = Store_Data[3];
    sys_mode     = Store_Data[4];
    temp_limit_2 = Store_Data[8];
    
    LED_SetBrightness((uint8_t)Store_Data[5]);
    if(Store_Data[6]) Servo_SetAngle(90.0f); else Servo_SetAngle(0.0f);
    Motor_SetSpeed((int)Store_Data[7] - 100); 
}

/* ================== 菜单执行函数定义 ================== */

void Show_InformationFunc(void){
    OLED_Clear(); 
    static u8 dht_update_cnt = 0; 
    static u8 last_humi = 0;
    u8 current_page = 0; 
    u8 refresh_flag = 1; 

    while(1){
        if (dht_update_cnt == 0) {
            u8 dht_temp = 0;
            DHT11_Read_Data(&dht_temp, &last_humi);
        }
        dht_update_cnt++;
        if (dht_update_cnt >= 15) dht_update_cnt = 0;

        float temp = Thermal_GetTemp();
        float ppm = MQ2_GetPPM();
        uint8_t light_percent = LightSensor_GetIntensity();

        // ================== 综合报警与状态仲裁逻辑 ==================
        uint8_t need_alarm = 0;         // 默认不报警
        int target_motor = 0;           // 默认关闭风扇
        float target_servo = 0.0f;      // 默认关闭舵机

        // 1. 温度判定
        if (temp >= temp_limit_2) {
            need_alarm = 1;
            target_motor = 10;   // 满足条件，标记风扇开启
            target_servo = 90.0f; // 满足条件，标记舵机开启
        } 
        else if (temp >= temp_limit_1) {
            need_alarm = 1;
            target_motor = 10;   // 满足条件，标记风扇开启
        }

        // 2. 湿度判定
        if (last_humi >= humi_limit) {
            need_alarm = 1;
            target_motor = 10;   // 湿度超标，标记风扇开启
        }

        // 3. 气体判定
        if (ppm >= gas_limit) {
            need_alarm = 1;
            target_motor = 10;   // 气体超标，排气风扇开启
            target_servo = 90.0f; // 气体超标，开窗(舵机)开启
        }

        // 统一执行外设控制 (仅在 AUTO 模式下进行覆盖)
        if (sys_mode == 0) {
            Motor_SetSpeed(target_motor);
            Servo_SetAngle(target_servo);
        }

        // 统一执行蜂鸣器报警
        if (need_alarm) {
            Buzzer_Sound(50); 
        }
        // ========================================================

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

        uint8_t key = Key_GetNum();
        if (key == 1) { OLED_Clear(); break; } 
        else if (key == 2 || key == 4) { current_page = !current_page; refresh_flag = 1; }
        Delay_ms(100);
    }
}

/* ================== 阈值设置函数 ================== */
void Set_Temp_Limit_1_Func(void) {
    OLED_Clear();
    while(1) {
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
            SetNode(EXE_type, "Gas Thres",    Set_Gas_Limit_Func)             
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
