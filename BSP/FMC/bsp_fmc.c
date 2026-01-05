#include "bsp_fmc.h"
#include <string.h>

SRAM_HandleTypeDef bsp_fmc_psram_handle;                                        // SRAM 句柄结构体，存储 FMC PSRAM 的配置参数

// ========================================================================== 私有函数声明 ==========================================================================
static void BSP_FMC_PSRAM_MspInit(void);
static void BSP_FMC_PSRAM_MspDeInit(void);

/**
  * @brief  初始化 FMC 外设为 PSRAM 模式
  * @note   配置 FMC 的时序参数、数据总线宽度、读写模式等
  *         内存映射：
  *         - FMC Bank1 区域：0x60000000 ~ 0x6FFFFFFF (256MB)
  *         - 本配置使用 Bank1 Sector1：0x60000000 起始
  * @retval BSP_Status_t BSP_OK: 初始化成功, BSP_ERROR: 初始化失败
  */
BSP_Status_t BSP_FMC_PSRAM_Init(void)
{
  FMC_NORSRAM_TimingTypeDef Timing = {0};                                       // FMC 时序配置结构体（单位：HCLK 周期）


  // 配置 SRAM 外设基本参数
  bsp_fmc_psram_handle.Instance = FMC_NORSRAM_DEVICE;                           // FMC NORSRAM 设备基地址（Bank1 Sector1 寄存器）
  bsp_fmc_psram_handle.Extended = FMC_NORSRAM_EXTENDED_DEVICE;                  // FMC NORSRAM 扩展寄存器基地址（扩展时序模式）
  bsp_fmc_psram_handle.Init.NSBank = FMC_NORSRAM_BANK1;                         // 使用 Bank1 Sector1（地址：0x60000000）
  bsp_fmc_psram_handle.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;  // 禁用地址数据复用
  bsp_fmc_psram_handle.Init.MemoryType = FMC_MEMORY_TYPE_SRAM;             // 改为SRAM模式
  // bsp_fmc_psram_handle.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;       // 使能地址/数据复用（节省 GPIO，地址和数据共用引脚）
  // bsp_fmc_psram_handle.Init.MemoryType = FMC_MEMORY_TYPE_PSRAM;                 // 存储器类型：PSRAM（伪静态 RAM，内部自动刷新）
  bsp_fmc_psram_handle.Init.MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_16;    // 数据总线宽度：16 位（每次读写 2 字节）
  bsp_fmc_psram_handle.Init.BurstAccessMode = FMC_BURST_ACCESS_MODE_DISABLE;    // 禁用突发访问模式（PSRAM 不需要突发模式）
  bsp_fmc_psram_handle.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW; // 等待信号极性：低电平有效（NWAIT 引脚）
  bsp_fmc_psram_handle.Init.WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;       // 等待信号生效时机：在等待状态之前检测 NWAIT
  bsp_fmc_psram_handle.Init.WriteOperation = FMC_WRITE_OPERATION_ENABLE;        // 使能写操作（允许向 PSRAM 写入数据）
  bsp_fmc_psram_handle.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;               // 禁用等待信号（不使用 NWAIT 引脚，时序由软件控制）
  bsp_fmc_psram_handle.Init.ExtendedMode = FMC_EXTENDED_MODE_DISABLE;           // 禁用扩展模式（读写使用相同时序）
  // bsp_fmc_psram_handle.Init.ExtendedMode = FMC_EXTENDED_MODE_ENABLE;          // 使能扩展模式（读写使用不同时序，写入更快） ！！！使能扩展模式FMC_ACCESS_MODE_A才生效
  bsp_fmc_psram_handle.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;    // 不使能异步等待（在异步模式下插入等待状态）
  bsp_fmc_psram_handle.Init.WriteBurst = FMC_WRITE_BURST_DISABLE;               // 禁用写突发（写操作不使用突发模式）
  bsp_fmc_psram_handle.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;   // 连续时钟：仅同步模式（异步模式不需要）
  bsp_fmc_psram_handle.Init.WriteFifo = FMC_WRITE_FIFO_ENABLE;                  // 使能写 FIFO（内部 FIFO 缓存写数据，提高性能）
  bsp_fmc_psram_handle.Init.PageSize = FMC_PAGE_SIZE_NONE;                      // 页大小：无（PSRAM 不需要页模式）
  
  // 配置 FMC 时序参数-----1 HCLK = 4.17ns @ 240MHz
  Timing.AddressSetupTime = 12;        // 地址建立时间: 6 HCLK = 25.02ns @ 240MHz ------ t_RD_SETUP
  Timing.AddressHoldTime = 2;         // 地址保持时间: 1 HCLK = 8.34ns @ 240MHz 模式 A用不到此参数
  Timing.DataSetupTime = 12;          // 数据建立时间: 10 HCLK = 41.7ns @ 240MHz
  Timing.BusTurnAroundDuration = 5;   // 总线转换时间: 5 HCLK = 25.02ns
  Timing.CLKDivision = 2;             // 同步模式时钟分频 (异步模式不使用)
  Timing.DataLatency = 2;             // 同步模式数据延迟 (异步模式不使用)
  Timing.AccessMode = FMC_ACCESS_MODE_A;                                        // 访问模式：模式 A（标准异步模式）

  // 初始化底层硬件（GPIO 和时钟）
  BSP_FMC_PSRAM_MspInit();
  
  // 初始化 FMC SRAM 外设
  if (HAL_SRAM_Init(&bsp_fmc_psram_handle, &Timing, &Timing) != HAL_OK)           // 调用 HAL 库初始化函数
  {
    return BSP_ERROR;                                                           // 初始化失败
  }
  
  return BSP_OK;                                                                // 初始化成功
}

/**
  * @brief  FMC PSRAM 反初始化
  * @retval BSP_Status_t BSP_OK: 反初始化成功
  */
BSP_Status_t BSP_FMC_PSRAM_DeInit(void)
{
  HAL_StatusTypeDef status;
  
  // 反初始化 FMC SRAM 外设
  status = HAL_SRAM_DeInit(&bsp_fmc_psram_handle);
  
  // 反初始化底层硬件
  BSP_FMC_PSRAM_MspDeInit();
  if(status != HAL_OK)
  {
    return BSP_ERROR;
  }
  return BSP_OK;
}

// ========================================================================== 数据读写接口 ==========================================================================

/**
  * @brief  从 PSRAM 读取 8 位数据
  * @param  address: 读取地址（相对于 PSRAM 基地址的偏移）
  * @retval uint8_t 读取的数据
  */
uint8_t BSP_FMC_PSRAM_ReadByte(uint32_t address)
{
  return *(__IO uint8_t *)(PSRAM_BASE_ADDR + address);
}

/**
  * @brief  向 PSRAM 写入 8 位数据
  * @param  address: 写入地址（相对于 PSRAM 基地址的偏移）
  * @param  data: 要写入的数据
  * @retval None
  */
void BSP_FMC_PSRAM_WriteByte(uint32_t address, uint8_t data)
{
  *(__IO uint8_t *)(PSRAM_BASE_ADDR + address) = data;
}

/**
  * @brief  从 PSRAM 读取 16 位数据
  * @param  address: 读取地址（相对于 PSRAM 基地址的偏移，必须 2 字节对齐）
  * @retval uint16_t 读取的数据
  */
uint16_t BSP_FMC_PSRAM_ReadHalfWord(uint32_t address)
{
  return *(__IO uint16_t *)(PSRAM_BASE_ADDR + address);
}

/**
  * @brief  向 PSRAM 写入 16 位数据
  * @param  address: 写入地址（相对于 PSRAM 基地址的偏移，必须 2 字节对齐）
  * @param  data: 要写入的数据
  * @retval None
  */
void BSP_FMC_PSRAM_WriteHalfWord(uint32_t address, uint16_t data)
{
  *(__IO uint16_t *)(PSRAM_BASE_ADDR + address) = data;
  // __DSB();
  // HAL_SRAM_Write_16b(&bsp_fmc_psram_handle, 
  //                    (uint32_t *)(PSRAM_BASE_ADDR + address), 
  //                    &data, 
  //                    1); 
}


/**
  * @brief  从 PSRAM 读取 32 位数据
  * @param  address: 读取地址（相对于 PSRAM 基地址的偏移，必须 4 字节对齐）
  * @retval uint32_t 读取的数据
  */
uint32_t BSP_FMC_PSRAM_ReadWord(uint32_t address)
{
  return *(__IO uint32_t *)(PSRAM_BASE_ADDR + address);
}

/**
  * @brief  向 PSRAM 写入 32 位数据
  * @param  address: 写入地址（相对于 PSRAM 基地址的偏移，必须 4 字节对齐）
  * @param  data: 要写入的数据
  * @retval None
  */
void BSP_FMC_PSRAM_WriteWord(uint32_t address, uint32_t data)
{
  *(__IO uint32_t *)(PSRAM_BASE_ADDR + address) = data;
}

/**
  * @brief  从 PSRAM 读取数据缓冲区
  * @param  address: 读取地址（相对于 PSRAM 基地址的偏移）
  * @param  pBuffer: 数据缓冲区指针
  * @param  length: 读取长度（字节数）
  * @retval BSP_Status_t BSP_OK: 读取成功
  */
BSP_Status_t BSP_FMC_PSRAM_ReadBuffer(uint32_t address, uint8_t *pBuffer, uint32_t length)
{
  if (pBuffer == NULL || address + length > PSRAM_SIZE)
  {
    return BSP_ERROR;
  }

  if(HAL_SRAM_Read_8b(&bsp_fmc_psram_handle, 
                          (uint32_t *)(PSRAM_BASE_ADDR + address), 
                          pBuffer, 
                          length) == HAL_OK)
  {
    return BSP_OK;
  }          
  else
  {
    return BSP_ERROR;
  }

}

/**
  * @brief  向 PSRAM 写入数据缓冲区
  * @param  address: 写入地址（相对于 PSRAM 基地址的偏移）
  * @param  pBuffer: 数据缓冲区指针
  * @param  length: 写入长度（字节数）
  * @retval BSP_Status_t BSP_OK: 写入成功
  */
BSP_Status_t BSP_FMC_PSRAM_WriteBuffer(uint32_t address, uint8_t *pBuffer, uint32_t length)
{
  if (pBuffer == NULL || address + length > PSRAM_SIZE)
  {
    return BSP_ERROR;
  }

  if(HAL_SRAM_Write_8b(&bsp_fmc_psram_handle, 
                           (uint32_t *)(PSRAM_BASE_ADDR + address), 
                           pBuffer, 
                           length) == HAL_OK)
  {
    return BSP_OK;
  }          
  else
  {
    return BSP_ERROR;
  }
}

/**
  * @brief  从 PSRAM 读取 16 位数据缓冲区
  * @param  address: 读取地址（相对于 PSRAM 基地址的偏移，必须 2 字节对齐）
  * @param  pBuffer: 数据缓冲区指针
  * @param  length: 读取长度（16 位字数）
  * @retval BSP_Status_t BSP_OK: 读取成功
  */
BSP_Status_t BSP_FMC_PSRAM_ReadBuffer_16b(uint32_t address, uint16_t *pBuffer, uint32_t length)
{
  if (pBuffer == NULL || address + (length * 2) > PSRAM_SIZE)
  {
    return BSP_ERROR;
  }
  
  if(HAL_SRAM_Read_16b(&bsp_fmc_psram_handle, 
                           (uint32_t *)(PSRAM_BASE_ADDR + address), 
                           pBuffer, 
                           length) == HAL_OK)
  {
    return BSP_OK;
  }          
  else
  {
    return BSP_ERROR;
  }


}

/**
  * @brief  向 PSRAM 写入 16 位数据缓冲区
  * @param  address: 写入地址（相对于 PSRAM 基地址的偏移，必须 2 字节对齐）
  * @param  pBuffer: 数据缓冲区指针
  * @param  length: 写入长度（16 位字数）
  * @retval BSP_Status_t BSP_OK: 写入成功
  */
BSP_Status_t BSP_FMC_PSRAM_WriteBuffer_16b(uint32_t address, uint16_t *pBuffer, uint32_t length)
{
  if (pBuffer == NULL || address + (length * 2) > PSRAM_SIZE)
  {
    return BSP_ERROR;
  }
  
  if(HAL_SRAM_Write_16b(&bsp_fmc_psram_handle, 
                            (uint32_t *)(PSRAM_BASE_ADDR + address), 
                            pBuffer, 
                            length) == HAL_OK)
  {
    return BSP_OK;
  }          
  else
  {
    return BSP_ERROR;
  }
}

/**
  * @brief  从 PSRAM 读取 32 位数据缓冲区
  * @param  address: 读取地址（相对于 PSRAM 基地址的偏移，必须 4 字节对齐）
  * @param  pBuffer: 数据缓冲区指针
  * @param  length: 读取长度（32 位字数）
  * @retval BSP_Status_t BSP_OK: 读取成功
  */
BSP_Status_t BSP_FMC_PSRAM_ReadBuffer_32b(uint32_t address, uint32_t *pBuffer, uint32_t length)
{
  if (pBuffer == NULL || address + (length * 4) > PSRAM_SIZE)
  {
    return BSP_ERROR;
  }
  
  if(HAL_SRAM_Read_32b(&bsp_fmc_psram_handle, 
                           (uint32_t *)(PSRAM_BASE_ADDR + address), 
                           pBuffer, 
                           length) == HAL_OK)
  {
    return BSP_OK;
  }          
  else
  {
    return BSP_ERROR;
  }
}

/**
  * @brief  向 PSRAM 写入 32 位数据缓冲区
  * @param  address: 写入地址（相对于 PSRAM 基地址的偏移，必须 4 字节对齐）
  * @param  pBuffer: 数据缓冲区指针
  * @param  length: 写入长度（32 位字数）
  * @retval BSP_Status_t BSP_OK: 写入成功
  */
BSP_Status_t BSP_FMC_PSRAM_WriteBuffer_32b(uint32_t address, uint32_t *pBuffer, uint32_t length)
{
  if (pBuffer == NULL || address + (length * 4) > PSRAM_SIZE)
  {
    return BSP_ERROR;
  }
  
  if(HAL_SRAM_Write_32b(&bsp_fmc_psram_handle, 
                            (uint32_t *)(PSRAM_BASE_ADDR + address), 
                            pBuffer, 
                            length) == HAL_OK)
  {
    return BSP_OK;
  }          
  else
  {
    return BSP_ERROR;
  }
}

// ========================================================================== DMA 读写接口 ==========================================================================

/**
  * @brief  使用 DMA 从 PSRAM 读取数据
  * @param  address: 读取地址（相对于 PSRAM 基地址的偏移）
  * @param  pBuffer: 数据缓冲区指针
  * @param  length: 读取长度（字节数）
  * @retval BSP_Status_t BSP_OK: 读取启动成功
  */
BSP_Status_t BSP_FMC_PSRAM_ReadBuffer_DMA(uint32_t address, uint8_t *pBuffer, uint32_t length)
{
  if (pBuffer == NULL || address + length > PSRAM_SIZE)
  {
    return BSP_ERROR;
  }
  
  if(HAL_SRAM_Read_DMA(&bsp_fmc_psram_handle, 
                           (uint32_t *)(PSRAM_BASE_ADDR + address), 
                           (uint32_t *)pBuffer, 
                           length) == HAL_OK)
  {
    return BSP_OK;
  }          
  else
  {
    return BSP_ERROR;
  }
}

/**
  * @brief  使用 DMA 向 PSRAM 写入数据
  * @param  address: 写入地址（相对于 PSRAM 基地址的偏移）
  * @param  pBuffer: 数据缓冲区指针
  * @param  length: 写入长度（字节数）
  * @retval BSP_Status_t BSP_OK: 写入启动成功
  */
BSP_Status_t BSP_FMC_PSRAM_WriteBuffer_DMA(uint32_t address, uint8_t *pBuffer, uint32_t length)
{
  if (pBuffer == NULL || address + length > PSRAM_SIZE)
  {
    return BSP_ERROR;
  }
  
  if(HAL_SRAM_Write_DMA(&bsp_fmc_psram_handle, 
                            (uint32_t *)(PSRAM_BASE_ADDR + address), 
                            (uint32_t *)pBuffer, 
                            length) == HAL_OK)
  {
    return BSP_OK;
  }          
  else
  {
    return BSP_ERROR;
  }

}

// ========================================================================== FMC 底层初始化（GPIO 和时钟配置） ==========================================================================

static uint32_t FMC_Initialized = 0;                                            // FMC 初始化标志（0: 未初始化，1: 已初始化）

/**
  * @brief  FMC 底层初始化（GPIO 和时钟配置）
  * @note   配置 FMC 所需的 GPIO 引脚和使能 FMC 时钟
  * @retval None
  */
static void  BSP_FMC_PSRAM_MspInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};                                       // GPIO 初始化结构体

  if (FMC_Initialized) {                                                        // 检查是否已初始化
    return;                                                                     // 避免重复初始化导致的问题
  }
  FMC_Initialized = 1;                                                          // 设置初始化标志

  // 使能 GPIO 时钟
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  // 配置 FMC 时钟源
  LL_RCC_SetFMCClockSource(LL_RCC_FMC_CLKSOURCE_HCLK);                          // 设置 FMC 时钟源为 HCLK（AHB 时钟，通常 200MHz，最高性能）
  __HAL_RCC_FMC_CLK_ENABLE();                                                   // 使能 FMC 外设时钟（必须在配置 FMC 寄存器之前）

  /**
    * FMC GPIO 引脚功能说明（地址/数据复用模式）
    * 
    * 数据线 (16-bit):
    *   PE7   -> FMC_DA4   (复用地址 A4 / 数据 D4)
    *   PE8   -> FMC_DA5   (复用地址 A5 / 数据 D5)
    *   PE9   -> FMC_DA6   (复用地址 A6 / 数据 D6)
    *   PE10  -> FMC_DA7   (复用地址 A7 / 数据 D7)
    *   PE11  -> FMC_DA8   (复用地址 A8 / 数据 D8)                     
    *   PE12  -> FMC_DA9   (复用地址 A9 / 数据 D9)
    *   PE13  -> FMC_DA10  (复用地址 A10 / 数据 D10)
    *   PE14  -> FMC_DA11  (复用地址 A11 / 数据 D11)
    *   PE15  -> FMC_DA12  (复用地址 A12 / 数据 D12)
    *   PD8   -> FMC_DA13  (复用地址 A13 / 数据 D13)
    *   PD9   -> FMC_DA14  (复用地址 A14 / 数据 D14)
    *   PD10  -> FMC_DA15  (复用地址 A15 / 数据 D15)
    *   PD14  -> FMC_DA0   (复用地址 A0 / 数据 D0)
    *   PD15  -> FMC_DA1   (复用地址 A1 / 数据 D1) 
    *   PD0   -> FMC_DA2   (复用地址 A2 / 数据 D2)                                            
    *   PD1   -> FMC_DA3   (复用地址 A3 / 数据 D3)
    * 
    * 控制信号:
    *   PC7   -> FMC_NE1   (片选信号 Bank1 Sector1，低电平有效，选中 PSRAM)
    *   PD4   -> FMC_NOE   (读使能，Output Enable，低电平有效，PSRAM 输出数据)
    *   PD5   -> FMC_NWE   (写使能，Write Enable，低电平有效，PSRAM 接收数据)
    * 
    * 注：DA = Data/Address（地址/数据复用引脚，时分复用）
    */

  // 配置 GPIOE 引脚 (数据线 D4~D12)
  GPIO_InitStruct.Pin = GPIO_PIN_7  | GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10  // PE7~PE15：FMC_DA4 ~ FMC_DA12
                       | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
                       | GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;                                       // 模式：复用推挽输出（输出由 FMC 外设控制）
  GPIO_InitStruct.Pull = GPIO_NOPULL;                                           // 无上下拉（外部 PSRAM 芯片有内部上下拉电阻）
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;                            // 速度：极高速（200MHz，减小信号延迟和边沿时间）
  GPIO_InitStruct.Alternate = GPIO_AF12_FMC;                                    // 复用功能 12：FMC（查阅芯片数据手册确认）
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);                                       // 初始化 GPIOE 的 FMC 引脚

  // 配置 GPIOD 引脚 (数据线 D0~D3, D13~D15 + 控制信号 NOE, NWE)
  GPIO_InitStruct.Pin = GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 | GPIO_PIN_14  // PD8~PD10：FMC_DA13~DA15，PD14~PD15：FMC_DA0~DA1
                       | GPIO_PIN_15 | GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_4  // PD0~PD1：FMC_DA2~DA3，PD4：NOE，PD5：NWE
                       | GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;                                       // 模式：复用推挽输出
  GPIO_InitStruct.Pull = GPIO_NOPULL;                                           // 无上下拉
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;                            // 速度：极高速
  GPIO_InitStruct.Alternate = GPIO_AF12_FMC;                                    // 复用功能 12：FMC
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);                                       // 初始化 GPIOD 的 FMC 引脚

  // 配置 GPIOC 引脚 (控制信号 NE1)
  GPIO_InitStruct.Pin = GPIO_PIN_7;                                             //PC7：FMC_NE1（片选）
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;                                       // 模式：复用推挽输出
  GPIO_InitStruct.Pull = GPIO_NOPULL;                                           // 无上下拉
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;                            // 速度：极高速
  GPIO_InitStruct.Alternate = GPIO_AF9_FMC;                                     // 复用功能 9：FMC（注意：PC6/PC7 使用 AF9，而不是 AF12）
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);                                       // 初始化 GPIOC 的 FMC 引脚


  //   // GPIOE: D4-D12
  // GPIO_InitStruct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
  //                       GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
  //                       GPIO_PIN_15;
  // GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  // GPIO_InitStruct.Pull = GPIO_NOPULL;
  // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  // GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
  // HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  // // GPIOD: D0-D3, D13-D15
  // GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_8 | 
  //                       GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 | GPIO_PIN_15;
  // HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  // // ========== 控制信号 ==========
  // // PC7: NE1/CS（片选）
  // GPIO_InitStruct.Pin = GPIO_PIN_7;
  // GPIO_InitStruct.Alternate = GPIO_AF9_FMC;  // 注意：PC7使用AF9！
  // HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  // // PD4: NOE/RD（读使能）
  // GPIO_InitStruct.Pin = GPIO_PIN_4;
  // GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
  // HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  // // PD5: NWE/WR（写使能）
  // GPIO_InitStruct.Pin = GPIO_PIN_5;
  // HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}


// ========================================================================== FMC 反初始化（释放资源） ==========================================================================

static uint32_t FMC_DeInitialized = 0;                                          // FMC 反初始化标志（0: 未反初始化，1: 已反初始化）

/**
  * @brief  FMC 底层反初始化
  * @note   关闭 FMC 时钟，释放 GPIO 引脚
  * @retval None
  */
static void BSP_FMC_PSRAM_MspDeInit(void)
{
  if (FMC_DeInitialized) {                                                      // 检查是否已反初始化
    return;                                                                     // 避免重复反初始化
  }
  FMC_DeInitialized = 1;                                                        // 设置反初始化标志
  FMC_Initialized = 0;                                                          // 清除初始化标志

  __HAL_RCC_FMC_CLK_DISABLE();                                                  // 关闭 FMC 外设时钟（节省功耗）

  // 释放 GPIO 引脚（恢复为默认状态：模拟输入，浮空）
  HAL_GPIO_DeInit(GPIOE, GPIO_PIN_7  | GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 // 释放 GPIOE 的 FMC 引脚
                        | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
                        | GPIO_PIN_15);

  HAL_GPIO_DeInit(GPIOD, GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 | GPIO_PIN_14 // 释放 GPIOD 的 FMC 引脚
                        | GPIO_PIN_15 | GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_4
                        | GPIO_PIN_5);

  HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6 | GPIO_PIN_7);                              // 释放 GPIOC 的 FMC 引脚
}
