#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>

void Key_Init(void);
uint16_t Key_GetADCValue(void);
uint8_t Key_GetNum(void);

#endif
