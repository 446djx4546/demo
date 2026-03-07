#include "motor.h"

static int current_motor_speed = 0;

// ==========================================
// 初始化电机控制引脚及定时器 (TIM3)
// ==========================================
void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;

    // 1. 开启 GPIOA, GPIOB 和 TIM3 的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    // 2. 配置 PA7 (TIM3_CH2) 为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 PB0 (TIM3_CH3) 为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 4. 配置定时器 TIM3 (系统时钟 72MHz)
    // 预分频器 (Prescaler) 设为 72-1，使定时器时钟为 1MHz
    // 重装载值 (Period) 设为 100-1，使 PWM 周期为 100us (即频率 10kHz)
    TIM_TimeBaseStructure.TIM_Period = 100 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    // 5. 配置 TIM3 的通道 2 和通道 3 为 PWM 模式 1
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    // 初始化通道 2 (PA7)
    TIM_OC2Init(TIM3, &TIM_OCInitStructure); 
    TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);

    // 初始化通道 3 (PB0)
    TIM_OC3Init(TIM3, &TIM_OCInitStructure); 
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);

    // 6. 使能 TIM3
    TIM_Cmd(TIM3, ENABLE);
}

// ==========================================
// 设置电机运行状态和速度
// 参数 speed: 范围 -100 到 100
// (正数表示正转，负数表示反转，0表示停止)
// ==========================================
void Motor_SetSpeed(int speed)
{
    // 在限幅之前或之后保存都可以，最好保存限幅后的真实转速
    if (speed > 100) speed = 100;    
    if (speed < -100) speed = -100;  
    
    current_motor_speed = speed; // 保存当前速度

    if (speed > 0)
    {
        TIM_SetCompare2(TIM3, speed);    
        TIM_SetCompare3(TIM3, 0);        
    }
    else if (speed < 0)
    {
        TIM_SetCompare2(TIM3, 0);        
        TIM_SetCompare3(TIM3, -speed);   
    }
    else
    {
        TIM_SetCompare2(TIM3, 0);
        TIM_SetCompare3(TIM3, 0);
    }
}

int Motor_GetSpeed(void)
{
    return current_motor_speed;
}

