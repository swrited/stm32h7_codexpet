#ifndef __LCD_H
#define __LCD_H

#include "sys.h"
#include "stdlib.h"
#include "delay.h"
#include "spi.h"

/**************************************************************
 *  240x240 IPS LCD 驱动 (ST7789)
 *  引脚对应:
 *    PB15 = SDI (MOSI)
 *    PB13 = SCL (SCK)
 *    PB12 = RES (硬件复位)  ← 原来例程是 CS，现在改接屏幕 RES
 *    PB1  = DC  (命令/数据)
 *    PB0  = BLK (背光)
 *    PB14 = SDO (MISO, 未用)
 **************************************************************/

/* 类型定义 */
typedef unsigned          char   uint8_t;
typedef unsigned short     int   uint16_t;
typedef unsigned           int   uint32_t;
typedef uint32_t  u32;
typedef uint16_t u16;
typedef uint8_t  u8;

/*********************** 引脚控制宏 ***************************/

/* SDI (MOSI) - PB15 */
#define LCD_SDA_SET   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET)
#define LCD_SDA_CLR   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET)

/* SCL (SCK) - PB13 */
#define LCD_SCL_SET   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET)
#define LCD_SCL_CLR   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET)

/* RES (硬件复位) - PB14 / 板子 SDO */
#define LCD_RST_Set   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET)
#define LCD_RST_Clr   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET)

/* DC (命令/数据) - PB1 */
#define LCD_RS_SET    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET)
#define LCD_RS_CLR    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET)

/* BLK (背光) - PB0 */
#define LCD_BLK_SET   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)
#define LCD_BLK_CLR   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)

#define LCD_BLK_On    LCD_BLK_SET
#define LCD_BLK_Off   LCD_BLK_CLR

/* 屏幕尺寸 */
#define LCD_W  240
#define LCD_H  240

/* LCD 参数结构体 */
typedef struct
{
    u16 width;
    u16 height;
    u16 id;
    u8  dir;
    u16 wramcmd;
    u16 setxcmd;
    u16 setycmd;
} _lcd_dev;

extern _lcd_dev lcddev;
extern u16 POINT_COLOR;
extern u16 BACK_COLOR;

/* 别名 - 兼容旧例程 */
#define D_Color  POINT_COLOR
#define B_Color  BACK_COLOR

/* 颜色定义 (RGB565) */
#define WHITE     0xFFFF
#define BLACK     0x0000
#define BLUE      0x001F
#define RED       0xF800
#define GREEN     0x07E0
#define CYAN      0x7FFF
#define YELLOW    0xFFE0
#define MAGENTA   0xF81F
#define GRAY      0x8430
#define BRED      0xF81F
#define GRED      0xFFE0
#define GBLUE     0x07FF
#define BROWN     0xBC40
#define BRRED     0xFC07
#define DARKBLUE  0x01CF
#define LIGHTBLUE 0x7D7C
#define GRAYBLUE  0x5458
#define LIGHTGREEN 0x841F
#define GRAY0   0xEF7D
#define GRAY1   0x8410
#define GRAY2   0x4208

/* 延时宏 */
#define LCD_Delay_us  delay_us
#define LCD_Delay_ms  delay_ms

/*********************** 函数声明 *****************************/

void LCD_GPIO_Init(void);
void LCD_WR_REG(u8 regval);
void LCD_WR_DATA8(u8 data);
void LCD_WR_DATA16(u16 data);
void LCD_WriteReg(u8 LCD_Reg, u8 LCD_RegValue);
void LCD_WriteRAM_Prepare(void);
void LCD_WriteRAM(u16 RGB_Code);

void LCD_Init(void);
void LCD_HardwareRest(void);
void LCD_DisplayOn(void);
void LCD_DisplayOff(void);

void LCD_Clear(u16 Color);
void LCD_SetCursor(u16 Xpos, u16 Ypos);
void LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height);
void LCD_DrawPoint(u16 x, u16 y, u16 color);
void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color);
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2, u16 color);
void LCD_Draw_Circle(u16 x0, u16 y0, u8 r, u16 color);
void LCD_ShowChar(u16 x, u16 y, u8 num, u8 size, u16 color, u8 mode);
void LCD_ShowString(u16 x, u16 y, u16 width, u16 height, u8 size, u16 color, u8 *p);
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size, u16 color);
void LCD_Fast_DrawPoint(u16 x, u16 y, u16 color);

void LCD_WriteBytes(u8 *buf, u32 len);
void LCD_ShowRGB565Bytes(u16 x, u16 y, u16 w, u16 h, u8 *buf, u32 len);

void LCD_Scan_Dir(u8 dir);
void LCD_Display_Dir(u8 dir);

void Color_Test(void);
void Draw_Test(void);

#endif
