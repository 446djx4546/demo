#include "Store.h"
#include "stm32f10x_flash.h"

#define STORE_START_ADDRESS		0x0800FC00 // STM32F103C8T6 最后一页的起始地址

uint16_t Store_Data[16];

void Store_Init(void)
{
    // 读取当前 Flash 第一个位置的数据
    // 我们用 0xA5A5 作为“已经初始化过”的标志字
    if (*(__IO uint16_t*)STORE_START_ADDRESS != 0xA5A5)
    {
        // 如果不是 0xA5A5，说明是第一次运行或被清空过，需要初始化这一页 Flash
        FLASH_Unlock();
        FLASH_ErasePage(STORE_START_ADDRESS);
        FLASH_ProgramHalfWord(STORE_START_ADDRESS, 0xA5A5); // 写入标志字
        for (uint16_t i = 1; i < 16; i++)
        {
            FLASH_ProgramHalfWord(STORE_START_ADDRESS + i * 2, 0x0000); // 其它全填0
        }
        FLASH_Lock();
    }
    
    // 将 Flash 中的数据读出到内存里的 Store_Data 数组，方便后续读取和修改
    for (uint16_t i = 0; i < 16; i++)
    {
        Store_Data[i] = *(__IO uint16_t*)(STORE_START_ADDRESS + i * 2);
    }
}

void Store_Save(void)
{
    // 保存数据时，先擦除整页，再把整个数组写进去
    FLASH_Unlock();
    FLASH_ErasePage(STORE_START_ADDRESS);
    for (uint16_t i = 0; i < 16; i++)
    {
        FLASH_ProgramHalfWord(STORE_START_ADDRESS + i * 2, Store_Data[i]);
    }
    FLASH_Lock();
}

void Store_Clear(void)
{
    FLASH_Unlock();
    FLASH_ErasePage(STORE_START_ADDRESS);
    FLASH_Lock();
}
