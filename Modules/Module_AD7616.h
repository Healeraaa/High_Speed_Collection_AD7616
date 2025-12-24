#ifndef __MODULE_AD7616_H__
#define __MODULE_AD7616_H__

#include "main.h"
#include "Module.h"
#include "stdbool.h"

// ========================================================================== AD7616 配置参数 ==========================================================================

// AD7616 在 PSRAM 地址空间中的映射地址（根据硬件设计修改）
#define AD7616_BASE_ADDR        (PSRAM_BASE_ADDR)           // AD7616 基地址
#define AD7616_REG_ADDR         (AD7616_BASE_ADDR)          // 寄存器地址（A0=0）
#define AD7616_DATA_ADDR        (AD7616_BASE_ADDR + 2)      // 数据地址（A0=1）

// AD7616 通道数
#define AD7616_CHANNEL_NUM      16                          // 16 个通道

// AD7616 量程配置（根据硬件跳线设置）
typedef enum {
    AD7616_RANGE_2V5 = 0,   // ±2.5V
    AD7616_RANGE_5V  = 1,   // ±5V
    AD7616_RANGE_10V = 2    // ±10V
} AD7616_Range_TypeDef;

// AD7616 工作模式
typedef enum {
    AD7616_MODE_NORMAL = 0,     // 正常模式（全速采样）
    AD7616_MODE_STANDBY = 1,    // 待机模式（低功耗）
    AD7616_MODE_POWERDOWN = 2   // 关断模式（最低功耗）
} AD7616_Mode_TypeDef;

// AD7616 寄存器地址
#define AD7616_REG_CONFIG           0x02    // 配置寄存器
#define AD7616_REG_CHANNEL          0x03    // 通道选择寄存器
#define AD7616_REG_RANGE_A          0x04    // 通道 A0-A7 量程配置
#define AD7616_REG_RANGE_B          0x05    // 通道 B0-B7 量程配置



// ========================================================================== 函数声明 ==========================================================================

// 初始化和配置
Module_Status_t Module_AD7616_Config(void);
Module_Status_t Module_AD7616_SetRange(uint8_t channel, AD7616_Range_TypeDef range);
Module_Status_t Module_AD7616_SetMode(AD7616_Mode_TypeDef mode);

// 寄存器读写
Module_Status_t Module_AD7616_WriteReg(uint8_t reg_addr, uint16_t data);
uint16_t Module_AD7616_ReadReg(uint8_t reg_addr);

// 数据采集
Module_Status_t Module_AD7616_StartConversion(void);
uint16_t Module_AD7616_ReadChannel(uint8_t channel);
Module_Status_t Module_AD7616_ReadAllChannels(uint16_t *pData);

// 批量采集
Module_Status_t Module_AD7616_StartBatchConversion(uint16_t *pBuffer, uint32_t sample_count);
Module_Status_t AD7616_ReadBatch_DMA(uint32_t psram_addr, uint32_t sample_count);

// 工具函数
float Module_AD7616_ConvertToVoltage(uint16_t adc_value, AD7616_Range_TypeDef range);
int32_t Module_AD7616_ConvertToMilliVolt(uint16_t adc_value, AD7616_Range_TypeDef range);


#endif /* __MODULE_AD7616_H__ */
