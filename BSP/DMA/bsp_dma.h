#ifndef __BSP_DMA_H__
#define __BSP_DMA_H__

/* Includes ------------------------------------------------------------------*/
#include "main.h"
void BSP_DMA_SetMemoryAddress(DMA_TypeDef *DMAx, uint32_t Stream, uint32_t MemoryAddress,uint16_t Datasize);
void BSP_DMA_SetPeriphAddress(DMA_TypeDef *DMAx, uint32_t Stream, uint32_t PeriphAddress);

void BSP_DMA_TIM3_Init(void);
void BSP_DMA_TIM3_Start(uint16_t *Databuff, uint32_t Datasize);

#endif 
