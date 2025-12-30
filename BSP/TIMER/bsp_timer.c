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
// ...existing code...

/**
  * @brief  初始化 TIM3 PWM 通道 0 (CH1) 用于 AD7616 采样触发
  * @note   配置 PWM 频率为 100kHz (10us 周期)，占空比 0.625% (150ns 脉冲宽度)
  *         TIM3 时钟源：APB1 Timer Clock = 240MHz (HCLK/2 × 2)
  *         PWM 频率计算：f_pwm = 240MHz / (Prescaler+1) / (Autoreload+1)
  *                            = 240MHz / 1 / 2400 = 100kHz
  *         脉冲宽度计算：T_pulse = (CompareValue / Autoreload) × T_period
  *                              = (15 / 2400) × 10us = 62.5ns
  *         
  *         DMA 配置用于捕获 PWM 输入信号（此处配置为 TIM3_CH1 触发 DMA1 Stream0）
  * @retval None
  */
void BSP_TIM3_PWM0_Init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct = {0};                                      // 定时器基本配置结构体
  LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};                                // 输出比较配置结构体
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};                                    // GPIO 配置结构体
  
  /* ========== 使能外设时钟 ========== */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);                           // 使能 TIM3 时钟（APB1 总线）

  /* ========== 配置 DMA1 Stream0 用于 TIM3_CH1 捕获 ========== */
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_STREAM_0, LL_DMAMUX1_REQ_TIM3_CH1);     // 绑定 DMA 请求源：TIM3_CH1
  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_STREAM_0, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);  // 传输方向：外设→内存（捕获模式）
  LL_DMA_SetStreamPriorityLevel(DMA1, LL_DMA_STREAM_0, LL_DMA_PRIORITY_VERYHIGH);  // DMA 优先级：最高（确保采样不丢失）
  LL_DMA_SetMode(DMA1, LL_DMA_STREAM_0, LL_DMA_MODE_CIRCULAR);                 // 循环模式：连续捕获数据
  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_STREAM_0, LL_DMA_PERIPH_NOINCREMENT);   // 外设地址不递增（固定读取 TIM3->CCR1）
  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_0, LL_DMA_MEMORY_INCREMENT);     // 内存地址递增（写入不同缓冲区位置）
  LL_DMA_SetPeriphSize(DMA1, LL_DMA_STREAM_0, LL_DMA_PDATAALIGN_HALFWORD);     // 外设数据宽度：16 位（TIM3->CCR1 寄存器宽度）
  LL_DMA_SetMemorySize(DMA1, LL_DMA_STREAM_0, LL_DMA_MDATAALIGN_HALFWORD);     // 内存数据宽度：16 位（匹配 AD7616 数据格式）
  LL_DMA_EnableFifoMode(DMA1, LL_DMA_STREAM_0);                                 // 使能 FIFO 模式（提高 DMA 传输效率）
  LL_DMA_SetFIFOThreshold(DMA1, LL_DMA_STREAM_0, LL_DMA_FIFOTHRESHOLD_FULL);   // FIFO 阈值：满（累积 4 个数据后触发传输）
  LL_DMA_SetMemoryBurstxfer(DMA1, LL_DMA_STREAM_0, LL_DMA_MBURST_SINGLE);      // 内存突发传输：单次（兼容性最佳）
  LL_DMA_SetPeriphBurstxfer(DMA1, LL_DMA_STREAM_0, LL_DMA_PBURST_SINGLE);      // 外设突发传输：单次（TIM3 不支持突发）
  
  /* ========== 配置 TIM3 基本参数 ========== */
  TIM_InitStruct.Prescaler = 0;                                                 // 预分频器：不分频（TIM3 时钟 = 240MHz）
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;                           // 计数模式：向上计数（0→ARR）
  TIM_InitStruct.Autoreload = 2399;                                             // 自动重载值：2400 周期 → PWM 频率 = 100kHz (10us)
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;                     // 时钟分频：不分频（用于死区时间计算）
  LL_TIM_Init(TIM3, &TIM_InitStruct);                                           // 初始化定时器基本参数
  LL_TIM_EnableARRPreload(TIM3);                                                // 使能 ARR 预装载（防止计数器溢出时产生毛刺）
  LL_TIM_SetClockSource(TIM3, LL_TIM_CLOCKSOURCE_INTERNAL);                    // 时钟源：内部时钟（CK_INT = 240MHz）
  
  /* ========== 配置 TIM3 通道 1 输出比较（PWM 模式）========== */
  LL_TIM_OC_EnablePreload(TIM3, LL_TIM_CHANNEL_CH1);                            // 使能 CCR1 预装载（防止占空比跳变）
  TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;                                // PWM 模式 1：CNT < CCR1 时输出高电平
  TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_DISABLE;                           // 输出状态：禁用（需手动使能后输出 PWM）
  TIM_OC_InitStruct.OCNState = LL_TIM_OCSTATE_DISABLE;                          // 互补输出：禁用（仅用于高级定时器）
  TIM_OC_InitStruct.CompareValue = 15;                                          // 比较值：15 → 占空比 = 15/2400 = 0.625% (62.5ns 脉冲)
  TIM_OC_InitStruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;                        // 输出极性：高电平有效
  LL_TIM_OC_Init(TIM3, LL_TIM_CHANNEL_CH1, &TIM_OC_InitStruct);                // 初始化输出比较通道 1
  LL_TIM_OC_DisableFast(TIM3, LL_TIM_CHANNEL_CH1);                              // 禁用快速模式（正常 PWM 模式）
  
  /* ========== 配置 TIM3 主模式输出（触发 ADC/DAC）========== */
  LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_OC1REF);                            // 主输出触发源：OC1REF (CCR1 比较匹配时触发)
  LL_TIM_DisableMasterSlaveMode(TIM3);                                          // 禁用主从模式（TIM3 作为主定时器）
  
  /* ========== 配置 TIM3_CH1 输出引脚（PA6）========== */
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);                          // 使能 GPIOA 时钟
  
  /**TIM3 GPIO Configuration
    PA6     ------> TIM3_CH1                                                    // PA6 复用为 TIM3 通道 1 输出
    */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_6;                                          // 选择引脚：PA6
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;                                // 模式：复用功能（TIM3_CH1）
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;                               // 速度：低速（50MHz，PWM 频率仅 100kHz）
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;                         // 输出类型：推挽输出（高低电平驱动能力强）
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;                                       // 上下拉：无（外部已有上拉/下拉）
  GPIO_InitStruct.Alternate = LL_GPIO_AF_2;                                     // 复用功能：AF2（TIM3/4/5）
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);                                        // 初始化 GPIO
}

/**
  * @brief  启动 TIM3 PWM 输出
  * @note   必须在 BSP_TIM3_PWM0_Init() 之后调用
  * @retval None
  */
void BSP_TIM3_PWM0_Start(void)
{
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