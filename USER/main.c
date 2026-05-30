#include "sys.h"
#include "delay.h"
#include "lcd.h"
#include "key.h"
#include "sdcard.h"
#include "fat32_mini.h"

#define INNI_X              24
#define INNI_Y              16
#define INNI_W              192
#define INNI_H              208

static const char *idle_frames[6] = {
    "0:/INNI/I/I00.BIN",
    "0:/INNI/I/I01.BIN",
    "0:/INNI/I/I02.BIN",
    "0:/INNI/I/I03.BIN",
    "0:/INNI/I/I04.BIN",
    "0:/INNI/I/I05.BIN",
};

static const char *wave_frames[4] = {
    "0:/INNI/WV/WV00.BIN",
    "0:/INNI/WV/WV01.BIN",
    "0:/INNI/WV/WV02.BIN",
    "0:/INNI/WV/WV03.BIN",
};

static u8 show_frame(const char *path)
{
    return FAT32_DisplayRGB565File(path, INNI_X, INNI_Y, INNI_W, INNI_H);
}

static void play_action(const char **frames, u8 count, u16 delay_ms_time)
{
    u8 i;
    for (i = 0; i < count; i++)
    {
        if (show_frame(frames[i]) != 0)
        {
            LCD_Clear(RED);
            return;
        }
        HAL_Delay(delay_ms_time);
    }
}

int main(void)
{
    GPIO_InitTypeDef led;
    u8 ret;
    u8 frame = 0;

    HAL_Init();
    Stm32_Clock_Init(160, 5, 2, 4);
    delay_init(400);

    LCD_Init();
    KEY_Init();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    led.Pin   = GPIO_PIN_1;
    led.Mode  = GPIO_MODE_OUTPUT_PP;
    led.Pull  = GPIO_PULLUP;
    led.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &led);

    LCD_Clear(BLACK);

    ret = SDCard_Init();
    if (ret != 0)
    {
        LCD_Clear(RED);       // SD 初始化失败
        while (1)
        {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
            HAL_Delay(100);
        }
    }

    ret = FAT32_Mount();
    if (ret != 0)
    {
        LCD_Clear(YELLOW);    // SD 能初始化，但 FAT32 挂载失败
        while (1)
        {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
            HAL_Delay(200);
        }
    }

    LCD_Clear(GREEN);         // SD + FAT32 成功
    HAL_Delay(300);

    while (1)
    {
        u8 key = KEY_Scan(0);

        if (key == KEY1_PRES)
        {
            play_action(idle_frames, 6, 120);   // K1 播放 idle
        }
        else if (key == KEY2_PRES)
        {
            play_action(wave_frames, 4, 120);   // K2 播放 waving
        }
        else if (key == KEY3_PRES)
        {
            LCD_Clear(BLUE);
        }
        else if (key == KEY4_PRES)
        {
            LCD_Clear(MAGENTA);
        }

        /* 默认循环播放 idle */
        show_frame(idle_frames[frame]);
        frame++;
        if (frame >= 6) frame = 0;

        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
        HAL_Delay(120);
    }
}
