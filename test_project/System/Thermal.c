#include "stm32f10x.h"
#include "Thermal.h"
#include "Kalman.h"
#include <math.h>

static Kalman_TypeDef Thermal_KF;

float Thermal_RawTemp = 0.0f;

/**
  * @brief  热敏传感器初始化 (对应引脚 PA3)
  */
void Thermal_Init(void)
{
    // 1. 开启 GPIOA 和 ADC1 的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    // ADC 时钟分频 (72MHz / 6 = 12MHz)
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    // 2. 配置 PA3 为模拟输入模式
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3; // 【修改处】：改为 PA3
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

    // 5. 初始化卡尔曼滤波器，设置过程噪声协方差 Q 和测量噪声协方差 R
    Kalman_Init(&Thermal_KF, 0.01f, 0.1f);
}

/**
  * @brief  获取热敏传感器 ADC 原始值
  * @retval ADC 采样值 (0~4095)
  */
uint16_t Thermal_GetADCValue(void)
{
    // 1. 将采样周期改为最长：ADC_SampleTime_239Cycles5
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 1, ADC_SampleTime_239Cycles5);
    
    // 2. 第一次转换：假读 (Dummy Read)，用来给采样电容充电，覆盖掉上一个通道的残留电压
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    ADC_GetConversionValue(ADC1); // 读出来的数据直接丢弃，不保存
    
    // 3. 第二次转换：真读，这才是当前通道最准确的电压
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
    return ADC_GetConversionValue(ADC1);

}

/**
  * @brief  获取计算后的实际温度值 (摄氏度)
  * @retval 温度值 (float 类型)
  */
float Thermal_GetTemp(void)
{
    uint16_t adc_val = Thermal_GetADCValue();
    
    // 防止除以 0 或无穷大的错误，针对上偏置电路，adc_val 接近 4095 时也要保护
    if(adc_val == 0 || adc_val >= 4090) return 0.0f; 

    // 1. 将 ADC 值转换为电压值 (假设基准电压为 3.3V)
    float voltage = (float)adc_val / 4095.0f * 3.3f;

    // 2. 根据上偏置分压原理计算 NTC 热敏电阻的当前阻值 Rt
    // 电路结构：VCC(3.3V) -> 10k固定电阻 -> PA3 -> NTC热敏电阻 -> GND
    // 【已修改】：适应“温度越高 ADC 越小”的硬件
    float Rt = 10000.0f * voltage / (3.3f - voltage);

    // 3. 使用 Steinhart-Hart 方程 (B值法) 计算温度
    // 典型 NTC 10K 传感器的 B 值为 3950
    float T1 = 1.0f / 298.15f;
    float T2 = logf(Rt / 10000.0f) / 3950.0f;  // 继续使用 logf 防止编译报错
    float T_Kelvin = 1.0f / (T1 + T2);
    
    // 转换为摄氏度
    float T_Celsius = T_Kelvin - 273.15f;

    Thermal_RawTemp = T_Celsius;

    return Kalman_Filter(&Thermal_KF, T_Celsius);
}
