#ifndef _KEY_H
#define _KEY_H

#include "sys.h"

/* 屏幕模块四个按键
 * K1 -> PB3
 * K2 -> PB5
 * K3 -> PB7
 * K4 -> PB9
 * 默认按下为低电平，使用内部上拉
 */

#define KEY1        HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3)
#define KEY2        HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5)
#define KEY3        HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7)
#define KEY4        HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9)

#define KEY1_PRES   1
#define KEY2_PRES   2
#define KEY3_PRES   3
#define KEY4_PRES   4

void KEY_Init(void);
u8 KEY_Scan(u8 mode);

#endif
