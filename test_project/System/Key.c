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
    uint32_t adc_sum = 0;
    for(int i = 0; i < 3; i++) {
        adc_sum += Key_GetADCValue();
    }
    uint16_t adc_value = adc_sum / 3;

    uint8_t current_key = 0;

    // 1. 获取当前按下的是哪个键
    if (adc_value < 400) {
        current_key = 1; // SW1 (原 0~200 的扩大版)
    } 
    else if (adc_value < 1450) {
        current_key = 2; // SW2 (原 500~900 的扩大版)
    } 
    else if (adc_value < 2380) {
        current_key = 3; // SW3 (原 2000~2400 的扩大版)
    } 
    else if (adc_value < 3500) {
        current_key = 4; // SW4 (原 2400~2700 的扩大版)
    }
    else {
        current_key = 0; // >= 3500 认为是未按下状态
    }

    // 2. 核心边缘检测逻辑（静态变量会记住上一次的值）
    static uint8_t last_key = 0; 
    uint8_t valid_key = 0;

    // 如果这次按下了按键，且上一次是没有按下的状态（说明是刚按下的瞬间）
    if (current_key != 0 && last_key == 0) 
    {
        valid_key = current_key; // 记录下有效键值
    }

    // 更新上一次的状态，供下一次循环使用
    last_key = current_key; 

    // 只在按下的瞬间返回 1~4，按住不放或者松开时都返回 0
    return valid_key; 
}
