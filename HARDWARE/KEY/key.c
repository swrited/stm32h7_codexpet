#include "key.h"
#include "delay.h"

void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_Initure.Pin = GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_9;
    GPIO_Initure.Mode = GPIO_MODE_INPUT;
    GPIO_Initure.Pull = GPIO_PULLUP;              // 按键另一端接 GND 时使用上拉
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);
}

u8 KEY_Scan(u8 mode)
{
    static u8 key_up = 1;

    if (mode == 1) key_up = 1;  // 支持连按

    if (key_up && (KEY1 == 0 || KEY2 == 0 || KEY3 == 0 || KEY4 == 0))
    {
        delay_ms(10);           // 消抖
        key_up = 0;

        if (KEY1 == 0) return KEY1_PRES;
        if (KEY2 == 0) return KEY2_PRES;
        if (KEY3 == 0) return KEY3_PRES;
        if (KEY4 == 0) return KEY4_PRES;
    }
    else if (KEY1 == 1 && KEY2 == 1 && KEY3 == 1 && KEY4 == 1)
    {
        key_up = 1;
    }

    return 0;
}
