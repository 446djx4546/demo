#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

// 声明初始化和亮度设置函数
void Motor_Init(void);
void Motor_SetSpeed(int speed);
// 声明获取电机转速的函数
int Motor_GetSpeed(void);


#endif
