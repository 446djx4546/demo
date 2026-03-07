#include "led_pwm.h"

/**
 * @brief  初始化 LED PWM (引脚 PA8, 定时器 TIM1_CH1)
 * @note   基于 72MHz 系统主频，PWM 频率设置为 1kHz
 */
void LED_PWM_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 1. 开启时钟：GPIOA 和 TIM1 均挂载在 APB2 总线上
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1, ENABLE);

    // 2. 配置 GPIOA8 为复用推挽输出 (Alternate Function Push-Pull)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 TIM1 基础定时器参数
    // 时钟频率 = 72MHz / (71 + 1) = 1MHz。计 1000 个数正好是 1ms，即 1kHz 频率。
    TIM_TimeBaseStructure.TIM_Period = 1000 - 1;      // 自动重装载值 (ARR)
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;     // 预分频值 (PSC)
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;      // 时钟分割，这里不使用
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 向上计数模式
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    // 4. 配置 TIM1 通道 1 为 PWM 模式 1
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;             // PWM 模式 1 (计数值<比较值时输出有效电平)
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; // 开启比较输出
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable; // 互补输出关闭 (此处用不到)
    TIM_OCInitStructure.TIM_Pulse = 0;                            // 初始占空比设为 0，即上电默认熄灭
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;     // 输出极性：高电平有效 (与你的模块逻辑一致)
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);

    // 使能 TIM1_CH1 预装载寄存器
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    
    // 使能 TIM1 自动重装载寄存器 (ARR) 的预装载
    TIM_ARRPreloadConfig(TIM1, ENABLE);

    // 5. 开启 TIM1
    TIM_Cmd(TIM1, ENABLE);

    // 6. 开启 TIM1 的主输出 (高级定时器专属，千万别漏掉这一句！)
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

/**
 * @brief  设置 LED 亮度
 * @param  brightness 亮度百分比，范围: 0 ~ 100
 */
void LED_SetBrightness(uint8_t brightness)
{
    // 限制输入范围，防止溢出
    if (brightness > 100) {
        brightness = 100;
    }
    
    // 将 0~100 的百分比线性映射到 0~1000 的比较值(CCR1)中
    uint16_t compare_value = (uint16_t)(brightness * 10); 
    
    // 动态修改 TIM1 通道 1 的比较寄存器值，从而改变占空比
    TIM_SetCompare1(TIM1, compare_value);
}
