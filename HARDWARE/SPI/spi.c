#include "spi.h"

SPI_HandleTypeDef SPI2_Handler;

void SPI2_Init(void)
{
    SPI2_Handler.Instance = SPI2;
    SPI2_Handler.Init.Mode = SPI_MODE_MASTER;
    SPI2_Handler.Init.Direction = SPI_DIRECTION_1LINE;      // 只发送，屏幕 SDO/PB14 已接 RES，不读数据
    SPI2_Handler.Init.DataSize = SPI_DATASIZE_8BIT;
    SPI2_Handler.Init.CLKPolarity = SPI_POLARITY_HIGH;      // 按厂家例程使用 SPI Mode 3
    SPI2_Handler.Init.CLKPhase = SPI_PHASE_2EDGE;
    SPI2_Handler.Init.NSS = SPI_NSS_SOFT;
    SPI2_Handler.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    SPI2_Handler.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    SPI2_Handler.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;  // 先慢速，稳定后再调快
    SPI2_Handler.Init.FirstBit = SPI_FIRSTBIT_MSB;
    SPI2_Handler.Init.TIMode = SPI_TIMODE_DISABLE;
    SPI2_Handler.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    SPI2_Handler.Init.CRCPolynomial = 7;
    HAL_SPI_Init(&SPI2_Handler);

    __HAL_SPI_ENABLE(&SPI2_Handler);
    SPI2_ReadWriteByte(0xFF);  // 传输一个空字节
}

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef GPIO_Initure;
    RCC_PeriphCLKInitTypeDef SPI2ClkInit;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE();

    // SPI2 时钟源设为 PLL1Q
    SPI2ClkInit.PeriphClockSelection = RCC_PERIPHCLK_SPI2;
    SPI2ClkInit.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
    HAL_RCCEx_PeriphCLKConfig(&SPI2ClkInit);

    // PB13=SCK, PB15=MOSI
    // 注意：PB14 是板子 SDO 丝印，但现在接屏幕 RES，不能配置成 SPI2_MISO
    GPIO_Initure.Pin = GPIO_PIN_13 | GPIO_PIN_15;
    GPIO_Initure.Mode = GPIO_MODE_AF_PP;
    GPIO_Initure.Pull = GPIO_PULLUP;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_Initure.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);
}

void SPI2_SetSpeed(u32 SPI_BaudRatePrescaler)
{
    assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));
    __HAL_SPI_DISABLE(&SPI2_Handler);
    SPI2_Handler.Instance->CFG1 &= ~(0x7 << 28);
    SPI2_Handler.Instance->CFG1 |= SPI_BaudRatePrescaler;
    __HAL_SPI_ENABLE(&SPI2_Handler);
}

u8 SPI2_ReadWriteByte(u8 TxData)
{
    HAL_SPI_Transmit(&SPI2_Handler, &TxData, 1, 1000);
    return 0;
}
