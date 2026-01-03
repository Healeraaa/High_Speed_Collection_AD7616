#include "Module_AD7616.h"
#include "bsp.h"
#include "bsp_fmc.h"
#include "bsp_timer.h"
#include "stdio.h"

// ========================================================================== 私有变量 ==========================================================================

// static AD7616_Range_TypeDef g_channel_range[AD7616_CHANNEL_NUM];   // 保存每个通道的量程配置
static uint8_t g_input_range_register[AD7616_RANGE_RESGISTER_NUM];   // 保存每个输入范围寄存器的值
static uint8_t g_config_range_register = 0x00;                      // 保存配置寄存器的值
// ========================================================================== 初始化和配置 ==========================================================================

/**
  * @brief  初始化 AD7616
  * @retval Module_Status_t Module_OK: 初始化成功
  */
Module_Status_t Module_AD7616_Config(void)
{
    uint8_t i;
    // 初始化 FMC PSRAM（AD7616 通过 FMC 总线访问）
    if (BSP_FMC_PSRAM_Init() != BSP_OK)
    {
        return Module_ERROR;
    }

    // 初始化控制引脚
    // AD7616_GPIO_Init();
    
    // 初始化量程配置为 ±10V
    for (i = 0; i < AD7616_RANGE_RESGISTER_NUM; i++)
    {
        g_input_range_register[i] = 0xAA;
    }
    Module_AD7616_ConfigRegister(AD7616_CONFIG_OS_16X, false, false, false, false);
    BSP_DWT_Delay_ms(10);  // 等待 AD7616 上电稳定
    // 初始化配置寄存器为默认值
    Module_AD7616_ConfigRegister(AD7616_CONFIG_OS_16X, false, false, false, false);
    BSP_DWT_Delay_ms(10);  // 等待 AD7616 上电稳定
    // 默认选择通道 A0-B0
    Module_AD7616_SetChannelSelect(AD7616_CHANNEL_CHA_A0, AD7616_CHANNEL_CHB_B0);
    BSP_DWT_Delay_ms(10);  // 等待 AD7616 上电稳定
    // 配置所有通道为 ±5V 量程
    Module_AD7616_WriteReg(AD7616_REG_RANGE_A1, 0xAA);  
    BSP_DWT_Delay_ms(10);  // 等待 AD7616 上电稳定
    Module_AD7616_WriteReg(AD7616_REG_RANGE_A2, 0xAA);  
    BSP_DWT_Delay_ms(10);  // 等待 AD7616 上电稳定
    Module_AD7616_WriteReg(AD7616_REG_RANGE_B1, 0xAA);  
    BSP_DWT_Delay_ms(10);  // 等待 AD7616 上电稳定
    Module_AD7616_WriteReg(AD7616_REG_RANGE_B2, 0xAA);  
    BSP_DWT_Delay_ms(10);  // 等待 AD7616 上电稳定
    // while(1)
    // {
    //     __IO uint16_t read_data = Module_AD7616_ReadReg(AD7616_REG_RANGE_A1);
    //     BSP_DWT_Delay_ms(1);
    // }
    
    return Module_OK;
}

// ========================================================================== 初始化和配置 ==========================================================================

/**
  * @brief  配置 AD7616 配置寄存器
  * @param  os_mode: 过采样模式 (AD7616_CONFIG_OS_DISABLE / AD7616_CONFIG_OS_2X / ...)
  * @param  burst_en: 突发模式使能 (true/false)
  * @param  seq_en: 序列器使能 (true/false)
  * @param  status_en: 状态输出使能 (true/false)
  * @param  crc_en: CRC 校验使能 (true/false)
  * @retval Module_Status_t Module_OK: 配置成功
  */
Module_Status_t Module_AD7616_ConfigRegister(uint8_t os_mode, bool burst_en, bool seq_en, bool status_en, bool crc_en)
{
    uint8_t config_value = 0;
    
    config_value |= (crc_en ? AD7616_CONFIG_CRCEN_ENABLE : AD7616_CONFIG_CRCEN_DISABLE);
    config_value |= (status_en ? AD7616_CONFIG_STATUSEN_ENABLE : AD7616_CONFIG_STATUSEN_DISABLE);
    config_value |= os_mode;  // os_mode 本身就是已位移的宏值
    config_value |= (seq_en ? AD7616_CONFIG_SEQEN_ENABLE : AD7616_CONFIG_SEQEN_DISABLE);
    config_value |= (burst_en ? AD7616_CONFIG_BURSTEN_ENABLE : AD7616_CONFIG_BURSTEN_DISABLE);
    
    // 更新缓存
    g_config_range_register = config_value;
    
    // 写入寄存器
    return Module_AD7616_WriteReg(AD7616_REG_CONFIG, config_value);
}

/**
  * @brief  设置过采样率
  * @param  os_mode: 过采样模式
  *         - AD7616_CONFIG_OS_DISABLE (无过采样)
  *         - AD7616_CONFIG_OS_2X      (2x 过采样)
  *         - AD7616_CONFIG_OS_4X      (4x 过采样)
  *         - AD7616_CONFIG_OS_8X      (8x 过采样)
  *         - AD7616_CONFIG_OS_16X     (16x 过采样)
  *         - AD7616_CONFIG_OS_32X     (32x 过采样)
  *         - AD7616_CONFIG_OS_64X     (64x 过采样)
  *         - AD7616_CONFIG_OS_128X    (128x 过采样)
  * @retval Module_Status_t Module_OK: 设置成功
  */
Module_Status_t Module_AD7616_SetOversample(uint8_t os_mode)
{
    uint8_t config_value;
    
    // 读取当前配置
    config_value = g_config_range_register;
    
    // 修改过采样位 [4:2]
    config_value &= ~(0x07 << AD7616_CONFIG_OS_POS);  // 清除 bit[4:2]
    config_value |= os_mode;                           // 直接或上宏值
    
    // 更新缓存
    g_config_range_register = config_value;
    
    // 写回寄存器
    return Module_AD7616_WriteReg(AD7616_REG_CONFIG, config_value);
}

/**
  * @brief  使能/禁用突发模式
  * @param  enable: true=使能, false=禁用
  * @retval Module_Status_t Module_OK: 设置成功
  */
Module_Status_t Module_AD7616_SetBurstMode(bool enable)
{
    uint8_t config_value;
    
    config_value = g_config_range_register;
    
    // 修改突发模式位 [6]
    if (enable)
    {
        config_value |= AD7616_CONFIG_BURSTEN_ENABLE;
    }
    else
    {
        config_value &= ~(0x01 << AD7616_CONFIG_BURSTEN_POS);
    }
    
    g_config_range_register = config_value;
    
    return Module_AD7616_WriteReg(AD7616_REG_CONFIG, config_value);
}

/**
  * @brief  使能/禁用通道序列器
  * @param  enable: true=使能, false=禁用
  * @retval Module_Status_t Module_OK: 设置成功
  */
Module_Status_t Module_AD7616_SetSequencer(bool enable)
{
    uint8_t config_value;
    
    config_value = g_config_range_register;
    
    // 修改序列器使能位 [5]
    if (enable)
    {
        config_value |= AD7616_CONFIG_SEQEN_ENABLE;
    }
    else
    {
        config_value &= ~(0x01 << AD7616_CONFIG_SEQEN_POS);
    }
    
    g_config_range_register = config_value;
    
    return Module_AD7616_WriteReg(AD7616_REG_CONFIG, config_value);
}

/**
  * @brief  使能/禁用状态输出
  * @param  enable: true=使能, false=禁用
  * @retval Module_Status_t Module_OK: 设置成功
  */
Module_Status_t Module_AD7616_SetStatusOutput(bool enable)
{
    uint8_t config_value;
    
    config_value = g_config_range_register;
    
    // 修改状态输出位 [1]
    if (enable)
    {
        config_value |= AD7616_CONFIG_STATUSEN_ENABLE;
    }
    else
    {
        config_value &= ~(0x01 << AD7616_CONFIG_STATUSEN_POS);
    }
    
    g_config_range_register = config_value;
    
    return Module_AD7616_WriteReg(AD7616_REG_CONFIG, config_value);
}

/**
  * @brief  使能/禁用 CRC 校验
  * @param  enable: true=使能, false=禁用
  * @retval Module_Status_t Module_OK: 设置成功
  */
Module_Status_t Module_AD7616_SetCRC(bool enable)
{
    uint8_t config_value;
    
    config_value = g_config_range_register;
    
    // 修改 CRC 使能位 [0]
    if (enable)
    {
        config_value |= AD7616_CONFIG_CRCEN_ENABLE;
    }
    else
    {
        config_value &= ~(0x01 << AD7616_CONFIG_CRCEN_POS);
    }
    
    g_config_range_register = config_value;
    
    return Module_AD7616_WriteReg(AD7616_REG_CONFIG, config_value);
}

/**
  * @brief  检查自检状态
  * @retval bool true=自检通过, false=自检失败
  */
bool Module_AD7616_CheckSelfTest(void)
{
    uint16_t config_value;
    
    config_value = Module_AD7616_ReadReg(AD7616_REG_CONFIG);
    
    // 检查 SDEF 位 [7] (只读位)
    return ((config_value & (0x01 << AD7616_CONFIG_SDEF_POS)) != 0);
}

/**
  * @brief  设置通道选择寄存器
  * @param  channel_a: A 侧通道宏定义 (AD7616_CHANNEL_CHA_xxx)
  *         例如: AD7616_CHANNEL_CHA_A0, AD7616_CHANNEL_CHA_A1, ...
  * @param  channel_b: B 侧通道宏定义 (AD7616_CHANNEL_CHB_xxx)
  *         例如: AD7616_CHANNEL_CHB_B0, AD7616_CHANNEL_CHB_B1, ...
  * @retval Module_Status_t Module_OK: 设置成功
  */
Module_Status_t Module_AD7616_SetChannelSelect(uint8_t channel_a, uint8_t channel_b)
{
    uint16_t channel_value;
    
    // 直接组合宏值（宏已经包含位移）
    channel_value = channel_a | channel_b;
    
    return Module_AD7616_WriteReg(AD7616_REG_CHANNEL, channel_value); 
}

/**
  * @brief  读取配置寄存器
  * @param  pConfig: 配置值指针
  * @retval Module_Status_t Module_OK: 读取成功
  */
Module_Status_t Module_AD7616_GetConfig(uint16_t *pConfig)
{
    if (pConfig == NULL)
    {
        return Module_ERROR;
    }
    
    *pConfig = Module_AD7616_ReadReg(AD7616_REG_CONFIG);
    
    // 更新缓存
    g_config_range_register = (*pConfig) & 0x00FF;  // 取低 8 位
    
    return Module_OK;
}

/**
  * @brief  读取通道选择寄存器
  * @param  pChannelA: A 侧通道指针
  * @param  pChannelB: B 侧通道指针
  * @retval Module_Status_t Module_OK: 读取成功
  */
Module_Status_t Module_AD7616_GetChannelSelect(uint8_t *pChannelA, uint8_t *pChannelB)
{
    uint16_t channel_value;
    
    if (pChannelA == NULL || pChannelB == NULL)
    {
        return Module_ERROR;
    }
    
    channel_value = Module_AD7616_ReadReg(AD7616_REG_CHANNEL);
    
    *pChannelA = (channel_value >> AD7616_CHANNEL_CHA_POS) & 0x0F;
    *pChannelB = (channel_value >> AD7616_CHANNEL_CHB_POS) & 0x0F;
    
    return Module_OK;
}




/**
  * @brief  设置通道量程
  * @param  channel: 通道号（0-15）
  * @param  range: 量程配置
  * @retval Module_Status_t Module_OK: 设置成功
  */
Module_Status_t Module_AD7616_SetRange(uint8_t channel, AD7616_Range_TypeDef range)
{
    uint8_t reg_pos;
    uint8_t reg_value;
    uint8_t reg_addr;
    uint8_t bit_pos;
    
    if (channel >= AD7616_CHANNEL_NUM)
    {
        return Module_ERROR;
    }
    
    reg_pos = channel / 4;  // 每 4 个通道对应一个寄存器
    bit_pos = (channel % 4) * 2; // 每个通道占 2 位，所以需要 *2
    
    // 确定寄存器地址和位置
    if (reg_pos == 0 )
    {
        reg_addr = AD7616_REG_RANGE_A1;  // A0-A3
    }
    else if (reg_pos == 1)
    {
        reg_addr = AD7616_REG_RANGE_A2;  // A4-A7
    }
    else if (reg_pos == 2)
    {
        reg_addr = AD7616_REG_RANGE_B1;  // B0-B3
    }
    else // reg_pos == 3
    {
        reg_addr = AD7616_REG_RANGE_B2;  // B4-B7
    }
    
    // 读取当前寄存器值
    reg_value = g_input_range_register[reg_pos];
    
    // 修改对应位
    reg_value &= ~(0x03 << bit_pos);        // 清除原值
    reg_value |= (range << bit_pos);        // 设置新值

    // 更新缓存
    g_input_range_register[reg_pos] = reg_value;
    
    // 写回寄存器
    return Module_AD7616_WriteReg(reg_addr, reg_value);
}


// ========================================================================== 寄存器读写 ==========================================================================

/**
  * @brief  写 AD7616 寄存器
  * @param  reg_addr: 寄存器地址
  * @param  data: 要写入的数据
  * @retval Module_Status_t Module_OK: 写入成功
  */
Module_Status_t Module_AD7616_WriteReg(uint8_t reg_addr, uint8_t data)
{
    uint16_t write_word;
    
    // 构造 16 位写入字： [15] = 1 (写操作)   [14:9] = 寄存器地址 (取低 6 位)   [8:0] = 数据 (取低 9 位)
    write_word = (1 << 15)                      // D15: 写操作标志
               | ((reg_addr & 0x3F) << 9)       // D14~D9: 6 位寄存器地址
               | (data & 0xFF);                  // D7~D0: 9 位数据
    

    BSP_FMC_PSRAM_WriteHalfWord(0, write_word);
    // 短延时确保写入完成
    // for (volatile int i = 0; i < 10; i++);

    return Module_OK;
}

/**
  * @brief  读 AD7616 寄存器
  * @param  reg_addr: 寄存器地址
  * @retval uint16_t 读取的数据
  */
uint8_t Module_AD7616_ReadReg(uint8_t reg_addr)
{
    uint16_t read_cmd;
    uint16_t read_data;
    
    // 构造读命令字：[15] = 0 (读操作)   [14:9] = 寄存器地址 (取低 6 位)   [8:0] = 0 (读操作时该字段无意义)
    read_cmd = (0 << 15)                        // D15: 读操作标志
             | ((reg_addr & 0x3F) << 9);        // D14~D9: 6 位寄存器地址
    
    // 1.写入读命令（发起读请求）
    BSP_FMC_PSRAM_WriteHalfWord(0, read_cmd);
    
    // 短延时
    for (volatile int i = 0; i < 10; i++);
    
    // 2.从数据总线读取数据（低 9 位有效）
    read_data = BSP_FMC_PSRAM_ReadHalfWord(0);

    // 返回低 9 位有效数据
    return (read_data & 0xFF);
}


// ========================================================================== 数据采集 ==========================================================================

/**
  * @brief  启动转换
  * @retval Module_Status_t Module_OK: 启动成功
  */
Module_Status_t Module_AD7616_StartConversion(void)
{   
    return Module_OK;
}



/**
  * @brief  读取单个通道数据
  * @param  channel: 通道号（0-15）
  * @retval uint16_t 读取的 ADC 值
  */
uint16_t Module_AD7616_ReadChannel(uint8_t channel)
{
    uint16_t data;
    
    if (channel >= AD7616_CHANNEL_NUM)
    {
        return 0;
    }
    
    // 通过 FMC 总线读取数据（地址偏移 = 通道号 * 2）
    data = BSP_FMC_PSRAM_ReadHalfWord(channel * 2);
    
    return data;
}

/**
  * @brief  读取所有通道数据
  * @param  pData: 数据缓冲区指针（16 个 uint16_t）
  * @retval Module_Status_t Module_OK: 读取成功
  */
Module_Status_t Module_AD7616_ReadAllChannels(uint16_t *pData)
{
    uint8_t i;
    
    if (pData == NULL)
    {
        return Module_ERROR;
    }
    
    // 启动转换
    Module_AD7616_StartConversion();
    
    // 等待转换完成
    BSP_DWT_Delay_us(16);
    // 读取所有通道数据
    for (i = 0; i < AD7616_CHANNEL_NUM; i++)
    {
        pData[i] = Module_AD7616_ReadChannel(i);
    }
    
    return Module_OK;
}

// ========================================================================== 批量采集 ==========================================================================

/**
  * @brief  启动批量转换（将数据存储到内存）
  * @param  pBuffer: 数据缓冲区指针
  * @param  sample_count: 采样次数（每次采样 16 个通道）
  * @retval Module_Status_t Module_OK: 采集成功
  */
Module_Status_t Module_AD7616_StartBatchConversion(uint16_t *pBuffer, uint32_t sample_count)
{
    uint32_t i;
    
    if (pBuffer == NULL)
    {
        return Module_ERROR;
    }
    
    for (i = 0; i < sample_count; i++)
    {
        // 读取一次所有通道
        if (Module_AD7616_ReadAllChannels(&pBuffer[i * AD7616_CHANNEL_NUM]) != Module_OK)
        {
            return Module_ERROR;
        }
    }
    
    return Module_OK;
}

/**
  * @brief  批量读取数据到 PSRAM（使用 DMA）
  * @param  psram_addr: PSRAM 存储地址（相对偏移）
  * @param  sample_count: 采样次数
  * @retval Module_Status_t Module_OK: 读取成功
  * @note   此函数需要配合定时器和 DMA 实现高速连续采集
  */
Module_Status_t AD7616_ReadBatch_DMA(uint32_t psram_addr, uint32_t sample_count)
{
    // 此功能需要硬件 DMA 支持，具体实现依赖于定时器触发和 DMA 配置
    // 这里提供接口框架，实际应用时需要配置：
    // 1. 定时器触发 CONVST 信号
    // 2. DMA 从 FMC 数据地址搬运数据到 PSRAM
    // 3. 完成中断处理
    
    return Module_OK;
}

// ========================================================================== 工具函数 ==========================================================================

/**
  * @brief  将 ADC 值转换为电压（浮点）
  * @param  adc_value: ADC 采样值
  * @param  range: 量程配置
  * @retval float 电压值（V）
  */
float Module_AD7616_ConvertToVoltage(uint16_t adc_value, AD7616_Range_TypeDef range)
{
    float voltage;
    int16_t signed_value;
    
    // 转换为有符号数（AD7616 输出二进制补码）
    signed_value = (int16_t)adc_value;
    
    // 根据量程计算电压
    switch (range)
    {
        case AD7616_RANGE_2V5:
            voltage = (signed_value / 32768.0f) * 2.5f;
            break;
        case AD7616_RANGE_5V:
            voltage = (signed_value / 32768.0f) * 5.0f;
            break;
        case AD7616_RANGE_10V:
            voltage = (signed_value / 32768.0f) * 10.0f;
            break;
        default:
            voltage = 0.0f;
            break;
    }
    
    return voltage;
}

/**
  * @brief  将 ADC 值转换为电压（毫伏，整数）
  * @param  adc_value: ADC 采样值
  * @param  range: 量程配置
  * @retval int32_t 电压值（mV）
  */
int32_t Module_AD7616_ConvertToMilliVolt(uint16_t adc_value, AD7616_Range_TypeDef range)
{
    int32_t voltage_mv;
    int16_t signed_value;
    
    // 转换为有符号数
    signed_value = (int16_t)adc_value;
    
    // 根据量程计算电压（mV）
    switch (range)
    {
        case AD7616_RANGE_2V5:
            voltage_mv = ((int32_t)signed_value * 2500) / 32768;
            break;
        case AD7616_RANGE_5V:
            voltage_mv = ((int32_t)signed_value * 5000) / 32768;
            break;
        case AD7616_RANGE_10V:
            voltage_mv = ((int32_t)signed_value * 10000) / 32768;
            break;
        default:
            voltage_mv = 0;
            break;
    }
    
    return voltage_mv;
}
