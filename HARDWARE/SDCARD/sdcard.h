#ifndef __SDCARD_H
#define __SDCARD_H

#include "sys.h"

/* SDMMC1 引脚:
 * D0  -> PC8
 * D1  -> PC9
 * D2  -> PC10
 * D3  -> PC11
 * CLK -> PC12
 * CMD -> PD2
 */

extern SD_HandleTypeDef hsd1;

u8 SDCard_Init(void);
u8 SDCard_ReadBlocks(u8 *buf, u32 block_addr, u32 block_count);

#endif
