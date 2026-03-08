#include "stm32f10x.h"
#include "MQ2.h"
#include "Delay.h"
#include <math.h>

/**
  * @brief  MQ-2 传感器初始化 (对应引脚 PA1)
  */
void MQ2_Init(void)
{
    // 1. 开启 GPIOA 时钟 (ADC1 时钟我们在 LightSensor_Init 中已经初始化并校准过了)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 2. 配置 PA1 为模拟输入模式
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1; // 【PA1】
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // 注意：因为你的 LightSensor_Init 已经完整初始化了 ADC1 并做了校准，
    // 所以这里不需要再次调用 ADC_Init() 和校准过程。
}

/**
  * @brief  获取 MQ-2 传感器 ADC 原始值
  * @retval ADC 采样值 (0~4095)
  */
uint16_t MQ2_GetADCValue(void)
{
    // 1. 将 ADC1 规则组通道切换为 Channel_1 (PA1)，采用最长采样周期
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_239Cycles5);
    
    // 2. 第一次转换：假读 (Dummy Read)，消除刚从 PA2 切换到 PA1 时的电压串扰
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    ADC_GetConversionValue(ADC1); // 丢弃第一次的结果
    
    // 3. 第二次转换：获取干净真实的数据
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    return ADC_GetConversionValue(ADC1);
}

/**
  * @brief  获取 MQ-2 气体浓度 PPM 值 (抗溢出加固版)
  * @retval 气体浓度 PPM
  */
float MQ2_GetPPM(void)
{
    uint32_t tempData = 0;
    uint8_t read_times = 10;
    
    // 循环采样 10 次求平均，使数据更平滑
    for (uint8_t i = 0; i < read_times; i++)
    {
        tempData += MQ2_GetADCValue();
        Delay_ms(5);
    }
    float avgADC = (float)tempData / read_times;
    
    // 1. 计算 PA1 引脚处的实际电压 (STM32 ADC参考电压为3.3V)
    float V_PA1 = (avgADC * 3.3f) / 4095.0f;
    
    // 2. 还原传感器引脚的真实输出电压 (10k/10k 分压，乘以 2)
    float Vol = V_PA1 * 2.0f;
    
    // 【关键修复：硬件误差限幅保护】
    // 防止电压过低除以0，或者电压过高(>5V)导致计算出负数电阻
    if(Vol <= 0.01f) Vol = 0.01f; 
    if(Vol >= 4.99f) Vol = 4.99f; // 强制封顶，保证 5.0 - Vol 永远是正数！
    
    // 3. 计算传感器电阻 RS
    float RS = (5.0f - Vol) / (Vol * 0.5f);
    
    // 二次保护：防止极度接近 5V 时 RS 变得非常接近 0 导致溢出
    if(RS < 0.01f) RS = 0.01f;
    
    // 4. 计算 PPM 浓度
    float R0 = 6.64f; 
    float ppm = pow(11.5428f * R0 / RS, 0.6549f);
    
    // 【显示美化】：对于异常大的数值进行封顶限制，防止屏幕显示越界
    if (ppm > 9999.0f) {
        ppm = 9999.0f;
    }
    
    return ppm;
}
