#include "lcd.h"

/**************************************************************
 *  240x240 IPS LCD 驱动 (ST7789)
 *
 *  引脚:
 *    PB15 = SDI (MOSI)    PB13 = SCL (SCK)
 *    PB12 = RES (复位)     PB1  = DC
 *    PB0  = BLK (背光)     PB14 = SDO (未用)
 *
 *  屏幕无 CS 脚，写数据时不控制 CS
 **************************************************************/

u16 POINT_COLOR = 0x0000;   // 画点颜色(默认黑色)
u16 BACK_COLOR  = 0xFFFF;   // 背景色(默认白色)

_lcd_dev lcddev;            // LCD 参数

/************************************************
 *  GPIO 初始化 + SPI 初始化 + 背光点亮
 ************************************************/
void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* PB0=BLK, PB1=DC, PB14=RES 设为推挽输出 */
    GPIO_Initure.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_14;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_PULLUP;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);

    /* 初始状态：RES 拉高，DC 拉高，背光拉高 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0,  GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1,  GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);

    SPI2_Init();
    SPI2_SetSpeed(SPI_BAUDRATEPRESCALER_2);  // 初始化后加速

    LCD_HardwareRest();   // 硬件复位
    LCD_BLK_On;           // 打开背光
}

/************************************************
 *  硬件复位: 拉低 RES → 延时 → 拉高 RES
 *  你的屏幕 RES 接的是 PB14 / 板子 SDO
 ************************************************/
void LCD_HardwareRest(void)
{
    LCD_RST_Clr;           // PB14 拉低，复位开始
    LCD_Delay_ms(50);
    LCD_RST_Set;           // PB14 拉高，复位结束
    LCD_Delay_ms(120);     // 等待屏幕启动
}

/*************** SPI 写命令 (DC=0) ****************/
void LCD_WR_REG(u8 regval)
{
    LCD_RS_CLR;                     // DC=0, 命令模式
    SPI2_ReadWriteByte(regval);
    LCD_RS_SET;                     // DC=1
}

/*************** SPI 写 8bit 数据 (DC=1) **********/
void LCD_WR_DATA8(u8 data)
{
    LCD_RS_SET;                     // DC=1, 数据模式
    SPI2_ReadWriteByte(data);
}

/*************** SPI 写 16bit 数据 (DC=1) *********/
void LCD_WR_DATA16(u16 data)
{
    LCD_RS_SET;
    SPI2_ReadWriteByte(data >> 8);
    SPI2_ReadWriteByte(data);
}

/************** 写寄存器: 命令 + 1字节参数 *********/
void LCD_WriteReg(u8 LCD_Reg, u8 LCD_RegValue)
{
    LCD_WR_REG(LCD_Reg);
    LCD_WR_DATA8(LCD_RegValue);
}

/************** 准备写 GRAM ***********************/
void LCD_WriteRAM_Prepare(void)
{
    LCD_WR_REG(lcddev.wramcmd);
}

/************** 写一个像素到 GRAM *****************/
void LCD_WriteRAM(u16 RGB_Code)
{
    LCD_WR_DATA16(RGB_Code);
}

/************** 开/关显示 ************************/
void LCD_DisplayOn(void)  { LCD_WR_REG(0x29); }
void LCD_DisplayOff(void) { LCD_WR_REG(0x28); }

/************************************************
 *  设置窗口 (列/行范围)
 ************************************************/
void LCD_Set_Window(u16 sx, u16 sy, u16 width, u16 height)
{
    u16 ex = sx + width  - 1;
    u16 ey = sy + height - 1;

    LCD_WR_REG(0x2A);          // CASET 列地址
    LCD_WR_DATA16(sx);
    LCD_WR_DATA16(ex);

    LCD_WR_REG(0x2B);          // RASET 行地址
    LCD_WR_DATA16(sy);
    LCD_WR_DATA16(ey);

    LCD_WR_REG(0x2C);          // RAMWR 写内存
}

/************************************************
 *  向 LCD 连续写入字节数据
 ************************************************/
void LCD_WriteBytes(u8 *buf, u32 len)
{
    LCD_RS_SET;
    HAL_SPI_Transmit(&SPI2_Handler, buf, len, 1000);
}

/************************************************
 *  显示一段 RGB565 大端原始数据
 ************************************************/
void LCD_ShowRGB565Bytes(u16 x, u16 y, u16 w, u16 h, u8 *buf, u32 len)
{
    LCD_Set_Window(x, y, w, h);
    LCD_WriteBytes(buf, len);
}

/************************************************
 *  设置光标位置 (单点用)
 ************************************************/
void LCD_SetCursor(u16 Xpos, u16 Ypos)
{
    LCD_Set_Window(Xpos, Ypos, 1, 1);
}

/************************************************
 *  画点
 ************************************************/
void LCD_DrawPoint(u16 x, u16 y, u16 color)
{
    LCD_SetCursor(x, y);
    LCD_WriteRAM(color);
}

/************************************************
 *  填充矩形区域
 ************************************************/
void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color)
{
    u32 i, total;
    total = (u32)(ex - sx + 1) * (ey - sy + 1);

    LCD_Set_Window(sx, sy, ex - sx + 1, ey - sy + 1);
    for (i = 0; i < total; i++)
    {
        LCD_WR_DATA16(color);
    }
}

/************************************************
 *  清屏
 ************************************************/
void LCD_Clear(u16 Color)
{
    LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, Color);
}

/************************************************
 *  画线 (Bresenham)
 ************************************************/
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
    u16 t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;
    if (delta_x > 0) incx = 1;
    else if (delta_x == 0) incx = 0;
    else { incx = -1; delta_x = -delta_x; }
    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0;
    else { incy = -1; delta_y = -delta_y; }
    delta_x = (u16)delta_x;
    delta_y = (u16)delta_y;
    distance = delta_x > delta_y ? delta_x : delta_y;
    for (t = 0; t <= distance + 1; t++)
    {
        LCD_DrawPoint(row, col, color);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance) { xerr -= distance; row += incx; }
        if (yerr > distance) { yerr -= distance; col += incy; }
    }
}

/************************************************
 *  画矩形
 ************************************************/
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2, u16 color)
{
    LCD_DrawLine(x1, y1, x2, y1, color);
    LCD_DrawLine(x1, y2, x2, y2, color);
    LCD_DrawLine(x1, y1, x1, y2, color);
    LCD_DrawLine(x2, y1, x2, y2, color);
}

/************************************************
 *  画圆
 ************************************************/
void LCD_Draw_Circle(u16 x0, u16 y0, u8 r, u16 color)
{
    int a, b;
    int di;
    a = 0; b = r; di = 3 - (r << 1);
    while (a <= b)
    {
        LCD_DrawPoint(x0 + a, y0 - b, color);
        LCD_DrawPoint(x0 + b, y0 - a, color);
        LCD_DrawPoint(x0 + b, y0 + a, color);
        LCD_DrawPoint(x0 + a, y0 + b, color);
        LCD_DrawPoint(x0 - a, y0 + b, color);
        LCD_DrawPoint(x0 - b, y0 + a, color);
        LCD_DrawPoint(x0 - b, y0 - a, color);
        LCD_DrawPoint(x0 - a, y0 - b, color);
        a++;
        if (di < 0) di += 4 * a + 6;
        else { di += 10 + 4 * (a - b); b--; }
    }
}

/************************************************
 *  快速画点 (兼容旧例程)
 ************************************************/
void LCD_Fast_DrawPoint(u16 x, u16 y, u16 color)
{
    LCD_DrawPoint(x, y, color);
}

/************************************************
 *  显示字符 (6参数, 兼容 Text.c 和 GBK_LibDrive.c)
 ************************************************/
void LCD_ShowChar(u16 x, u16 y, u8 num, u8 size, u16 color, u8 mode)
{
    (void)x; (void)y; (void)num; (void)size; (void)color; (void)mode;
}

/************************************************
 *  显示字符串
 ************************************************/
void LCD_ShowString(u16 x, u16 y, u16 width, u16 height, u8 size, u16 color, u8 *p)
{
    (void)x; (void)y; (void)width; (void)height; (void)size; (void)color; (void)p;
}

/************************************************
 *  显示数字
 ************************************************/
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size, u16 color)
{
    (void)x; (void)y; (void)num; (void)len; (void)size; (void)color;
}

/************************************************
 *  扫描方向 / 显示方向 (兼容旧例程)
 ************************************************/
void LCD_Scan_Dir(u8 dir) { (void)dir; }
void LCD_Display_Dir(u8 dir) { (void)dir; }

/************************************************
 *  ST7789 初始化序列 (240x240)
 *  关键: 很多模块需要 y_offset = 80
 ************************************************/
void LCD_Init(void)
{
    LCD_GPIO_Init();  // 初始化 IO + SPI + 硬件复位

    /* ===== ST7789 初始化命令序列 ===== */

    LCD_WR_REG(0x11);       // Sleep out
    LCD_Delay_ms(120);      // 等待 120ms

    LCD_WR_REG(0x36);       // MADCTL
    LCD_WR_DATA8(0x00);     // 默认扫描方向 (可根据需求修改)
    /* MADCTL 常用值: 0x00=正向, 0x60=横屏, 0xC0=旋转180, 0xA0=旋转270 */

    LCD_WR_REG(0x3A);       // COLMOD 像素格式
    LCD_WR_DATA8(0x05);     // 16bit RGB565

    LCD_WR_REG(0xB2);       // PORCTRL
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x33);
    LCD_WR_DATA8(0x33);

    LCD_WR_REG(0xB7);       // GCTRL
    LCD_WR_DATA8(0x35);

    LCD_WR_REG(0xBB);       // VCOMS
    LCD_WR_DATA8(0x2B);

    LCD_WR_REG(0xC0);       // LCMCTRL
    LCD_WR_DATA8(0x2C);

    LCD_WR_REG(0xC2);       // VDVVRHEN
    LCD_WR_DATA8(0x01);
    LCD_WR_DATA8(0xFF);

    LCD_WR_REG(0xC3);       // VRHS
    LCD_WR_DATA8(0x11);

    LCD_WR_REG(0xC4);       // VDVS
    LCD_WR_DATA8(0x20);

    LCD_WR_REG(0xC6);       // FRCTRL2
    LCD_WR_DATA8(0x0F);

    LCD_WR_REG(0xD0);       // PWCTRL1
    LCD_WR_DATA8(0xA4);
    LCD_WR_DATA8(0xA1);

    LCD_WR_REG(0xE0);       // PVGAMCTRL
    LCD_WR_DATA8(0xD0);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x05);
    LCD_WR_DATA8(0x0E);
    LCD_WR_DATA8(0x15);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x37);
    LCD_WR_DATA8(0x43);
    LCD_WR_DATA8(0x47);
    LCD_WR_DATA8(0x09);
    LCD_WR_DATA8(0x15);
    LCD_WR_DATA8(0x12);
    LCD_WR_DATA8(0x16);
    LCD_WR_DATA8(0x19);

    LCD_WR_REG(0xE1);       // NVGAMCTRL
    LCD_WR_DATA8(0xD0);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x05);
    LCD_WR_DATA8(0x0D);
    LCD_WR_DATA8(0x0C);
    LCD_WR_DATA8(0x06);
    LCD_WR_DATA8(0x2D);
    LCD_WR_DATA8(0x44);
    LCD_WR_DATA8(0x40);
    LCD_WR_DATA8(0x0E);
    LCD_WR_DATA8(0x1C);
    LCD_WR_DATA8(0x18);
    LCD_WR_DATA8(0x16);
    LCD_WR_DATA8(0x19);

    LCD_WR_REG(0x21);       // INVON 颜色反转 (有些屏幕需要)

    LCD_WR_REG(0x29);       // Display ON

    /* 设置 LCD 参数 */
    lcddev.width   = 240;
    lcddev.height  = 240;
    lcddev.wramcmd = 0x2C;
    lcddev.setxcmd = 0x2A;
    lcddev.setycmd = 0x2B;
}

/************************************************
 *  颜色测试: 红→绿→蓝→黑→白 交替刷屏
 ************************************************/
void Color_Test(void)
{
    LCD_Clear(WHITE);  LCD_Delay_ms(200);
    LCD_Clear(BLACK);  LCD_Delay_ms(200);
    LCD_Clear(RED);    LCD_Delay_ms(200);
    LCD_Clear(GREEN);  LCD_Delay_ms(200);
    LCD_Clear(BLUE);   LCD_Delay_ms(200);
    LCD_Clear(WHITE);  LCD_Delay_ms(200);
}

/************************************************
 *  绘图测试: 在屏幕上画一些图形
 ************************************************/
void Draw_Test(void)
{
    LCD_Clear(WHITE);

    /* 画几条彩色直线 */
    LCD_DrawLine(0,   0,   239, 239, RED);
    LCD_DrawLine(239, 0,   0,   239, BLUE);
    LCD_DrawLine(0,   120, 239, 120, GREEN);
    LCD_DrawLine(120, 0,   120, 239, GREEN);

    /* 画矩形 */
    LCD_DrawRectangle(20, 20, 100, 100, YELLOW);
    LCD_DrawRectangle(140, 20, 220, 100, CYAN);

    /* 画实心圆 */
    LCD_Draw_Circle(60, 180, 30, MAGENTA);
    LCD_Draw_Circle(180, 180, 30, BRED);

    /* 填充色块 */
    LCD_Fill(30, 30, 90, 90, GRAY);
    LCD_Fill(150, 30, 210, 90, LIGHTBLUE);
}
