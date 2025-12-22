#include "Module_AD7616.h"
#include "bsp_fmc.h"
#include "stdio.h"

// ========================================================================== 私有变量 ==========================================================================

static AD7616_Range_TypeDef g_channel_range[AD7616_CHANNEL_NUM];   // 保存每个通道的量程配置

// ========================================================================== GPIO 初始化 ==========================================================================

/**
  * @brief  初始化 AD7616 控制引脚
  * @retval None
  */
static void AD7616_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能 GPIO 时钟
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // 配置 RESET 引脚（输出）
    GPIO_InitStruct.Pin = AD7616_RESET_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(AD7616_RESET_PORT, &GPIO_InitStruct);
    
    // 配置 CONVST 引脚（输出）
    GPIO_InitStruct.Pin = AD7616_CONVST_PIN;
    HAL_GPIO_Init(AD7616_CONVST_PORT, &GPIO_InitStruct);
    
    // 配置 BUSY 引脚（输入）
    GPIO_InitStruct.Pin = AD7616_BUSY_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(AD7616_BUSY_PORT, &GPIO_InitStruct);
    
    // 初始化引脚状态
    AD7616_RESET_HIGH();
    AD7616_CONVST_HIGH();
}

// ========================================================================== 初始化和配置 ==========================================================================

/**
  * @brief  初始化 AD7616
  * @retval HAL_StatusTypeDef HAL_OK: 初始化成功
  */
HAL_StatusTypeDef AD7616_Init(void)
{
    uint8_t i;
    
    // 初始化 FMC PSRAM（AD7616 通过 FMC 总线访问）
    if (BSP_FMC_PSRAM_Init() != HAL_OK)
    {
        return HAL_ERROR;
    }
    
    // 初始化控制引脚
    AD7616_GPIO_Init();
    
    // 硬件复位
    if (AD7616_Reset() != HAL_OK)
    {
        return HAL_ERROR;
    }
    
    // 初始化量程配置为 ±10V
    for (i = 0; i < AD7616_CHANNEL_NUM; i++)
    {
        g_channel_range[i] = AD7616_RANGE_10V;
    }
    
    // 配置所有通道为 ±10V 量程
    AD7616_WriteReg(AD7616_REG_RANGE_A, 0xAAAA);  // A0-A7 通道
    AD7616_WriteReg(AD7616_REG_RANGE_B, 0xAAAA);  // B0-B7 通道
    
    // 延时等待配置生效
    HAL_Delay(1);
    
    return HAL_OK;
}

/**
  * @brief  硬件复位 AD7616
  * @retval HAL_StatusTypeDef HAL_OK: 复位成功
  */
HAL_StatusTypeDef AD7616_Reset(void)
{
    // 复位脉冲（至少 50ns）
    AD7616_RESET_LOW();
    HAL_Delay(1);  // 延时 1ms（远大于 50ns）
    AD7616_RESET_HIGH();
    HAL_Delay(10); // 等待复位完成
    
    return HAL_OK;
}

/**
  * @brief  设置通道量程
  * @param  channel: 通道号（0-15）
  * @param  range: 量程配置
  * @retval HAL_StatusTypeDef HAL_OK: 设置成功
  */
HAL_StatusTypeDef AD7616_SetRange(uint8_t channel, AD7616_Range_TypeDef range)
{
    uint16_t reg_value;
    uint8_t reg_addr;
    uint8_t bit_pos;
    
    if (channel >= AD7616_CHANNEL_NUM)
    {
        return HAL_ERROR;
    }
    
    // 保存量程配置
    g_channel_range[channel] = range;
    
    // 确定寄存器地址和位置
    if (channel < 8)
    {
        reg_addr = AD7616_REG_RANGE_A;  // A0-A7
        bit_pos = channel * 2;
    }
    else
    {
        reg_addr = AD7616_REG_RANGE_B;  // B0-B7
        bit_pos = (channel - 8) * 2;
    }
    
    // 读取当前寄存器值
    reg_value = AD7616_ReadReg(reg_addr);
    
    // 修改对应位
    reg_value &= ~(0x03 << bit_pos);        // 清除原值
    reg_value |= (range << bit_pos);        // 设置新值
    
    // 写回寄存器
    return AD7616_WriteReg(reg_addr, reg_value);
}

/**
  * @brief  设置工作模式
  * @param  mode: 工作模式
  * @retval HAL_StatusTypeDef HAL_OK: 设置成功
  */
HAL_StatusTypeDef AD7616_SetMode(AD7616_Mode_TypeDef mode)
{
    uint16_t reg_value;
    
    // 读取配置寄存器
    reg_value = AD7616_ReadReg(AD7616_REG_CONFIG);
    
    // 修改模式位
    reg_value &= ~(0x03 << 6);          // 清除模式位
    reg_value |= (mode << 6);           // 设置新模式
    
    // 写回配置寄存器
    return AD7616_WriteReg(AD7616_REG_CONFIG, reg_value);
}

// ========================================================================== 寄存器读写 ==========================================================================

/**
  * @brief  写 AD7616 寄存器
  * @param  reg_addr: 寄存器地址
  * @param  data: 要写入的数据
  * @retval HAL_StatusTypeDef HAL_OK: 写入成功
  */
HAL_StatusTypeDef AD7616_WriteReg(uint8_t reg_addr, uint16_t data)
{
    uint16_t cmd;
    
    // 构造写命令：[15]=1(写), [14:8]=寄存器地址, [7:0]=数据高字节
    cmd = (1 << 15) | ((reg_addr & 0x7F) << 8) | ((data >> 8) & 0xFF);
    
    // 写入命令和地址
    BSP_FMC_PSRAM_WriteHalfWord(0, cmd);
    
    // 短延时
    for (volatile int i = 0; i < 10; i++);
    
    // 写入数据低字节
    BSP_FMC_PSRAM_WriteHalfWord(0, data & 0xFF);
    
    return HAL_OK;
}

/**
  * @brief  读 AD7616 寄存器
  * @param  reg_addr: 寄存器地址
  * @retval uint16_t 读取的数据
  */
uint16_t AD7616_ReadReg(uint8_t reg_addr)
{
    uint16_t cmd;
    uint16_t data_h, data_l;
    
    // 构造读命令：[15]=0(读), [14:8]=寄存器地址
    cmd = (0 << 15) | ((reg_addr & 0x7F) << 8);
    
    // 写入读命令
    BSP_FMC_PSRAM_WriteHalfWord(0, cmd);
    
    // 短延时
    for (volatile int i = 0; i < 10; i++);
    
    // 读取数据高字节
    data_h = BSP_FMC_PSRAM_ReadHalfWord(0);
    
    // 读取数据低字节
    data_l = BSP_FMC_PSRAM_ReadHalfWord(0);
    
    return ((data_h << 8) | data_l);
}

// ========================================================================== 数据采集 ==========================================================================

/**
  * @brief  启动转换
  * @retval HAL_StatusTypeDef HAL_OK: 启动成功
  */
HAL_StatusTypeDef AD7616_StartConversion(void)
{
    // 发送转换脉冲（CONVST 下降沿触发）
    AD7616_CONVST_HIGH();
    for (volatile int i = 0; i < 5; i++);  // 至少 25ns 高电平
    AD7616_CONVST_LOW();
    for (volatile int i = 0; i < 5; i++);  // 至少 25ns 低电平
    AD7616_CONVST_HIGH();
    
    return HAL_OK;
}

/**
  * @brief  检查 AD7616 是否忙
  * @retval bool true: 忙, false: 空闲
  */
bool AD7616_IsBusy(void)
{
    return (AD7616_READ_BUSY() == GPIO_PIN_SET);
}

/**
  * @brief  等待转换完成
  * @param  timeout_ms: 超时时间（毫秒）
  * @retval HAL_StatusTypeDef HAL_OK: 转换完成, HAL_TIMEOUT: 超时
  */
HAL_StatusTypeDef AD7616_WaitConversionDone(uint32_t timeout_ms)
{
    uint32_t tickstart = HAL_GetTick();
    
    // 等待 BUSY 信号变低
    while (AD7616_IsBusy())
    {
        if ((HAL_GetTick() - tickstart) > timeout_ms)
        {
            return HAL_TIMEOUT;
        }
    }
    
    return HAL_OK;
}

/**
  * @brief  读取单个通道数据
  * @param  channel: 通道号（0-15）
  * @retval uint16_t 读取的 ADC 值
  */
uint16_t AD7616_ReadChannel(uint8_t channel)
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
  * @retval HAL_StatusTypeDef HAL_OK: 读取成功
  */
HAL_StatusTypeDef AD7616_ReadAllChannels(uint16_t *pData)
{
    uint8_t i;
    
    if (pData == NULL)
    {
        return HAL_ERROR;
    }
    
    // 启动转换
    AD7616_StartConversion();
    
    // 等待转换完成
    if (AD7616_WaitConversionDone(10) != HAL_OK)
    {
        return HAL_TIMEOUT;
    }
    
    // 读取所有通道数据
    for (i = 0; i < AD7616_CHANNEL_NUM; i++)
    {
        pData[i] = AD7616_ReadChannel(i);
    }
    
    return HAL_OK;
}

// ========================================================================== 批量采集 ==========================================================================

/**
  * @brief  启动批量转换（将数据存储到内存）
  * @param  pBuffer: 数据缓冲区指针
  * @param  sample_count: 采样次数（每次采样 16 个通道）
  * @retval HAL_StatusTypeDef HAL_OK: 采集成功
  */
HAL_StatusTypeDef AD7616_StartBatchConversion(uint16_t *pBuffer, uint32_t sample_count)
{
    uint32_t i;
    
    if (pBuffer == NULL)
    {
        return HAL_ERROR;
    }
    
    for (i = 0; i < sample_count; i++)
    {
        // 读取一次所有通道
        if (AD7616_ReadAllChannels(&pBuffer[i * AD7616_CHANNEL_NUM]) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }
    
    return HAL_OK;
}

/**
  * @brief  批量读取数据到 PSRAM（使用 DMA）
  * @param  psram_addr: PSRAM 存储地址（相对偏移）
  * @param  sample_count: 采样次数
  * @retval HAL_StatusTypeDef HAL_OK: 读取成功
  * @note   此函数需要配合定时器和 DMA 实现高速连续采集
  */
HAL_StatusTypeDef AD7616_ReadBatch_DMA(uint32_t psram_addr, uint32_t sample_count)
{
    // 此功能需要硬件 DMA 支持，具体实现依赖于定时器触发和 DMA 配置
    // 这里提供接口框架，实际应用时需要配置：
    // 1. 定时器触发 CONVST 信号
    // 2. DMA 从 FMC 数据地址搬运数据到 PSRAM
    // 3. 完成中断处理
    
    return HAL_OK;
}

// ========================================================================== 工具函数 ==========================================================================

/**
  * @brief  将 ADC 值转换为电压（浮点）
  * @param  adc_value: ADC 采样值
  * @param  range: 量程配置
  * @retval float 电压值（V）
  */
float AD7616_ConvertToVoltage(uint16_t adc_value, AD7616_Range_TypeDef range)
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
int32_t AD7616_ConvertToMilliVolt(uint16_t adc_value, AD7616_Range_TypeDef range)
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
