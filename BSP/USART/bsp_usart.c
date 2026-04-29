#include "bsp_usart.h"
#include "stdio.h"

// ========================================================================== USART1 ==========================================================================

/**
 * @brief  初始化 USART1 (115200-8-N-1)
 * @note   时钟源: PCLK2 (120MHz), GPIO: PA9(TX), PA10(RX)
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
  PA10   ------> USART1_RX
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_9|LL_GPIO_PIN_10;          // 配置 PA9 和 PA10 引脚
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;               // 设置为复用功能模式
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;              // 设置低速输出
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;        // 设置推挽输出
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;                      // 无上下拉
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;                    // 复用功能 7 (USART1)
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);                       // 初始化 GPIO

  NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
  NVIC_EnableIRQ(USART1_IRQn);

  USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;              // 预分频器设置为 1
  USART_InitStruct.BaudRate = 3000000;                                     // 波特率 115200
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;                     // 数据位 8 位
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;                        // 停止位 1 位
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;                         // 无校验位
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;          // 收发模式
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;         // 无硬件流控
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;               // 16 倍过采样
  LL_USART_Init(USART1, &USART_InitStruct);                               // 初始化 USART1
  LL_USART_SetTXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);        // 设置发送 FIFO 阈值为 1/8
  LL_USART_SetRXFIFOThreshold(USART1, LL_USART_FIFOTHRESHOLD_1_8);        // 设置接收 FIFO 阈值为 1/8
  LL_USART_DisableFIFO(USART1);                                           // 禁用 FIFO
  LL_USART_ConfigAsyncMode(USART1);                                       // 配置为异步模式

  LL_USART_Enable(USART1);                                                // 使能 USART1

  LL_USART_EnableIT_RXNE(USART1);

  /* Polling USART1 initialisation */
  while((!(LL_USART_IsActiveFlag_TEACK(USART1))) || (!(LL_USART_IsActiveFlag_REACK(USART1))))  // 轮询等待 USART1 发送和接收使能确认标志
  {
  }
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

/**
 * @brief  重定向 fgetc 函数到 USART1 (scanf 输入)
 * @param  f: 文件指针
 * @retval 接收的字符
 */
int fgetc(FILE *f)
{
  while(!LL_USART_IsActiveFlag_RXNE_RXFNE(USART1));  // 等待接收数据
  return (int)LL_USART_ReceiveData8(USART1);         // 返回接收数据
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

int _read(int file, char *ptr, int len)
{
  for(int i = 0; i < len; i++)
  {
    while(!LL_USART_IsActiveFlag_RXNE_RXFNE(USART1));
    ptr[i] = (char)LL_USART_ReceiveData8(USART1);
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
