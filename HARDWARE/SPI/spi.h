#ifndef __SPI_H
#define __SPI_H
#include "sys.h"

/* SPI2 用于 LCD 通信
 * SCK  -> PB13
 * MOSI -> PB15 (SDI)
 * 屏幕无 CS 脚，不需要 CS
 */

extern SPI_HandleTypeDef SPI2_Handler;

void SPI2_Init(void);
void SPI2_SetSpeed(u32 SPI_BaudRatePrescaler);
u8 SPI2_ReadWriteByte(u8 TxData);

#endif
