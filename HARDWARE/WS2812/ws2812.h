#ifndef __WS2812_H
#define __WS2812_H

#include "sys.h"

#define WS2812_LED_NUM  60

/* 默认使用 PA0 作为 WS2812 数据输出脚。
 * 接线：PA0 -> WS2812 DIN，GND 共地。
 */
#define WS2812_GPIO_PORT GPIOA
#define WS2812_GPIO_PIN  GPIO_PIN_0

void WS2812_Init(void);
void WS2812_Clear(void);
void WS2812_SetPixel(u16 index, u8 r, u8 g, u8 b);
void WS2812_Show(void);
void WS2812_Fill(u8 r, u8 g, u8 b);
void WS2812_RainbowStep(u8 offset);

#endif
