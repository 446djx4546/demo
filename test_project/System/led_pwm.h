#ifndef __LED_PWM_H
#define __LED_PWM_H

#include "stm32f10x.h"

// 声明初始化和亮度设置函数
void LED_PWM_Init(void);
void LED_SetBrightness(uint8_t brightness);

#endif
