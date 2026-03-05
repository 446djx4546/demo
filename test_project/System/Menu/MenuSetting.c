#include "MenuSetting.h"
#include "OLED.h"       
#include "Buzzer.h"     
#include "Thermal.h"    
#include "LightSensor.h"
#include "DHT11.h"      // 引入你新的 DHT11 头文件
#include "Key.h"
#include "Delay.h"
#include <stdio.h>

Node_t* rootNode;

/* ================== 菜单执行函数定义 ================== */

// 测试蜂鸣器的函数
void Buzzer_TestFunc(void){
    Buzzer_Sound(100); 
}

// 实时显示传感器数据的页面 (按 SW1 返回)
void Show_SensorFunc(void){
    OLED_Clear(); // 进入页面前清屏
    
    // 静态变量，用于控制 DHT11 的读取频率
    static u8 dht_update_cnt = 0; 
    static u8 last_humi = 0;

    while(1){
        // === 1. 非阻塞式读取 DHT11 (每 15*100ms = 1.5s 读一次) ===
        if (dht_update_cnt == 0) {
            u8 dht_temp = 0;
            // 调用你新驱动的读取函数，将湿度值存入 last_humi
            DHT11_Read_Data(&dht_temp, &last_humi);
        }
        dht_update_cnt++;
        if (dht_update_cnt >= 15) dht_update_cnt = 0;

        // === 2. 获取并显示 NTC 温度 (第 1 行) ===
        float temp = Thermal_GetTemp();
        int temp_int = (int)temp;                            
        int temp_frac = (int)((temp - temp_int) * 100);      
        OLED_ShowString(1, 1, "Temp: ");
        if(temp < 0) { OLED_ShowChar(1, 7, '-'); temp_int = -temp_int; temp_frac = -temp_frac; } 
        else { OLED_ShowChar(1, 7, '+'); }
        OLED_ShowNum(1, 8, temp_int, 2);       
        OLED_ShowChar(1, 10, '.');
        OLED_ShowNum(1, 11, temp_frac, 2);     

        // === 3. 显示湿度 (第 2 行) ===
        OLED_ShowString(2, 1, "Humi: ");
        OLED_ShowNum(2, 7, last_humi, 2);
        OLED_ShowString(2, 9, " %  "); 

        // === 4. 获取并显示光照 (第 3 行) ===
        uint8_t light_percent = LightSensor_GetIntensity();
        OLED_ShowString(3, 1, "Light: ");
        OLED_ShowNum(3, 8, light_percent, 3);
        OLED_ShowString(3, 11, "%  ");

        // === 5. 检测按键退出 ===
        if (Key_GetNum() == 1) { // 按 SW1 退出
            OLED_Clear(); 
            break;
        }
        
        // 100ms 刷新率，保证按键和界面的丝滑响应
        Delay_ms(100);
    }
}

/* ================== 菜单树初始化 ================== */
void MenuInit() {
    // 根据你的硬件，重新设计了一个菜单树
    rootNode = 
    SetNode(DIR_type, "Main Menu", SetBranch(
        SetNode(EXE_type, "View Sensors", Show_SensorFunc), // 进入传感器页面
        SetNode(DIR_type, "Settings", SetBranch(
            SetNode(EXE_type, "Buzzer Test", Buzzer_TestFunc), // 测试蜂鸣器
            SetNode(EXE_type, "Temp Limit", NULL),             // 预留功能
            SetNode(EXE_type, "Light Limit", NULL)             // 预留功能
        )),
        SetNode(EXE_type, "About", NULL)
    ));

    Selector = rootNode->pointer;
}

/* ================== OLED 打印接口对接 ================== */
void myOLEDPrintStringLine(int line, const char* str) {
    // 1.3寸 OLED 一行通常能显示 16 个字符。我们利用 %-16s 左对齐并用空格补齐，避免清屏闪烁
    char buf[17]; 
    snprintf(buf, sizeof(buf), "%-16s", str); 
    
    OLED_ShowString(line + 1, 1, buf);
}
