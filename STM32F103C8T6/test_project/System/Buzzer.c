#include "stm32f10x.h"
#include "Buzzer.h"

// 定义蜂鸣器连接的引脚
#define BUZZER_PORT     GPIOB
#define BUZZER_PIN      GPIO_Pin_12
#define BUZZER_CLK      RCC_APB2Periph_GPIOB

/**
  * @brief  蜂鸣器初始化
  */
void Buzzer_Init(void)
{
    RCC_APB2PeriphClockCmd(BUZZER_CLK, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);

    // 【关键修改 1】：默认输出高电平。
    // 因为你的蜂鸣器是低电平触发，所以高电平代表关闭状态
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN); 
}

/**
  * @brief  蜂鸣器发声函数
  * @param  ms: 发声持续的时间（毫秒）
  */
void Buzzer_Sound(uint16_t ms)
{
    // 【关键修改 2】：不需要用 for 循环产生方波了，直接拉低电平即可
    GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);   // 输出低电平，蜂鸣器开始响
    
    Delay_ms(ms);                              // 保持响的状态 ms 毫秒
    
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);     // 输出高电平，蜂鸣器关闭
}
