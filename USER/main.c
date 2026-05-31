#include "sys.h"
#include "delay.h"
#include "lcd.h"
#include "key.h"
#include "sdcard.h"
#include "fat32_mini.h"
#include "ws2812.h"
#include "usart.h"
#include <string.h>

#define PET_X   16
#define PET_Y   16
#define PET_W   96
#define PET_H   104

#define UI_Y    168
#define UI_H    72

#define FULL_X  0
#define FULL_Y  0
#define FULL_W  240
#define FULL_H  240

typedef enum
{
    MODE_PET = 0,
    MODE_GIF,
    MODE_PIC_CROP,
    MODE_PIC_FIT
} DisplayMode;

typedef enum
{
    PET_STATE_IDLE = 0,
    PET_STATE_THINKING,
    PET_STATE_ANSWERING,
    PET_STATE_BUSY
} PetState;

static DisplayMode mode = MODE_PET;
static PetState pet_state = PET_STATE_IDLE;
static u8 pet_frame = 0;
static u8 pet_dir = 0;
static u8 pet_frame_count = 6;
static u8 gif_frame = 0;
static char path_buf[32];
static char pet_prefix[3] = "I";
static char status_text[16] = "Idle";

static void check_uart_state(void);

static void make_pet_path(u8 frame)
{
    if (pet_state == PET_STATE_THINKING && frame >= 4) frame++;

    path_buf[0] = 0;
    strcpy(path_buf, "0:/INNI050/");
    strcat(path_buf, pet_prefix);
    strcat(path_buf, "/");
    strcat(path_buf, pet_prefix);
    path_buf[strlen(path_buf) + 1] = 0;
    path_buf[strlen(path_buf)] = '0' + (frame / 10);
    path_buf[strlen(path_buf) + 1] = 0;
    path_buf[strlen(path_buf)] = '0' + (frame % 10);
    strcat(path_buf, ".BIN");
}

static void make_gif_path(u8 frame)
{
    path_buf[0] = 0;
    strcpy(path_buf, "0:/GIF240/G");
    path_buf[strlen(path_buf) + 1] = 0;
    path_buf[strlen(path_buf)] = '0' + (frame / 10);
    path_buf[strlen(path_buf) + 1] = 0;
    path_buf[strlen(path_buf)] = '0' + (frame % 10);
    strcat(path_buf, ".BIN");
}

static u8 show_pet_frame(u8 frame)
{
    make_pet_path(frame);
    return FAT32_DisplayRGB565File(path_buf, PET_X, PET_Y, PET_W, PET_H);
}

static u8 show_gif_frame(u8 frame)
{
    make_gif_path(frame);
    return FAT32_DisplayRGB565File(path_buf, FULL_X, FULL_Y, FULL_W, FULL_H);
}

static u8 show_pic_crop(void)
{
    return FAT32_DisplayRGB565File("0:/PIC240/P00.BIN", FULL_X, FULL_Y, FULL_W, FULL_H);
}

static u8 show_pic_fit(void)
{
    return FAT32_DisplayRGB565File("0:/PICFIT/P00.BIN", FULL_X, FULL_Y, FULL_W, FULL_H);
}

static void draw_pet_ui_static(void)
{
    LCD_Fill(120, 20, 239, 95, BLACK);
    LCD_ShowString(126, 34, 100, 16, 16, CYAN, (u8 *)status_text);
    LCD_ShowString(126, 58, 40, 16, 16, WHITE, (u8 *)"Time");
}

static void draw_clock_only(void)
{
    u32 sec = HAL_GetTick() / 1000;
    u32 h = (sec / 3600) % 24;
    u32 m = (sec / 60) % 60;
    u32 s = sec % 60;

    /* 只清时间数字区域，不重画整个 UI，避免闪烁 */
    LCD_Fill(170, 58, 238, 73, BLACK);
    LCD_ShowNum(170, 58, h, 2, 16, YELLOW);
    LCD_ShowString(186, 58, 8, 16, 16, YELLOW, (u8 *)":");
    LCD_ShowNum(194, 58, m, 2, 16, YELLOW);
    LCD_ShowString(210, 58, 8, 16, 16, YELLOW, (u8 *)":");
    LCD_ShowNum(218, 58, s, 2, 16, YELLOW);
}

static void set_status_text(const char *text)
{
    strncpy(status_text, text, sizeof(status_text) - 1);
    status_text[sizeof(status_text) - 1] = 0;
    LCD_Fill(126, 34, 238, 49, BLACK);
    LCD_ShowString(126, 34, 100, 16, 16, CYAN, (u8 *)status_text);
}

static void set_pet_state(PetState state, const char *text)
{
    pet_state = state;
    pet_frame = 0;
    pet_dir = 0;

    if (state == PET_STATE_THINKING)
    {
        strcpy(pet_prefix, "WT");  // waiting/thinking
        pet_frame_count = 5;
    }
    else if (state == PET_STATE_ANSWERING)
    {
        strcpy(pet_prefix, "WV");  // waving/answering
        pet_frame_count = 4;
    }
    else if (state == PET_STATE_BUSY)
    {
        strcpy(pet_prefix, "RN");  // running/busy
        pet_frame_count = 6;
    }
    else
    {
        strcpy(pet_prefix, "I");   // idle
        pet_frame_count = 6;
    }

    set_status_text(text);
}

static void next_pet_frame(void)
{
    if (pet_state == PET_STATE_ANSWERING)
    {
        if (pet_dir == 0)
        {
            pet_frame++;
            if (pet_frame >= pet_frame_count - 1) pet_dir = 1;
        }
        else
        {
            if (pet_frame > 0) pet_frame--;
            else pet_dir = 0;
        }
    }
    else
    {
        pet_frame++;
        if (pet_frame >= pet_frame_count) pet_frame = 0;
    }
}

static u16 pet_delay_ms(void)
{
    if (pet_state == PET_STATE_THINKING) return 80;
    if (pet_state == PET_STATE_ANSWERING) return 60;
    if (pet_state == PET_STATE_BUSY) return 40;
    return 180;
}

static void pet_delay_with_uart(u16 ms)
{
    u16 i;
    for (i = 0; i < ms; i++)
    {
        check_uart_state();
        HAL_Delay(1);
    }
}

static void check_uart_state(void)
{
    u16 len = USART_RX_STA & 0x3FFF;

    if (len > 0 || (USART_RX_STA & 0x8000))
    {
        USART_RX_BUF[len] = 0;

        if (strstr((char *)USART_RX_BUF, "THINK") != 0)
        {
            set_pet_state(PET_STATE_THINKING, "Thinking");
            USART_RX_STA = 0;
        }
        else if (strstr((char *)USART_RX_BUF, "ANSWER") != 0)
        {
            set_pet_state(PET_STATE_ANSWERING, "Answering");
            USART_RX_STA = 0;
        }
        else if (strstr((char *)USART_RX_BUF, "IDLE") != 0)
        {
            set_pet_state(PET_STATE_IDLE, "Idle");
            USART_RX_STA = 0;
        }
        else if (strstr((char *)USART_RX_BUF, "BUSY") != 0)
        {
            set_pet_state(PET_STATE_BUSY, "Busy");
            USART_RX_STA = 0;
        }
        else if (USART_RX_STA & 0x8000)
        {
            set_status_text("RX");
            USART_RX_STA = 0;
        }
    }
}

int main(void)
{
    GPIO_InitTypeDef led;
    u8 ret;

    HAL_Init();
    Stm32_Clock_Init(160, 5, 2, 4);
    delay_init(400);

    LCD_Init();
    KEY_Init();
    uart_init(9600);
    WS2812_Init();
    WS2812_Fill(16, 0, 0);  // 上电先低亮度红色，确认灯带接线

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
        LCD_Clear(RED);
        while (1)
        {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
            HAL_Delay(100);
        }
    }

    ret = FAT32_Mount();
    if (ret != 0)
    {
        LCD_Clear(YELLOW);
        while (1)
        {
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
            HAL_Delay(200);
        }
    }

    LCD_Clear(BLACK);
    draw_pet_ui_static();
    draw_clock_only();

    while (1)
    {
        u8 key = KEY_Scan(0);
        static u8 rainbow_offset = 0;

        check_uart_state();

        if (key == KEY1_PRES)
        {
            mode = MODE_PET;       // K1: 宠物 50%
            pet_frame = 0;
            LCD_Clear(BLACK);
            draw_pet_ui_static();
            draw_clock_only();
            WS2812_Fill(0, 16, 0); // 绿
        }
        else if (key == KEY2_PRES)
        {
            mode = MODE_GIF;       // K2: GIF 动画
            gif_frame = 0;
            LCD_Clear(BLACK);
            WS2812_Fill(0, 0, 16); // 蓝
        }
        else if (key == KEY3_PRES)
        {
            mode = MODE_PIC_CROP;  // K3: JPG 裁剪铺满
            LCD_Clear(BLACK);
            WS2812_Fill(16, 16, 0); // 黄
        }
        else if (key == KEY4_PRES)
        {
            mode = MODE_PIC_FIT;   // K4: JPG 等比例完整显示
            LCD_Clear(BLACK);
            WS2812_Fill(16, 0, 16); // 紫
        }

        if (mode == MODE_PET)
        {
            if (show_pet_frame(pet_frame) != 0) LCD_Clear(RED);
            draw_clock_only();
            next_pet_frame();
            pet_delay_with_uart(pet_delay_ms());
        }
        else if (mode == MODE_GIF)
        {
            if (show_gif_frame(gif_frame) != 0) LCD_Clear(RED);
            gif_frame++;
            if (gif_frame >= 8) gif_frame = 0;
            HAL_Delay(100);
        }
        else if (mode == MODE_PIC_CROP)
        {
            if (show_pic_crop() != 0) LCD_Clear(RED);
            HAL_Delay(300);
        }
        else if (mode == MODE_PIC_FIT)
        {
            if (show_pic_fit() != 0) LCD_Clear(RED);
            HAL_Delay(300);
        }

        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
        rainbow_offset++;
        WS2812_RainbowStep(rainbow_offset);
    }
}
