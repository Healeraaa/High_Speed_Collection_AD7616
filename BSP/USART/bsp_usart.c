#include "bsp_usart.h"
#include "stdio.h"

// ========================================================================== USART1 ==========================================================================

/*
 * USART1接收中断环形缓冲区
 * ========================
 * 中断函数只负责把RDR中的字节放进这里；Modules层在任务环境中取走字节。
 * 这种“中断写、任务读”的单生产者/单消费者模型不需要在每次访问时关中断。
 */
#define BSP_USART1_RX_BUFFER_SIZE  128U

static volatile uint8_t  s_usart1_rx_buffer[BSP_USART1_RX_BUFFER_SIZE];
static volatile uint16_t s_usart1_rx_write_index;
static volatile uint16_t s_usart1_rx_read_index;
static volatile uint32_t s_usart1_rx_drop_count;

/**
 * @brief  初始化 USART1 系统命令和调试串口 (115200-8-N-1)
 * @note   时钟源: PCLK2 (120MHz), GPIO: PA9(TX)、PA10(RX)
 * @param  None
 * @retval None
 */
void BSP_USART1_Init(void)
{
  LL_USART_InitTypeDef USART_InitStruct = {0};    // USART 初始化结构体
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};       // GPIO 初始化结构体

  LL_RCC_SetUSARTClockSource(LL_RCC_USART16_CLKSOURCE_PCLK2);  // 设置 USART1 时钟源为 PCLK2 (120MHz)
  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);        // 使能 USART1 时钟
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);         // 使能 GPIOA 时钟
  
  /**USART1 GPIO Configuration
  PA9   ------> USART1_TX
  PA10  ------> USART1_RX
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_9 | LL_GPIO_PIN_10;         // 同时配置TX和RX
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;               // 设置为复用功能模式
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;              // 设置低速输出
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;        // 设置推挽输出
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;                      // 无上下拉
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;                    // 复用功能 7 (USART1)
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);                       // 初始化 GPIO

  USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;              // 预分频器设置为 1
  USART_InitStruct.BaudRate = 115200;                                     // 与Bootloader保持一致
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;                     // 数据位 8 位
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;                        // 停止位 1 位
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;                         // 无校验位
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;           // 同时允许发送和接收
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;         // 无硬件流控
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;               // 16 倍过采样
  LL_USART_Init(USART1, &USART_InitStruct);                               // 初始化 USART1
  LL_USART_SetTXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);        // 设置发送 FIFO 阈值为 1/8
  LL_USART_SetRXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);        // 设置接收 FIFO 阈值为 1/8
  LL_USART_DisableFIFO(USART1);                                           // 禁用 FIFO
  LL_USART_ConfigAsyncMode(USART1);                                       // 配置为异步模式

  /* 每次初始化都从空缓冲区开始。 */
  s_usart1_rx_write_index = 0U;
  s_usart1_rx_read_index = 0U;
  s_usart1_rx_drop_count = 0U;

  LL_USART_Enable(USART1);                                                // 使能 USART1

  /* Polling USART1 initialisation */
  while((!LL_USART_IsActiveFlag_TEACK(USART1)) ||
        (!LL_USART_IsActiveFlag_REACK(USART1)))  // 等待发送和接收使能确认
  {
  }

  /* 清除初始化前线路上可能残留的错误，避免一开中断就进入异常分支。 */
  LL_USART_ClearFlag_ORE(USART1);
  LL_USART_ClearFlag_FE(USART1);
  LL_USART_ClearFlag_NE(USART1);

  /*
   * 本中断不调用FreeRTOS API，优先级10低于采集、DMA和USB中断，
   * 防止普通文本命令影响实时采样。
   */
  NVIC_SetPriority(
      USART1_IRQn,
      NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 10U, 0U));
  NVIC_ClearPendingIRQ(USART1_IRQn);
  NVIC_EnableIRQ(USART1_IRQn);
  LL_USART_EnableIT_RXNE_RXFNE(USART1);
}

/**
 * @brief  USART1 发送单字节数据 (等待发送完成)
 * @param  ch: 待发送的字节
 * @retval 发送的字节
 */
uint8_t BSP_USART1_SendByte(uint8_t ch)
{
  while(!LL_USART_IsActiveFlag_TXE_TXFNF(USART1));  // 等待发送数据寄存器空
  LL_USART_TransmitData8(USART1, ch);               // 发送数据
  while(!LL_USART_IsActiveFlag_TC(USART1));         // 等待发送完成 (新增)
  return ch;
}

/**
 * @brief  USART1阻塞发送一段连续数据
 * @note   每个字节只等待TXE，整段数据最后才等待一次TC，效率高于逐字节
 *         调用BSP_USART1_SendByte()。
 */
void BSP_USART1_SendBuffer(const uint8_t *data, uint16_t length)
{
  uint16_t index;

  if ((data == NULL) || (length == 0U))
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

  /* TC=1表示最后一个停止位已经真正离开TX引脚。 */
  while (!LL_USART_IsActiveFlag_TC(USART1))
  {
  }
}

uint8_t BSP_USART1_ReadByte(uint8_t *data)
{
  uint16_t read_index;

  if (data == NULL)
  {
    return 0U;
  }

  read_index = s_usart1_rx_read_index;

  /* 读写下标相同表示中断尚未放入新的字节。 */
  if (read_index == s_usart1_rx_write_index)
  {
    return 0U;
  }

  *data = s_usart1_rx_buffer[read_index];

  read_index++;
  if (read_index >= BSP_USART1_RX_BUFFER_SIZE)
  {
    read_index = 0U;
  }

  /* 最后更新读下标，通知中断这一个位置已经可以重新使用。 */
  s_usart1_rx_read_index = read_index;
  return 1U;
}

void BSP_USART1_IRQHandler(void)
{
  uint8_t received_byte;
  uint16_t write_index;
  uint16_t next_write_index;

  if ((LL_USART_IsActiveFlag_RXNE_RXFNE(USART1) != 0U) &&
      (LL_USART_IsEnabledIT_RXNE_RXFNE(USART1) != 0U))
  {
    /* 及时读取RDR会清除RXNE，防止同一个字节重复触发中断。 */
    received_byte = LL_USART_ReceiveData8(USART1);

    write_index = s_usart1_rx_write_index;
    next_write_index = (uint16_t)(write_index + 1U);
    if (next_write_index >= BSP_USART1_RX_BUFFER_SIZE)
    {
      next_write_index = 0U;
    }

    if (next_write_index != s_usart1_rx_read_index)
    {
      s_usart1_rx_buffer[write_index] = received_byte;

      /* 数据写完后再发布新下标，任务不会读到尚未写好的位置。 */
      __DMB();
      s_usart1_rx_write_index = next_write_index;
    }
    else
    {
      /* 缓冲区满时丢弃最新字节，同时保留统计值供调试。 */
      s_usart1_rx_drop_count++;
    }
  }

  /*
   * ORE表示任务/中断来不及读取导致硬件覆盖；FE和NE分别表示帧错误和
   * 噪声错误。这里清除标志，让后续正常字节仍能继续接收。
   */
  if (LL_USART_IsActiveFlag_ORE(USART1) != 0U)
  {
    LL_USART_ClearFlag_ORE(USART1);
    s_usart1_rx_drop_count++;
  }

  if (LL_USART_IsActiveFlag_FE(USART1) != 0U)
  {
    LL_USART_ClearFlag_FE(USART1);
  }

  if (LL_USART_IsActiveFlag_NE(USART1) != 0U)
  {
    LL_USART_ClearFlag_NE(USART1);
  }
}

uint32_t BSP_USART1_GetRxDropCount(void)
{
  return s_usart1_rx_drop_count;
}

/**
 * @brief  重定向 fputc 函数到 USART1 (printf 输出)
 * @note   适配 MDK-ARM (Keil) 编译器
 * @param  ch: 待发送字符
 * @param  f: 文件指针
 * @retval 发送的字符
 */
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)  // Keil MDK-ARM 编译器
int fputc(int ch, FILE *f)
{
  BSP_USART1_SendByte((uint8_t)ch);  // 发送字符到 USART1
  return ch;
}

#elif defined(__GNUC__)  // GCC 编译器 (STM32CubeIDE)
int _write(int file, char *ptr, int len)
{
  for(int i = 0; i < len; i++)
  {
    BSP_USART1_SendByte((uint8_t)ptr[i]);
  }
  return len;
}

#endif

// ========================================================================== USARTR3 ==========================================================================

/**
 * @brief  初始化 USART3 (115200-8-N-1)
 * @note   时钟源: PCLK1 (60MHz), GPIO: PC10(TX), PC11(RX)
 * @param  None
 * @retval None
 */
void BSP_USART3_Init(void)
{
  LL_USART_InitTypeDef USART_InitStruct = {0};    // USART 初始化结构体
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};       // GPIO 初始化结构体

  LL_RCC_SetUSARTClockSource(LL_RCC_USART234578_CLKSOURCE_PCLK1);  // 设置 USART3 时钟源为 PCLK1 (60MHz)
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);            // 使能 USART3 时钟
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOC);             // 使能 GPIOC 时钟
  
  /**USART3 GPIO Configuration
  PC10   ------> USART3_TX
  PC11   ------> USART3_RX
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_10|LL_GPIO_PIN_11;            // 配置 PC10 和 PC11 引脚
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;                  // 设置为复用功能模式
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;                 // 设置低速输出
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;           // 设置推挽输出
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;                         // 无上下拉
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;                       // 复用功能 7 (USART3)
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);                          // 初始化 GPIO

  USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;             // 预分频器设置为 1
  USART_InitStruct.BaudRate = 115200;                                    // 波特率 115200
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;                    // 数据位 8 位
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;                       // 停止位 1 位
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;                        // 无校验位
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;         // 收发模式
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;        // 无硬件流控
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;              // 16 倍过采样
  LL_USART_Init(USART3, &USART_InitStruct);                              // 初始化 USART3
  LL_USART_SetTXFIFOThreshold(USART3, LL_USART_FIFOTHRESHOLD_1_8);       // 设置发送 FIFO 阈值为 1/8
  LL_USART_SetRXFIFOThreshold(USART3, LL_USART_FIFOTHRESHOLD_1_8);       // 设置接收 FIFO 阈值为 1/8
  LL_USART_DisableFIFO(USART3);                                          // 禁用 FIFO
  LL_USART_ConfigAsyncMode(USART3);                                      // 配置为异步模式

  LL_USART_Enable(USART3);                                               // 使能 USART3

  /* Polling USART3 initialisation */
  while((!(LL_USART_IsActiveFlag_TEACK(USART3))) || (!(LL_USART_IsActiveFlag_REACK(USART3))))  // 轮询等待 USART3 发送和接收使能确认标志
  {
  }
}


/**
 * @brief  USART3 发送单字节数据 (等待发送完成)
 * @param  ch: 待发送的字节
 * @retval 发送的字节
 */
uint8_t BSP_USART3_SendByte(uint8_t ch)
{
  while(!LL_USART_IsActiveFlag_TXE_TXFNF(USART3));  // 等待发送数据寄存器空
  LL_USART_TransmitData8(USART3, ch);               // 发送数据
  while(!LL_USART_IsActiveFlag_TC(USART3));         // 等待发送完成 (新增)
  return ch;
}
