#include "bsp_dma.h"



void BSP_DMA_SetMemoryAddress(DMA_TypeDef *DMAx, uint32_t Stream, uint32_t MemoryAddress,uint16_t Datasize)
{
  LL_DMA_SetMemoryAddress(DMAx, Stream, MemoryAddress);
  LL_DMA_SetDataLength(DMAx, Stream, Datasize);
}

void BSP_DMA_SetPeriphAddress(DMA_TypeDef *DMAx, uint32_t Stream, uint32_t PeriphAddress)
{
  LL_DMA_SetPeriphAddress(DMAx, Stream, PeriphAddress);
}

void BSP_DMA_AD7616_Init(void)
{
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);                           // 使能 DMA1 时钟
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_STREAM_0, LL_DMAMUX1_REQ_GENERATOR0);   // 设置 DMA 请求源为 DMAMUX Generator 0
  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_STREAM_0, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);  // 数据传输方向：外设到内存
  LL_DMA_SetStreamPriorityLevel(DMA1, LL_DMA_STREAM_0, LL_DMA_PRIORITY_VERYHIGH);  // DMA 优先级：最高
  LL_DMA_SetMode(DMA1, LL_DMA_STREAM_0, LL_DMA_MODE_NORMAL);                    // 传输模式：单次传输（非循环）
  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_STREAM_0, LL_DMA_PERIPH_NOINCREMENT);   // 外设地址不自增（固定地址读取 FMC）
  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_0, LL_DMA_MEMORY_INCREMENT);     // 内存地址自增（数据依次存入缓冲区）
  LL_DMA_SetPeriphSize(DMA1, LL_DMA_STREAM_0, LL_DMA_PDATAALIGN_HALFWORD);     // 外设数据宽度：16-bit（匹配 AD7616 输出）
  LL_DMA_SetMemorySize(DMA1, LL_DMA_STREAM_0, LL_DMA_MDATAALIGN_HALFWORD);     // 内存数据宽度：16-bit
  LL_DMA_EnableFifoMode(DMA1, LL_DMA_STREAM_0);                                 // 使能 FIFO 模式（提升传输效率）
  LL_DMA_SetFIFOThreshold(DMA1, LL_DMA_STREAM_0, LL_DMA_FIFOTHRESHOLD_FULL);   // FIFO 阈值：满（4个字）
  LL_DMA_SetMemoryBurstxfer(DMA1, LL_DMA_STREAM_0, LL_DMA_MBURST_SINGLE);      // 内存突发传输：单次（16-bit 对齐限制）
  LL_DMA_SetPeriphBurstxfer(DMA1, LL_DMA_STREAM_0, LL_DMA_PBURST_SINGLE);      // 外设突发传输：单次
  LL_DMAMUX_SetRequestSignalID(DMAMUX1, LL_DMAMUX_REQ_GEN_0, LL_DMAMUX1_REQ_GEN_EXTI0);  // DMAMUX 触发源：EXTI0（PE0 BUSY 信号）
  LL_DMAMUX_SetRequestGenPolarity(DMAMUX1, LL_DMAMUX_REQ_GEN_0, LL_DMAMUX_REQ_GEN_POL_FALLING);  // DMAMUX 触发极性：下降沿
  LL_DMAMUX_SetGenRequestNb(DMAMUX1, LL_DMAMUX_REQ_GEN_0, 2);                  // 每次 EXTI0 事件生成 2 个 DMA 请求（对应 A/B 两路数据）
  LL_DMAMUX_EnableRequestGen(DMAMUX1, LL_DMAMUX_REQ_GEN_0);  // 使能 DMAMUX 请求生成器
  NVIC_SetPriority(DMA1_Stream0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));  // 设置中断优先级（低于关键实时任务）
  

}

void BSP_DMA_AD7616_Start(uint16_t *Databuff, uint32_t Datasize)
{
    /* 1. 停止当前DMA传输 */
    LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_0); 
    while(LL_DMA_IsEnabledStream(DMA1, LL_DMA_STREAM_0)) {}
    
    /* 2. 清除DMA标志位 */
    LL_DMA_ClearFlag_TE0(DMA1);
    LL_DMA_ClearFlag_TC0(DMA1);
    LL_DMA_ClearFlag_DME0(DMA1);
    LL_DMA_ClearFlag_FE0(DMA1);
    LL_DMA_ClearFlag_HT0(DMA1);
    
    /* 4. 配置DMA */
    LL_DMA_SetMode(DMA1, LL_DMA_STREAM_0, LL_DMA_MODE_NORMAL);
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_STREAM_0, 0x60000000U); // 外设地址：FMC PSRAM 起始地址
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_0, (uint32_t)Databuff);
    LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_0, Datasize);
    
    // 使能DMA传输完成中断（只需要监控一个流完成即可）
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_STREAM_0);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_STREAM_0);
    NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    
    /* 6. 启动传输 */
    LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_0);  
    LL_TIM_EnableDMAReq_CC2(TIM3);         // 使能CC2事件DMA请求
}