#ifndef __STORE_H
#define __STORE_H

#include "stm32f10x.h"

// 暴露给外部的数据数组
extern uint16_t Store_Data[10];

void Store_Init(void);
void Store_Save(void);
void Store_Clear(void);

#endif
