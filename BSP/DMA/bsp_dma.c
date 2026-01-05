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

void BSP_DMA_TIM3_Init(void)
{
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

  NVIC_SetPriority(DMA1_Stream0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
}

void BSP_DMA_TIM3_Start(uint16_t *Databuff, uint32_t Datasize)
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