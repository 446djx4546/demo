#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"

// 串口3初始化及基础发送函数
void USART3_Init(void);
void USART3_SendByte(uint8_t Byte);
void USART3_SendString(char *String);

// ESP8266 核心业务函数
void ESP8266_Init(void);
void ESP8266_SendData(char *Data);

#endif
