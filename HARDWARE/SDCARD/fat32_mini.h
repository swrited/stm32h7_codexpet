#ifndef __FAT32_MINI_H
#define __FAT32_MINI_H

#include "sys.h"

/* 简易 FAT32 读取器：只支持 512 字节扇区、短文件名 8.3、只读 */

u8 FAT32_Mount(void);
u8 FAT32_OpenFile(const char *path, u32 *first_cluster, u32 *file_size);
u8 FAT32_DisplayRGB565File(const char *path, u16 x, u16 y, u16 w, u16 h);

#endif
