#ifndef __THERMAL_H
#define __THERMAL_H

#include "stm32f10x.h"

void Thermal_Init(void);
uint16_t Thermal_GetADCValue(void);
float Thermal_GetTemp(void);

#endif
