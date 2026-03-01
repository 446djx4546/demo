#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"
#include "Delay.h" // 需要用到延时函数

void Buzzer_Init(void);
void Buzzer_Sound(uint16_t ms); // 让蜂鸣器以固定频率响指定的时间

#endif
