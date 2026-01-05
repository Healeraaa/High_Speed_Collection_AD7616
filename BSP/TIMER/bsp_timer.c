#include "bsp_timer.h"
#include "bsp.h"
#include "Module.h"


// ========================================================================== DWT ==========================================================================
/**
 * @brief 初始化DWT CYCCNT
 * @note 需要在使用延迟函数前调用一次
 */
void BSP_DWT_DelayInit(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // 使能DWT
    DWT->CYCCNT = 0;                                  // 清零计数器
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             // 使能CYCCNT计数器
} 

/**
 * @brief 微秒级延迟函数
 * @param us 延迟时间(微秒)
 * @note 基于DWT CYCCNT实现的精确延迟
 */
void BSP_DWT_Delay_us(uint32_t us)
{
    uint32_t start_ticks = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);  // 计算需要的时钟周期数
    
    while ((DWT->CYCCNT - start_ticks) < ticks);
}

/**
 * @brief 毫秒级延迟函数
 * @param ms 延迟时间(毫秒)
 */
void BSP_DWT_Delay_ms(uint32_t ms)
{
    uint32_t start_ticks = DWT->CYCCNT;
    uint32_t ticks = ms * (SystemCoreClock / 1000);  // 计算需要的时钟周期数
    
    while ((DWT->CYCCNT - start_ticks) < ticks);
}

/**
 * @brief 秒级延迟函数
 * @param s 延迟时间(秒)
 */
void BSP_DWT_Delay_s(uint32_t s)
{
    for (uint32_t i = 0; i < s; i++)
    {
        BSP_DWT_Delay_ms(1000);
    }
}

/**
 * @brief 获取当前计数值(用于测量代码执行时间)
 * @return 当前CYCCNT计数值
 */
uint32_t BSP_DWT_GetCycles(void)
{
    return DWT->CYCCNT;
}

// ========================================================================== TIM3 ==========================================================================
void BSP_TIM3_PWM0_Init(void)
{
    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    /* Peripheral clock enable */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);  // 使能 TIM3 外设时钟（APB1 总线）
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

    // ========== DMA 配置（用于 TIM3_CH2 触发数据采集） ==========
    LL_DMA_SetPeriphRequest(DMA1, LL_DMA_STREAM_0, LL_DMAMUX1_REQ_TIM3_CH2);           // 设置 DMA 请求源为 TIM3_CH2
    LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_STREAM_0, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);  // 方向：外设 → 内存
    LL_DMA_SetStreamPriorityLevel(DMA1, LL_DMA_STREAM_0, LL_DMA_PRIORITY_VERYHIGH);    // 优先级：最高（保证数据采集实时性）
    LL_DMA_SetMode(DMA1, LL_DMA_STREAM_0, LL_DMA_MODE_CIRCULAR);                       // 循环模式（连续采集）
    LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_STREAM_0, LL_DMA_PERIPH_NOINCREMENT);        // 外设地址不递增（固定读取 AD7616 数据寄存器）
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_0, LL_DMA_MEMORY_INCREMENT);          // 内存地址递增（顺序存储采样数据）
    LL_DMA_SetPeriphSize(DMA1, LL_DMA_STREAM_0, LL_DMA_PDATAALIGN_HALFWORD);          // 外设数据宽度：16-bit（AD7616 数据宽度）
    LL_DMA_SetMemorySize(DMA1, LL_DMA_STREAM_0, LL_DMA_MDATAALIGN_HALFWORD);          // 内存数据宽度：16-bit
    LL_DMA_EnableFifoMode(DMA1, LL_DMA_STREAM_0);                                      // 使能 FIFO 模式（提高突发传输效率）
    LL_DMA_SetFIFOThreshold(DMA1, LL_DMA_STREAM_0, LL_DMA_FIFOTHRESHOLD_FULL);        // FIFO 阈值：满触发（4×16-bit）
    LL_DMA_SetMemoryBurstxfer(DMA1, LL_DMA_STREAM_0, LL_DMA_MBURST_SINGLE);           // 内存突发：单次传输
    LL_DMA_SetPeriphBurstxfer(DMA1, LL_DMA_STREAM_0, LL_DMA_PBURST_SINGLE);           // 外设突发：单次传输

    // ========== TIM3 基本参数配置 ==========
    TIM_InitStruct.Prescaler = 0;                          // 预分频器 = 0（不分频，定时器时钟 = 240MHz）
    TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;    // 向上计数模式（0 → ARR）
    TIM_InitStruct.Autoreload = 50000;                      // 自动重载值 = 2399（PWM 频率 = 240MHz / 2400 = 100kHz）
    TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;  // 时钟分频 = 1（不分频）
    LL_TIM_Init(TIM3, &TIM_InitStruct);                    // 应用配置到 TIM3
    LL_TIM_EnableARRPreload(TIM3);                         // 使能 ARR 预装载（ARR 修改在下一个周期生效）
    LL_TIM_SetClockSource(TIM3, LL_TIM_CLOCKSOURCE_INTERNAL);  // 时钟源：内部时钟（APB1 = 120MHz × 2 = 240MHz）
    
    // ========== TIM3_CH1 配置（PWM 输出到 PA6） ==========
    LL_TIM_OC_EnablePreload(TIM3, LL_TIM_CHANNEL_CH1);     // 使能 CH1 比较值预装载
    TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;         // PWM 模式 1（CNT < CCR1 时输出高电平）
    TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_DISABLE;    // 输出状态：初始禁用（需调用 BSP_TIM3_PWM0_Start() 启动）
    TIM_OC_InitStruct.OCNState = LL_TIM_OCSTATE_DISABLE;   // 互补输出：禁用
    TIM_OC_InitStruct.CompareValue = 15;                   // 比较值 = 15（占空比 = 15/2400 = 0.625%）
    TIM_OC_InitStruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH; // 输出极性：高电平有效
    LL_TIM_OC_Init(TIM3, LL_TIM_CHANNEL_CH1, &TIM_OC_InitStruct);  // 应用配置到 CH1
    LL_TIM_OC_DisableFast(TIM3, LL_TIM_CHANNEL_CH1);       // 禁用快速模式（标准 PWM 模式）
    
    // ========== TIM3_CH2 配置（DMA 触发源，不输出 PWM） ==========
    TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_FROZEN;       // 冻结模式（不影响输出引脚，仅用于触发 DMA）
    TIM_OC_InitStruct.CompareValue = 10000;                  // 比较值 = 200（触发时刻：CNT = 200）
    LL_TIM_OC_Init(TIM3, LL_TIM_CHANNEL_CH2, &TIM_OC_InitStruct);  // 应用配置到 CH2
    LL_TIM_OC_DisableFast(TIM3, LL_TIM_CHANNEL_CH2);       // 禁用快速模式
    LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_OC2REF);     // 触发输出：CH2 比较事件（用于触发 DMA 或其他外设）
    LL_TIM_DisableMasterSlaveMode(TIM3);                   // 禁用主从模式（TIM3 作为独立定时器）
    LL_TIM_OC_EnablePreload(TIM3, LL_TIM_CHANNEL_CH2);     // 使能 CH2 比较值预装载

    // ========== GPIO 配置（PA6 = TIM3_CH1 PWM 输出） ==========
    LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);   // 使能 GPIOA 时钟
    GPIO_InitStruct.Pin = LL_GPIO_PIN_6;                   // 引脚：PA6
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;         // 复用功能模式
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;        // 速度：低速（PWM 频率 100kHz 足够）
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;  // 输出类型：推挽输出
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;                // 上下拉：无
    GPIO_InitStruct.Alternate = LL_GPIO_AF_2;              // 复用功能 2：TIM3_CH1
    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);                 // 应用配置到 PA6
    NVIC_SetPriority(DMA1_Stream0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
}

// ...existing code...

/**
  * @brief  启动 TIM3 PWM 输出
  * @note   必须在 BSP_TIM3_PWM0_Init() 之后调用
  * @retval None
  */
void BSP_TIM3_PWM0_Start(void)
{
  LL_TIM_ClearFlag_CC2(TIM3);                         // 清除CC2比较事件标志位
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH1);  // 使能通道 1 输出（开始输出 PWM 波形）
  LL_TIM_EnableCounter(TIM3);                         // 使能定时器计数器（开始计数 0→ARR）
}

/**
  * @brief  停止 TIM3 PWM 输出
  * @note   停止后可通过 BSP_TIM3_PWM0_Start() 重新启动
  * @retval None
  */
void BSP_TIM3_PWM0_Stop(void)
{
  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH1); // 禁用通道 1 输出（停止 PWM 波形）
  LL_TIM_DisableCounter(TIM3);                        // 禁用定时器计数器（停止计数）
}

/**
  * @brief  动态调整 TIM3 PWM 频率
  * @param  frequency_hz 目标频率（Hz），范围：1Hz ~ 1MHz
  * @note   修改 ARR 值来改变 PWM 频率，占空比保持不变
  *         新频率 = 240MHz / (Prescaler+1) / (Autoreload+1)
  * @retval BSP_Status_t BSP_OK: 设置成功, BSP_ERROR: 参数错误
  */
BSP_Status_t BSP_TIM3_PWM0_SetFrequency(uint32_t frequency_hz)
{
  if (frequency_hz == 0 || frequency_hz > 1000000)    // 参数范围检查：1Hz ~ 1MHz
  {
    return BSP_ERROR;                                 // 频率超出合理范围
  }
  
  uint32_t arr = (240000000 / frequency_hz) - 1;      // 计算新的 ARR 值（TIM3 时钟 = 240MHz）
  LL_TIM_SetAutoReload(TIM3, arr);                    // 更新自动重载值（立即生效）
  
  return BSP_OK;
}

/**
  * @brief  动态调整 TIM3 PWM 占空比
  * @param  duty_percent 占空比百分比（0.0 ~ 100.0）
  * @note   修改 CCR1 值来改变占空比，频率保持不变
  *         新 CCR1 = ARR × (duty_percent / 100)
  * @retval BSP_Status_t BSP_OK: 设置成功, BSP_ERROR: 参数错误
  */
BSP_Status_t BSP_TIM3_PWM0_SetDutyCycle(float duty_percent)
{
  if (duty_percent < 0.0f || duty_percent > 100.0f)   // 参数范围检查：0% ~ 100%
  {
    return BSP_ERROR;                                 // 占空比超出范围
  }
  
  uint32_t arr = LL_TIM_GetAutoReload(TIM3);          // 获取当前 ARR 值
  uint32_t ccr = (uint32_t)((arr + 1) * duty_percent / 100.0f);  // 计算新的 CCR1 值
  LL_TIM_OC_SetCompareCH1(TIM3, ccr);                 // 更新比较值（下一个周期生效）
  
  return BSP_OK;
}