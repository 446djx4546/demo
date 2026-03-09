#include "ESP8266.h"
#include "Delay.h"   // 使用你现有的延时函数
#include <string.h>
#include <stdio.h>

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
    
    // 4. 配置 USART3 参数
    USART_InitTypeDef USART_InitStructure;
    USART_InitStructure.USART_BaudRate = 115200;                                    // 波特率设置
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;                 // 收发模式
    USART_InitStructure.USART_Parity = USART_Parity_No;                             // 无校验
    USART_InitStructure.USART_StopBits = USART_StopBits_1;                          // 1位停止位
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;                     // 8位数据位
    USART_Init(USART3, &USART_InitStructure);
    
    // 5. 使能 USART3
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
