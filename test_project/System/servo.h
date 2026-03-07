#ifndef __SERVO_H
#define	__SERVO_H
#include "stm32f10x.h"
#include "delay.h"
#include <stdint.h>

#define	SERVO_CLK					RCC_APB2Periph_GPIOA
#define	SERVO_GPIO_PORT				GPIOA
#define	SERVO_GPIO_PIN				GPIO_Pin_6

void SERVO_Init(void);
void PWM_SetCompare1(uint16_t Compare); // 改为 Channel 1
void Servo_SetAngle(float Angle);
// 声明获取舵机状态的函数
uint8_t Servo_GetState(void);

#endif
