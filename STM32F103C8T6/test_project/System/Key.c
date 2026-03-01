#include "stm32f10x.h"
#include "Key.h"

/**
  * @brief  按键(ADC)初始化
  */
void Key_Init(void)
{
    // 1. 开启 GPIOA 和 ADC1 的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    // 2. 配置 PA0 为模拟输入模式
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; // 模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 配置 ADC1
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;                  // 独立模式
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;                       // 单通道模式
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;                 // 单次转换模式
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; // 软件触发
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;              // 数据右对齐
    ADC_InitStructure.ADC_NbrOfChannel = 1;                             // 转换通道数量
    ADC_Init(ADC1, &ADC_InitStructure);

    // 4. 开启 ADC 并进行校准
    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) == SET);
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) == SET);
}

/**
  * @brief  获取 ADC 原始转换结果
  * @retval ADC 采样值 (0~4095)
  */
uint16_t Key_GetADCValue(void)
{
    // 配置规则组通道：ADC1, 通道0, 序列1, 采样时间55.5个周期
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_55Cycles5);
    
    // 触发软件转换
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    
    // 等待转换完成标志位 (EOC)
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    
    // 读取并返回结果
    return ADC_GetConversionValue(ADC1);
}

/**
  * @brief  获取按键键码
  * @retval 按键键码 (0:无按键, 1:SW1, 2:SW2, 3:SW3, 4:SW4)
  */
uint8_t Key_GetNum(void)
{
    uint16_t adc_value = Key_GetADCValue();
    uint8_t key_num = 0;

    // 根据原理图分压计算的阈值进行判断
    if (adc_value > 3800) {
        key_num = 0; // 无按键按下 (约 4095)
    } 
    else if (adc_value < 200) {
        key_num = 1; // SW1 被按下 (直接接地，约 0)
    } 
    else if (adc_value > 500 && adc_value < 900) {
        key_num = 2; // SW2 被按下 (约 738)
    } 
    else if (adc_value > 1100 && adc_value < 1400) {
        key_num = 3; // SW3 被按下 (约 1251)
    } 
    else if (adc_value > 1700 && adc_value < 2200) {
        key_num = 4; // SW4 被按下 (约 1951)
    }

    return key_num;
}
