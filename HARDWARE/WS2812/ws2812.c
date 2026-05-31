#include "ws2812.h"
#include "delay.h"

/* WS2812 数据缓存，顺序按 RGB 存，发送时按 GRB 输出 */
static u8 ws2812_buf[WS2812_LED_NUM][3];

#define WS2812_HIGH() HAL_GPIO_WritePin(WS2812_GPIO_PORT, WS2812_GPIO_PIN, GPIO_PIN_SET)
#define WS2812_LOW()  HAL_GPIO_WritePin(WS2812_GPIO_PORT, WS2812_GPIO_PIN, GPIO_PIN_RESET)

/*
 * 400MHz 主频下的粗略软件延时。
 * WS2812 时序要求约：
 * 0码: 高 0.35us，低 0.8us
 * 1码: 高 0.7us， 低 0.6us
 *
 * 这里用空循环实现，够用于先点亮测试；如果后续不稳定，再改成 TIM+DMA。
 */
static void ws_delay_cycles(volatile u32 n)
{
    while (n--)
    {
        __NOP();
    }
}

static void ws_send_0(void)
{
    WS2812_HIGH();
    ws_delay_cycles(22);
    WS2812_LOW();
    ws_delay_cycles(58);
}

static void ws_send_1(void)
{
    WS2812_HIGH();
    ws_delay_cycles(58);
    WS2812_LOW();
    ws_delay_cycles(22);
}

static void ws_send_byte(u8 byte)
{
    u8 i;
    for (i = 0; i < 8; i++)
    {
        if (byte & 0x80) ws_send_1();
        else ws_send_0();
        byte <<= 1;
    }
}

static u8 wheel(u8 pos, u8 *r, u8 *g, u8 *b)
{
    pos = 255 - pos;
    if (pos < 85)
    {
        *r = 255 - pos * 3;
        *g = 0;
        *b = pos * 3;
    }
    else if (pos < 170)
    {
        pos -= 85;
        *r = 0;
        *g = pos * 3;
        *b = 255 - pos * 3;
    }
    else
    {
        pos -= 170;
        *r = pos * 3;
        *g = 255 - pos * 3;
        *b = 0;
    }
    return 0;
}

void WS2812_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_Initure.Pin = WS2812_GPIO_PIN;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(WS2812_GPIO_PORT, &GPIO_Initure);

    WS2812_LOW();
    WS2812_Clear();
    WS2812_Show();
}

void WS2812_Clear(void)
{
    u16 i;
    for (i = 0; i < WS2812_LED_NUM; i++)
    {
        ws2812_buf[i][0] = 0;
        ws2812_buf[i][1] = 0;
        ws2812_buf[i][2] = 0;
    }
}

void WS2812_SetPixel(u16 index, u8 r, u8 g, u8 b)
{
    if (index >= WS2812_LED_NUM) return;
    ws2812_buf[index][0] = r;
    ws2812_buf[index][1] = g;
    ws2812_buf[index][2] = b;
}

void WS2812_Show(void)
{
    u16 i;
    __disable_irq();
    for (i = 0; i < WS2812_LED_NUM; i++)
    {
        /* WS2812 数据顺序是 GRB */
        ws_send_byte(ws2812_buf[i][1]);
        ws_send_byte(ws2812_buf[i][0]);
        ws_send_byte(ws2812_buf[i][2]);
    }
    __enable_irq();

    /* reset/latch，低电平 > 50us */
    delay_us(80);
}

void WS2812_Fill(u8 r, u8 g, u8 b)
{
    u16 i;
    for (i = 0; i < WS2812_LED_NUM; i++)
    {
        WS2812_SetPixel(i, r, g, b);
    }
    WS2812_Show();
}

void WS2812_RainbowStep(u8 offset)
{
    u16 i;
    u8 r, g, b;
    for (i = 0; i < WS2812_LED_NUM; i++)
    {
        wheel((u8)((i * 256 / WS2812_LED_NUM + offset) & 0xFF), &r, &g, &b);
        /* 先降低亮度，避免 60 颗灯电流太大 */
        WS2812_SetPixel(i, r / 8, g / 8, b / 8);
    }
    WS2812_Show();
}
