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

// AD7616 输入范围寄存器数量
#define AD7616_RANGE_RESGISTER_NUM       4               // 4 个输入范围寄存器（A1, A2, B1, B2）

// AD7616 量程配置
typedef enum {
    AD7616_RANGE_2V5 = 1,   // ±2.5V
    AD7616_RANGE_5V  = 2,   // ±5V
    AD7616_RANGE_10V = 3    // ±10V
} AD7616_Range_TypeDef;


// AD7616 寄存器地址
#define AD7616_REG_CONFIG           0x02    // 配置寄存器
#define AD7616_REG_CHANNEL          0x03    // 通道选择寄存器
#define AD7616_REG_RANGE_A1         0x04    // 通道 A0-A3 量程配置
#define AD7616_REG_RANGE_A2         0x05    // 通道 A4-A7 量程配置
#define AD7616_REG_RANGE_B1         0x06    // 通道 B0-B3 量程配置
#define AD7616_REG_RANGE_B2         0x07    // 通道 B4-B7 量程配置

// ========================================================================== 配置寄存器位定义 (Configuration Register) ==========================================================================

// CRCEN [0] R/W
#define AD7616_CONFIG_CRCEN_POS              0
#define AD7616_CONFIG_CRCEN_DISABLE         (0x00 << AD7616_CONFIG_CRCEN_POS)           // 禁用 CRC 校验
#define AD7616_CONFIG_CRCEN_ENABLE          (0x01 << AD7616_CONFIG_CRCEN_POS)           // 启用 CRC 校验

// STATUSEN [1] R/W
#define AD7616_CONFIG_STATUSEN_POS           1
#define AD7616_CONFIG_STATUSEN_DISABLE       (0x00 << AD7616_CONFIG_STATUSEN_POS)       // 禁用状态输出
#define AD7616_CONFIG_STATUSEN_ENABLE        (0x01 << AD7616_CONFIG_STATUSEN_POS)       // 启用状态输出

// OS [4:2] R/W
#define AD7616_CONFIG_OS_POS                 2
#define AD7616_CONFIG_OS_DISABLE             (0x00 << AD7616_CONFIG_OS_POS)             // 8x 过采样
#define AD7616_CONFIG_OS_2X                  (0x01 << AD7616_CONFIG_OS_POS)             // 2x 过采样
#define AD7616_CONFIG_OS_4X                  (0x02 << AD7616_CONFIG_OS_POS)             // 4x 过采样
#define AD7616_CONFIG_OS_8X                  (0x03 << AD7616_CONFIG_OS_POS)             // 8x 过采样
#define AD7616_CONFIG_OS_16X                 (0x04 << AD7616_CONFIG_OS_POS)             // 16x 过采样
#define AD7616_CONFIG_OS_32X                 (0x05 << AD7616_CONFIG_OS_POS)             // 32x 过采样
#define AD7616_CONFIG_OS_64X                 (0x06 << AD7616_CONFIG_OS_POS)             // 64x 过采样
#define AD7616_CONFIG_OS_128X                (0x07 << AD7616_CONFIG_OS_POS)             // 128x 过采样

// SEQEN [5] R/W
#define AD7616_CONFIG_SEQEN_POS              5
#define AD7616_CONFIG_SEQEN_DISABLE          (0x00 << AD7616_CONFIG_SEQEN_POS)           // 禁用通道序列器
#define AD7616_CONFIG_SEQEN_ENABLE           (0x01 << AD7616_CONFIG_SEQEN_POS)           // 启用通道序列器

// BURSTEN [6] R/W
#define AD7616_CONFIG_BURSTEN_POS            6
#define AD7616_CONFIG_BURSTEN_DISABLE        (0x00 << AD7616_CONFIG_BURSTEN_POS)        // 禁用突发模式
#define AD7616_CONFIG_BURSTEN_ENABLE         (0x01 << AD7616_CONFIG_BURSTEN_POS)        // 启用突发模式

// BURSTEN [7] R
#define AD7616_CONFIG_SDEF_POS               7
#define AD7616_CONFIG_SDEF_FAILED            (0x00 << AD7616_CONFIG_BURSTEN_POS)        // 自检失败        
#define AD7616_CONFIG_SDEF_PASSED            (0x01 << AD7616_CONFIG_BURSTEN_POS)        // 自检通过   

// ========================================================================== 通道寄存器定义 (Channel Register) ==========================================================================

// 通道 A [3:0] R/W
#define AD7616_CHANNEL_CHA_POS                0
#define AD7616_CHANNEL_CHA_A0                 (0x00 << AD7616_CHANNEL_CHA_POS)          // A0 
#define AD7616_CHANNEL_CHA_A1                 (0x01 << AD7616_CHANNEL_CHA_POS)          // A1
#define AD7616_CHANNEL_CHA_A2                 (0x02 << AD7616_CHANNEL_CHA_POS)          // A2
#define AD7616_CHANNEL_CHA_A3                 (0x03 << AD7616_CHANNEL_CHA_POS)          // A3
#define AD7616_CHANNEL_CHA_A4                 (0x04 << AD7616_CHANNEL_CHA_POS)          // A4
#define AD7616_CHANNEL_CHA_A5                 (0x05 << AD7616_CHANNEL_CHA_POS)          // A5
#define AD7616_CHANNEL_CHA_A6                 (0x06 << AD7616_CHANNEL_CHA_POS)          // A6
#define AD7616_CHANNEL_CHA_A7                 (0x07 << AD7616_CHANNEL_CHA_POS)          // A7
#define AD7616_CHANNEL_CHA_VCC                (0x08 << AD7616_CHANNEL_CHA_POS)          // VCC
#define AD7616_CHANNEL_CHA_ALDO               (0x09 << AD7616_CHANNEL_CHA_POS)          // ALDO
#define AD7616_CHANNEL_CHA_NONE               (0x0A << AD7616_CHANNEL_CHA_POS)          // Reserved
#define AD7616_CHANNEL_CHA_TEST               (0x0B << AD7616_CHANNEL_CHA_POS)          // TEST 读取代码为0xAAAA

// 通道 B [7:4] R/W
#define AD7616_CHANNEL_CHB_POS                4
#define AD7616_CHANNEL_CHB_B0                 (0x00 << AD7616_CHANNEL_CHA_POS)          // B0 
#define AD7616_CHANNEL_CHB_B1                 (0x01 << AD7616_CHANNEL_CHA_POS)          // B1
#define AD7616_CHANNEL_CHB_B2                 (0x02 << AD7616_CHANNEL_CHA_POS)          // B2
#define AD7616_CHANNEL_CHB_B3                 (0x03 << AD7616_CHANNEL_CHA_POS)          // B3
#define AD7616_CHANNEL_CHB_B4                 (0x04 << AD7616_CHANNEL_CHA_POS)          // B4
#define AD7616_CHANNEL_CHB_B5                 (0x05 << AD7616_CHANNEL_CHA_POS)          // B5
#define AD7616_CHANNEL_CHB_B6                 (0x06 << AD7616_CHANNEL_CHA_POS)          // B6
#define AD7616_CHANNEL_CHB_B7                 (0x07 << AD7616_CHANNEL_CHA_POS)          // B7
#define AD7616_CHANNEL_CHB_VCC                (0x08 << AD7616_CHANNEL_CHA_POS)          // VCC
#define AD7616_CHANNEL_CHB_ALDO               (0x09 << AD7616_CHANNEL_CHA_POS)          // ALDO
#define AD7616_CHANNEL_CHB_NONE               (0x0A << AD7616_CHANNEL_CHA_POS)          // Reserved
#define AD7616_CHANNEL_CHB_TEST               (0x0B << AD7616_CHANNEL_CHA_POS)          // TEST 读取代码为0x5555

// ========================================================================== 输入范围寄存器A1定义 (Input Range Register A1) ==========================================================================

// V0A [1:0] R/W
#define AD7616_Range_A1_V0A_POS                 0
// V1A [3:2] R/W
#define AD7616_Range_A1_V1A_POS                 2
// V2A [5:4] R/W
#define AD7616_Range_A1_V2A_POS                 4
// V3A [7:6] R/W
#define AD7616_Range_A1_V3A_POS                 6

// ========================================================================== 输入范围寄存器A2定义 (Input Range Register A2) ==========================================================================

// V4A [1:0] R/W
#define AD7616_Range_A2_V0A_POS                 0
// V5A [3:2] R/W
#define AD7616_Range_A2_V1A_POS                 2
// V6A [5:4] R/W
#define AD7616_Range_A2_V2A_POS                 4
// V7A [7:6] R/W
#define AD7616_Range_A2_V3A_POS                 6

// ========================================================================== 输入范围寄存器B1定义 (Input Range Register B1) ==========================================================================

// V0B [1:0] R/W
#define AD7616_Range_B1_V0B_POS                 0
// V1B [3:2] R/W
#define AD7616_Range_B1_V1B_POS                 2
// V2B [5:4] R/W
#define AD7616_Range_B1_V2B_POS                 4
// V3B [7:6] R/W
#define AD7616_Range_B1_V3B_POS                 6

// ========================================================================== 输入范围寄存器B2定义 (Input Range Register B2) ==========================================================================

// V4B [1:0] R/W
#define AD7616_Range_B2_V4B_POS                 0
// V5B [3:2] R/W
#define AD7616_Range_B2_V5B_POS                 2
// V6B [5:4] R/W
#define AD7616_Range_B2_V6B_POS                 4
// V7B [7:6] R/W
#define AD7616_Range_B2_V7B_POS                 6


// ========================================================================== 函数声明 ==========================================================================

// 初始化和配置
Module_Status_t Module_AD7616_Config(void);
Module_Status_t Module_AD7616_ConfigRegister(uint8_t os_mode, bool burst_en, bool seq_en, bool status_en, bool crc_en);
Module_Status_t Module_AD7616_SetRange(uint8_t channel, AD7616_Range_TypeDef range);
Module_Status_t Module_AD7616_SetOversample(uint8_t os_mode);
Module_Status_t Module_AD7616_SetBurstMode(bool enable);
Module_Status_t Module_AD7616_SetSequencer(bool enable);
Module_Status_t Module_AD7616_SetStatusOutput(bool enable);
Module_Status_t Module_AD7616_SetCRC(bool enable);
Module_Status_t Module_AD7616_GetConfig(uint16_t *pConfig);
bool Module_AD7616_CheckSelfTest(void);
Module_Status_t Module_AD7616_SetChannelSelect(uint8_t channel_a, uint8_t channel_b);
Module_Status_t Module_AD7616_GetChannelSelect(uint8_t *pChannelA, uint8_t *pChannelB);

// ...existing code...

// 寄存器读写
Module_Status_t Module_AD7616_WriteReg(uint8_t reg_addr, uint8_t data);
uint8_t Module_AD7616_ReadReg(uint8_t reg_addr);

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
