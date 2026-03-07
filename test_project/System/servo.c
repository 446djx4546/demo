#include "servo.h"

void SERVO_Init(void)
{
	// 1. 开启 TIM3 时钟 (PA6 对应 TIM3)
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB2PeriphClockCmd(SERVO_CLK, ENABLE);

	// 2. 配置 GPIO PA6
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = SERVO_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SERVO_GPIO_PORT, &GPIO_InitStructure);

	// 3. 选择 TIM3 内部时钟
	TIM_InternalClockConfig(TIM3);

	// 4. 配置时基单元 (50Hz) - 你的参数非常标准，保持不变
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 20000 - 1;
	TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	// 5. 配置输出比较: PA6 必须使用 通道 1 (Channel 1)
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OC1Init(TIM3, &TIM_OCInitStructure); // 修改为 OC1Init

	// 6. 使能定时器 TIM3
	TIM_Cmd(TIM3, ENABLE);
}

// 修改为操作 TIM3 的 Channel 1
void PWM_SetCompare1(uint16_t Compare)
{
	TIM_SetCompare1(TIM3, Compare);
}

void Servo_SetAngle(float Angle)
{
	// 加上 .0 保证浮点运算更严谨，避免 C 语言潜在的隐式整数除法截断问题
	PWM_SetCompare1((uint16_t)(Angle / 180.0 * 2000.0 + 500.0));
}
