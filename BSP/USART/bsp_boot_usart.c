#include "bsp_boot_usart.h"

/*
 * Boot专用串口驱动
 * ===============
 *
 * 这里只使用USART1轮询收发，不使用中断、DMA、FreeRTOS或HAL UART句柄。
 * 好处是Boot路径依赖少、状态简单；代价是收发期间CPU会一直等待。
 * Boot本来就只负责升级，因此这个取舍合适。
 */

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

    /*
     * USART1位于APB2总线。先选择PCLK2为外设时钟源，再开启USART1和
     * GPIOA时钟，否则后面的寄存器配置不会生效。
     */
    LL_RCC_SetUSARTClockSource(LL_RCC_USART16_CLKSOURCE_PCLK2);

    LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

    LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);

    /* PA9 -> USART1_TX，PA10 -> USART1_RX，二者使用复用功能AF7。 */
    GPIO_InitStruct.Pin = LL_GPIO_PIN_9 | LL_GPIO_PIN_10;

    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;

    /* 高速IO边沿更适合115200波特率；TX使用推挽输出，无外部流控。 */
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;

    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;

    GPIO_InitStruct.Alternate = LL_GPIO_AF_7;

    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 配置前先关闭USART；整个Boot不配置NVIC，也不开RXNE中断。 */
    LL_USART_Disable(USART1);

    /* 115200波特率、8数据位、无校验、1停止位，即115200-8-N-1。 */
    USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;

    USART_InitStruct.BaudRate = 115200U;

    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;

    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;

    USART_InitStruct.Parity = LL_USART_PARITY_NONE;

    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;

    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;

    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;

    LL_USART_Init(USART1, &USART_InitStruct);

    /*
     * 先给出FIFO阈值，再关闭FIFO，使用最直接的单字节TXE/RXNE标志。
     * 这样后面的轮询函数不需要维护FIFO状态。
     */
    LL_USART_SetTXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);

    LL_USART_SetRXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);

    LL_USART_DisableFIFO(USART1);
    LL_USART_ConfigAsyncMode(USART1);

    LL_USART_Enable(USART1);

    /* 等待硬件确认发送器和接收器真正启用，之后才允许收发。 */
    while ((!LL_USART_IsActiveFlag_TEACK(USART1)) || (!LL_USART_IsActiveFlag_REACK(USART1)))
    {
    }

    /* 清除初始化前后可能残留的溢出、帧格式和噪声错误。 */
    LL_USART_ClearFlag_ORE(USART1);
    LL_USART_ClearFlag_FE(USART1);
    LL_USART_ClearFlag_NE(USART1);
}

/**
 * @brief 轮询发送一个字节
 */
void BSP_BOOT_USART1_SendByte(uint8_t data)
{
    /* TXE/TXFNF=1表示发送数据寄存器可以接受下一个字节。 */
    while (!LL_USART_IsActiveFlag_TXE_TXFNF(USART1))
    {
    }

    LL_USART_TransmitData8(USART1, data);

    /* TC=1才表示最后一个停止位也已经真正离开TX引脚。 */
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

    /*
     * 每写一个字节只等待发送寄存器空，不等待整字节完全发完，效率比
     * 循环调用SendByte更高。
     */
    for (index = 0U; index < length; index++)
    {
        while (!LL_USART_IsActiveFlag_TXE_TXFNF(USART1))
        {
        }

        LL_USART_TransmitData8(USART1, data[index]);
    }

    /* 整个缓冲区写完后只等待一次TC，确保完整帧已经发送完毕。 */
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

    /* HAL Tick当前由工程的TIM6时基提供，单位为毫秒。 */
    start_tick = HAL_GetTick();

    /* RXNE/RXFNE=1表示接收寄存器中已经有一个可读取字节。 */
    while (!LL_USART_IsActiveFlag_RXNE_RXFNE(USART1))
    {
        /*
         * 轮询等待期间主动清除接收错误，避免错误标志长期阻塞后续数据。
         * ORE：上一个字节未及时读取；FE：停止位错误；NE：线路噪声。
         */
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

        /* 无符号减法可正确处理32位Tick自然回绕。 */
        if ((HAL_GetTick() - start_tick) >= timeout_ms)
        {
            return 0U;
        }
    }

    /* 读取RDR，同时清除本次RXNE状态。 */
    *data = LL_USART_ReceiveData8(USART1);

    return 1U;
}
