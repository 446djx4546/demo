#include "stm32f10x.h"
#include "LightSensor.h"

/**
  * @brief  光敏传感器初始化 (对应引脚 PA2)
  */
void LightSensor_Init(void)
{
    // 1. 开启 GPIOA 和 ADC1 的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    // ADC 时钟分频 (72MHz / 6 = 12MHz)
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    // 2. 配置 PA2 为模拟输入模式
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; // 【PA2】
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 ADC1
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    // 4. 开启 ADC 并进行校准
    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) == SET);
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) == SET);
}

/**
  * @brief  获取光敏传感器 ADC 原始值
  * @retval ADC 采样值 (0~4095)
  */
uint16_t LightSensor_GetADCValue(void)
{
   // 1. 将采样周期改为最长：ADC_SampleTime_239Cycles5
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_239Cycles5);
    
    // 2. 第一次转换：假读 (Dummy Read)，消除通道切换带来的串扰
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    ADC_GetConversionValue(ADC1); // 丢弃第一次的结果
    
    // 3. 第二次转换：获取干净真实的数据
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    return ADC_GetConversionValue(ADC1);
}

/**
  * @brief  获取光照强度百分比 (0~100)
  * @retval 相对光照强度百分比
  */
uint8_t LightSensor_GetIntensity(void)
{
    uint16_t adc_val = LightSensor_GetADCValue();
    
    // 【修改处】：将公式反转。因为光照越强，adc_val 越小。
    // 用 4095 减去 adc_val，就能算出真实的相对光照强度百分比了
    float percentage = (4095.0f - (float)adc_val) / 4095.0f * 100.0f;
    
    // 限制上下限，防止计算出现溢出
    if (percentage > 100.0f) percentage = 100.0f;
    if (percentage < 0.0f) percentage = 0.0f;

    return (uint8_t)percentage;
}
