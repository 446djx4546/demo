#include "ESP8266.h"
#include "Delay.h"   // 使用你现有的延时函数
#include <string.h>
#include <stdio.h>

#include "Store.h"
#include "led_pwm.h"
#include "servo.h"
#include "motor.h"

extern int temp_limit_1;
extern int temp_limit_2;
extern int gas_limit;
extern int humi_limit;
extern int target_light;
extern uint8_t sys_mode;
extern uint16_t Store_Data[10];

char ESP8266_RX_BUF[128];
uint16_t ESP8266_RX_STA = 0;

/**
  * @brief  USART3 初始化 (PB10=TX, PB11=RX)
  * 波特率: 115200 (ESP8266默认)
  */
void USART3_Init(void)
{
    // 1. 开启时钟：GPIOB (APB2) 和 USART3 (APB1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    
    // 2. 配置 GPIOB_Pin_10 (TX) -> 复用推挽输出
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 3. 配置 GPIOB_Pin_11 (RX) -> 浮空输入或上拉输入
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 4. 配置 USART3 参数和中断
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 115200;                                    // 波特率设置
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;                 // 收发模式
    USART_InitStructure.USART_Parity = USART_Parity_No;                             // 无校验
    USART_InitStructure.USART_StopBits = USART_StopBits_1;                          // 1位停止位
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;                     // 8位数据位
    USART_Init(USART3, &USART_InitStructure);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;        // 子优先级
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    USART_Cmd(USART3, ENABLE);
}

/**
  * @brief  USART3 发送一个字节
  */
void USART3_SendByte(uint8_t Byte)
{
    USART_SendData(USART3, Byte);
    // 等待发送数据寄存器为空
    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
}

/**
  * @brief  USART3 发送字符串
  */
void USART3_SendString(char *String)
{
    uint16_t i = 0;
    while (String[i] != '\0')
    {
        USART3_SendByte(String[i]);
        i++;
    }
}

/**
  * @brief  初始化 ESP8266 开启 TCP 服务器
  */
void ESP8266_Init(void)
{
    USART3_Init();
    
    // 给 ESP8266 一点时间完成上电启动
    Delay_ms(1500); 
    
    // 1. 开启多连接模式
    USART3_SendString("AT+CIPMUX=1\r\n");
    Delay_ms(500); // 盲等模块回复 OK
    
    // 2. 开启 TCP 服务器，端口号 8080
    USART3_SendString("AT+CIPSERVER=1,8080\r\n");
    Delay_ms(500); // 盲等模块回复 OK
}

/**
  * @brief  向所有连接到 ESP8266 (id=0) 的客户端发送数据
  * @param  Data: 要发送的字符串数据
  */
void ESP8266_SendData(char *Data)
{
    char cmd[32];
    uint16_t len = strlen(Data);
    
    // 1. 发送准备接收数据的指令 (向 0 号连接发送 len 长度的数据)
    sprintf(cmd, "AT+CIPSEND=0,%d\r\n", len);
    USART3_SendString(cmd);
    
    // 2. 等待 ESP8266 响应 ">" 符号 (这里简单处理，固定延时 50ms 替代复杂的接收判断)
    Delay_ms(50); 
    
    // 3. 发送实际数据
    USART3_SendString(Data);
    
    // 发送完稍微延时，防止数据粘连
    Delay_ms(50);
}


void ESP8266_ParseCommand(void)
{
    // 1. 切换自动模式
    if (strstr(ESP8266_RX_BUF, "MODE:AUTO")) {
        sys_mode = 0;
        Store_Data[4] = sys_mode; Store_Save();
    }
    // 2. 切换手动模式
    else if (strstr(ESP8266_RX_BUF, "MODE:MANUAL")) {
        sys_mode = 1;
        Store_Data[4] = sys_mode; Store_Save();
    }
    else if (strstr(ESP8266_RX_BUF, "GET_SYNC")) {
        char syncBuffer[80];
        // 将单片机内的真实数据打包 (注意 Motor 速度需要 -100 还原真实值)
        // 格式: SYNC:t1,t2,h,m,l,mode,led,servo,motor
        sprintf(syncBuffer, "SYNC:%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
                temp_limit_1, temp_limit_2, humi_limit, gas_limit, target_light,
                sys_mode, Store_Data[5], Store_Data[6], (int)Store_Data[7] - 100);
        
        ESP8266_SendData(syncBuffer);
    }
    // 3. 解析阈值设置 (格式: THRES:Temp,MQ2,Humi,Light)
    else {
        char *cmd_thres = strstr(ESP8266_RX_BUF, "THRES:");
        if (cmd_thres) {
           int t1, t2, h, m, l;
            if (sscanf(cmd_thres + 6, "%d,%d,%d,%d,%d", &t1, &t2, &h, &m, &l) == 5) {
                temp_limit_1 = (int)t1;
                temp_limit_2 = (int)t2;
                humi_limit = h;
                gas_limit = (int)m;
                target_light = l;
                
                // 实时保存到 Flash，断电不丢失
                Store_Data[1] = temp_limit_1;
                Store_Data[8] = temp_limit_2;
                Store_Data[2] = humi_limit;
                Store_Data[3] = gas_limit;
                Store_Data[9] = target_light;
                Store_Save();
            }
        }

        // 4. 解析手动控制 (格式: CTRL:LED,Servo,Motor)
        char *cmd_ctrl = strstr(ESP8266_RX_BUF, "CTRL:");
        if (cmd_ctrl && sys_mode == 1) { // 安全互锁：仅手动模式下允许控制
            int led, servo, motor;
            if (sscanf(cmd_ctrl + 5, "%d,%d,%d", &led, &servo, &motor) == 3) {
                // 直接驱动底层硬件
                LED_SetBrightness((uint8_t)led);
                Servo_SetAngle((float)servo);
                Motor_SetSpeed(motor);

                // 记录状态到 Flash 以便屏幕同步
                Store_Data[5] = led;
                Store_Data[6] = (servo > 45) ? 1 : 0;
                Store_Data[7] = motor + 100; // 抵消负数逻辑
                Store_Save();
            }
        }
    }

    // 执行完毕后，清空缓冲区迎接下一次数据
    memset(ESP8266_RX_BUF, 0, sizeof(ESP8266_RX_BUF));
    ESP8266_RX_STA = 0;
}