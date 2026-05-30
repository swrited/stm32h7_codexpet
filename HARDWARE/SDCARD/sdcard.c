#include "sdcard.h"

SD_HandleTypeDef hsd1;

u8 SDCard_Init(void)
{
    hsd1.Instance = SDMMC1;
    hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;      // 初始化先用 1-bit
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv = 8;                     // 先慢一点，稳定优先

    if (HAL_SD_Init(&hsd1) != HAL_OK)
    {
        return 1;
    }

    /* 切到 4-bit 模式 */
    if (HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B) != HAL_OK)
    {
        return 2;
    }

    return 0;
}

u8 SDCard_ReadBlocks(u8 *buf, u32 block_addr, u32 block_count)
{
    if (HAL_SD_ReadBlocks(&hsd1, buf, block_addr, block_count, 5000) != HAL_OK)
    {
        return 1;
    }

    /* 等待传输完成 */
    while (HAL_SD_GetCardState(&hsd1) != HAL_SD_CARD_TRANSFER)
    {
    }

    return 0;
}

void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef GPIO_Initure;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

    if (hsd->Instance == SDMMC1)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();
        __HAL_RCC_SDMMC1_CLK_ENABLE();

        /* SDMMC1 时钟源 */
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SDMMC;
        PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
        HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

        /* PC8/9/10/11/12 -> D0/D1/D2/D3/CLK */
        GPIO_Initure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_Initure.Mode = GPIO_MODE_AF_PP;
        GPIO_Initure.Pull = GPIO_PULLUP;
        GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_Initure.Alternate = GPIO_AF12_SDMMC1;
        HAL_GPIO_Init(GPIOC, &GPIO_Initure);

        /* PD2 -> CMD */
        GPIO_Initure.Pin = GPIO_PIN_2;
        GPIO_Initure.Mode = GPIO_MODE_AF_PP;
        GPIO_Initure.Pull = GPIO_PULLUP;
        GPIO_Initure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_Initure.Alternate = GPIO_AF12_SDMMC1;
        HAL_GPIO_Init(GPIOD, &GPIO_Initure);
    }
}
