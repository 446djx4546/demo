#ifndef __LIGHTSENSOR_H
#define __LIGHTSENSOR_H

#include "stm32f10x.h"

void LightSensor_Init(void);
uint16_t LightSensor_GetADCValue(void);
uint8_t LightSensor_GetIntensity(void); // 获取光照强度百分比 (0~100)

#endif
