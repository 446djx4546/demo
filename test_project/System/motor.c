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
    // 【关键修改】：为了兼容舵机(PA6)，必须使用和舵机完全一样的 50Hz 频率！
    TIM_TimeBaseStructure.TIM_Period = 20000 - 1;      // 周期改为 20000
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;      // 分频保持 72
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    // 5. 配置 TIM3 的通道 2 和通道 3 为 PWM 模式 1
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_Pulse = 0; // 初始占空比为0

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
// ==========================================
void Motor_SetSpeed(int speed)
{
    if (speed > 100) speed = 100;    
    if (speed < -100) speed = -100;  
    
    current_motor_speed = speed; 

    // 【关键修改】：将 0~100 的速度值，按比例放大到 0~20000 的占空比范围
    int pwm_value = speed * 200; 

    if (speed > 0)
    {
        TIM_SetCompare2(TIM3, pwm_value);    
        TIM_SetCompare3(TIM3, 0);        
    }
    else if (speed < 0)
    {
        TIM_SetCompare2(TIM3, 0);        
        TIM_SetCompare3(TIM3, -pwm_value);   
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
