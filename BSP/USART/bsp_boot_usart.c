#include "bsp_boot_usart.h"

/**
 * @brief 初始化 Boot 专用 USART1
 * @note
 *  TX：PA9
 *  RX：PA10
 *  参数：115200-8-N-1
 *  模式：轮询收发，不使用中断
 */
void BSP_BOOT_USART1_Init(void)
{
    LL_USART_InitTypeDef USART_InitStruct = {0};
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* USART1 使用 PCLK2 */
    LL_RCC_SetUSARTClockSource(LL_RCC_USART16_CLKSOURCE_PCLK2);

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

    LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);

    /*
     * PA9  -> USART1_TX
     * PA10 -> USART1_RX
     */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_9 | LL_GPIO_PIN_10;

    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;

    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;

    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;

    GPIO_InitStruct.Alternate = LL_GPIO_AF_7;

    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*
     * 不配置 NVIC。
     * 不开启 RXNE 中断。
     * Boot 采用轮询收发。
     */
    LL_USART_Disable(USART1);

    USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;

    USART_InitStruct.BaudRate = 115200U;

    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;

    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;

    USART_InitStruct.Parity = LL_USART_PARITY_NONE;

    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;

    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;

    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;

    LL_USART_Init(USART1, &USART_InitStruct);

    LL_USART_SetTXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);

    LL_USART_SetRXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);

    LL_USART_DisableFIFO(USART1);
    LL_USART_ConfigAsyncMode(USART1);

    LL_USART_Enable(USART1);

    while ((!LL_USART_IsActiveFlag_TEACK(USART1)) || (!LL_USART_IsActiveFlag_REACK(USART1)))
    {
    }

    /* 清除可能残留的错误标志 */
    LL_USART_ClearFlag_ORE(USART1);
    LL_USART_ClearFlag_FE(USART1);
    LL_USART_ClearFlag_NE(USART1);
}

/**
 * @brief 轮询发送一个字节
 */
void BSP_BOOT_USART1_SendByte(uint8_t data)
{
    while (!LL_USART_IsActiveFlag_TXE_TXFNF(USART1))
    {
    }

    LL_USART_TransmitData8(USART1, data);

    while (!LL_USART_IsActiveFlag_TC(USART1))
    {
    }
}

/**
 * @brief 轮询发送一段数据
 */
void BSP_BOOT_USART1_SendBuffer(
    const uint8_t *data,
    uint32_t length)
{
    uint32_t index;

    if (data == NULL)
    {
        return;
    }

    for (index = 0U; index < length; index++)
    {
        while (!LL_USART_IsActiveFlag_TXE_TXFNF(USART1))
        {
        }

        LL_USART_TransmitData8(USART1, data[index]);
    }

    /* 整个缓冲区发完后只等待一次 TC */
    while (!LL_USART_IsActiveFlag_TC(USART1))
    {
    }
}

/**
 * @brief 带超时接收一个字节
 * @retval 1：收到数据；0：超时或参数错误
 */
uint8_t BSP_BOOT_USART1_ReceiveByte(
    uint8_t *data,
    uint32_t timeout_ms)
{
    uint32_t start_tick;

    if (data == NULL)
    {
        return 0U;
    }

    start_tick = HAL_GetTick();

    while (!LL_USART_IsActiveFlag_RXNE_RXFNE(USART1))
    {
        if (LL_USART_IsActiveFlag_ORE(USART1))
        {
            LL_USART_ClearFlag_ORE(USART1);
        }

        if (LL_USART_IsActiveFlag_FE(USART1))
        {
            LL_USART_ClearFlag_FE(USART1);
        }

        if (LL_USART_IsActiveFlag_NE(USART1))
        {
            LL_USART_ClearFlag_NE(USART1);
        }

        if ((HAL_GetTick() - start_tick) >= timeout_ms)
        {
            return 0U;
        }
    }

    *data = LL_USART_ReceiveData8(USART1);

    return 1U;
}