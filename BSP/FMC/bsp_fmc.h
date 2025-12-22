#ifndef __BSP_FMC_H
#define __BSP_FMC_H

#include "main.h"

// ========================================================================== PSRAM 配置参数 ==========================================================================

#define PSRAM_BASE_ADDR         0x60000000U     // FMC Bank1 Sector1 起始地址
#define PSRAM_SIZE              (2 * 1024 * 1024)  // PSRAM 大小（根据实际芯片修改，例如 2MB）

// ========================================================================== 函数声明 ==========================================================================

// 初始化和反初始化
HAL_StatusTypeDef BSP_FMC_PSRAM_Init(void);
HAL_StatusTypeDef BSP_FMC_PSRAM_DeInit(void);

// 基本读写接口（8/16/32位）
uint8_t BSP_FMC_PSRAM_ReadByte(uint32_t address);
void BSP_FMC_PSRAM_WriteByte(uint32_t address, uint8_t data);
uint16_t BSP_FMC_PSRAM_ReadHalfWord(uint32_t address);
void BSP_FMC_PSRAM_WriteHalfWord(uint32_t address, uint16_t data);
uint32_t BSP_FMC_PSRAM_ReadWord(uint32_t address);
void BSP_FMC_PSRAM_WriteWord(uint32_t address, uint32_t data);

// 缓冲区读写接口
HAL_StatusTypeDef BSP_FMC_PSRAM_ReadBuffer(uint32_t address, uint8_t *pBuffer, uint32_t length);
HAL_StatusTypeDef BSP_FMC_PSRAM_WriteBuffer(uint32_t address, uint8_t *pBuffer, uint32_t length);
HAL_StatusTypeDef BSP_FMC_PSRAM_ReadBuffer_16b(uint32_t address, uint16_t *pBuffer, uint32_t length);
HAL_StatusTypeDef BSP_FMC_PSRAM_WriteBuffer_16b(uint32_t address, uint16_t *pBuffer, uint32_t length);
HAL_StatusTypeDef BSP_FMC_PSRAM_ReadBuffer_32b(uint32_t address, uint32_t *pBuffer, uint32_t length);
HAL_StatusTypeDef BSP_FMC_PSRAM_WriteBuffer_32b(uint32_t address, uint32_t *pBuffer, uint32_t length);

// DMA 读写接口
HAL_StatusTypeDef BSP_FMC_PSRAM_ReadBuffer_DMA(uint32_t address, uint8_t *pBuffer, uint32_t length);
HAL_StatusTypeDef BSP_FMC_PSRAM_WriteBuffer_DMA(uint32_t address, uint8_t *pBuffer, uint32_t length);


#endif /* __BSP_FMC_H */
