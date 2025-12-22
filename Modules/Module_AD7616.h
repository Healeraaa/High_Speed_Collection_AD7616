#ifndef __MODULE_AD7616_H__
#define __MODULE_AD7616_H__

#include "main.h"
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

// AD7616 控制引脚（根据实际硬件连接修改）
#define AD7616_RESET_PIN            GPIO_PIN_0
#define AD7616_RESET_PORT           GPIOA
#define AD7616_CONVST_PIN           GPIO_PIN_1
#define AD7616_CONVST_PORT          GPIOA
#define AD7616_BUSY_PIN             GPIO_PIN_2
#define AD7616_BUSY_PORT            GPIOA

// 控制引脚操作宏
#define AD7616_RESET_LOW()          HAL_GPIO_WritePin(AD7616_RESET_PORT, AD7616_RESET_PIN, GPIO_PIN_RESET)
#define AD7616_RESET_HIGH()         HAL_GPIO_WritePin(AD7616_RESET_PORT, AD7616_RESET_PIN, GPIO_PIN_SET)
#define AD7616_CONVST_LOW()         HAL_GPIO_WritePin(AD7616_CONVST_PORT, AD7616_CONVST_PIN, GPIO_PIN_RESET)
#define AD7616_CONVST_HIGH()        HAL_GPIO_WritePin(AD7616_CONVST_PORT, AD7616_CONVST_PIN, GPIO_PIN_SET)
#define AD7616_READ_BUSY()          HAL_GPIO_ReadPin(AD7616_BUSY_PORT, AD7616_BUSY_PIN)

// ========================================================================== 函数声明 ==========================================================================

// 初始化和配置
HAL_StatusTypeDef AD7616_Init(void);
HAL_StatusTypeDef AD7616_Reset(void);
HAL_StatusTypeDef AD7616_SetRange(uint8_t channel, AD7616_Range_TypeDef range);
HAL_StatusTypeDef AD7616_SetMode(AD7616_Mode_TypeDef mode);

// 寄存器读写
HAL_StatusTypeDef AD7616_WriteReg(uint8_t reg_addr, uint16_t data);
uint16_t AD7616_ReadReg(uint8_t reg_addr);

// 数据采集
HAL_StatusTypeDef AD7616_StartConversion(void);
bool AD7616_IsBusy(void);
HAL_StatusTypeDef AD7616_WaitConversionDone(uint32_t timeout_ms);
uint16_t AD7616_ReadChannel(uint8_t channel);
HAL_StatusTypeDef AD7616_ReadAllChannels(uint16_t *pData);

// 批量采集（DMA）
HAL_StatusTypeDef AD7616_StartBatchConversion(uint16_t *pBuffer, uint32_t sample_count);
HAL_StatusTypeDef AD7616_ReadBatch_DMA(uint32_t psram_addr, uint32_t sample_count);

// 工具函数
float AD7616_ConvertToVoltage(uint16_t adc_value, AD7616_Range_TypeDef range);
int32_t AD7616_ConvertToMilliVolt(uint16_t adc_value, AD7616_Range_TypeDef range);

#endif /* __MODULE_AD7616_H__ */
