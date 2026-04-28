// #include "bsp_timer.h"
#include "bsp.h"
#include "Module.h"

// ========================================================================== DWT ==========================================================================
/**
 * @brief 初始化DWT CYCCNT
 * @note 需要在使用延迟函数前调用一次
 */
void BSP_DWT_DelayInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // 使能DWT
  DWT->CYCCNT = 0;                                // 清零计数器
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // 使能CYCCNT计数器
}

/**
 * @brief 微秒级延迟函数
 * @param us 延迟时间(微秒)
 * @note 基于DWT CYCCNT实现的精确延迟
 */
void BSP_DWT_Delay_us(uint32_t us)
{
  uint32_t start_ticks = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000); // 计算需要的时钟周期数

  while ((DWT->CYCCNT - start_ticks) < ticks)
    ;
}

/**
 * @brief 毫秒级延迟函数
 * @param ms 延迟时间(毫秒)
 */
void BSP_DWT_Delay_ms(uint32_t ms)
{
  uint32_t start_ticks = DWT->CYCCNT;
  uint32_t ticks = ms * (SystemCoreClock / 1000); // 计算需要的时钟周期数

  while ((DWT->CYCCNT - start_ticks) < ticks)
    ;
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
BSP_TIM_Config_t TIM3_Config = {
    .TIM_CLK = 240000000,  // 定时器时钟频率（APB1 时钟 120MHz × 2）
    .TIM_MAX_ARR = 0xFFFF, // 16位自动重装载寄存器
    .TIM_MAX_PSC = 0xFFFF  // 16 位预分频器最大值
};

BSP_TIM_Config_t BSP_Get_TIM3_Config(void)
{
  return TIM3_Config;
}

void BSP_TIM3_PWM0_Init(void)
{

  LL_TIM_InitTypeDef TIM_InitStruct = {0};
  LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3); // 使能 TIM3 外设时钟（APB1 总线）

  // ========== TIM3 基本参数配置 ==========
  TIM_InitStruct.Prescaler = 0;                             // 预分频器 = 0（不分频，定时器时钟 = 240MHz）
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;       // 向上计数模式（0 → ARR）
  TIM_InitStruct.Autoreload = 50000;                        // 自动重载值 = 2399（PWM 频率 = 240MHz / 2400 = 100kHz）
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1; // 时钟分频 = 1（不分频）
  LL_TIM_Init(TIM3, &TIM_InitStruct);                       // 应用配置到 TIM3
  LL_TIM_EnableARRPreload(TIM3);                            // 使能 ARR 预装载（ARR 修改在下一个周期生效）
  LL_TIM_SetClockSource(TIM3, LL_TIM_CLOCKSOURCE_INTERNAL); // 时钟源：内部时钟（APB1 = 120MHz × 2 = 240MHz）

  // ========== TIM3_CH1 配置（PWM 输出到 PA6） ==========
  LL_TIM_OC_EnablePreload(TIM3, LL_TIM_CHANNEL_CH1);            // 使能 CH1 比较值预装载
  TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;                // PWM 模式 1（CNT < CCR1 时输出高电平）
  TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_DISABLE;           // 输出状态：初始禁用（需调用 BSP_TIM3_PWM0_Start() 启动）
  TIM_OC_InitStruct.OCNState = LL_TIM_OCSTATE_DISABLE;          // 互补输出：禁用
  TIM_OC_InitStruct.CompareValue = 15;                          // 比较值 = 15（占空比 = 15/2400 = 0.625%）
  TIM_OC_InitStruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;        // 输出极性：高电平有效
  LL_TIM_OC_Init(TIM3, LL_TIM_CHANNEL_CH1, &TIM_OC_InitStruct); // 应用配置到 CH1
  LL_TIM_OC_DisableFast(TIM3, LL_TIM_CHANNEL_CH1);              // 禁用快速模式（标准 PWM 模式）

  // ========== GPIO 配置（PA6 = TIM3_CH1 PWM 输出） ==========
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);  // 使能 GPIOA 时钟
  GPIO_InitStruct.Pin = LL_GPIO_PIN_6;                  // 引脚：PA6
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;        // 复用功能模式
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;       // 速度：低速（PWM 频率 100kHz 足够）
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL; // 输出类型：推挽输出
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;               // 上下拉：无
  GPIO_InitStruct.Alternate = LL_GPIO_AF_2;             // 复用功能 2：TIM3_CH1
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);                // 应用配置到 PA6
}

/**
 * @brief  启动 TIM3 PWM 输出
 * @note   必须在 BSP_TIM3_PWM0_Init() 之后调用
 * @retval None
 */
void BSP_TIM3_PWM0_Start(void)
{
  LL_TIM_GenerateEvent_UPDATE(TIM3);
  LL_TIM_ClearFlag_UPDATE(TIM3);

  // 使能通道和计数器
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH1);
  LL_TIM_EnableCounter(TIM3);
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
  LL_TIM_ClearFlag_UPDATE(TIM3);
}

/**
 * @brief  设置 TIM3 PWM 定时器参数
 * @param  psc 预分频器值 (0 ~ 65535)
 * @param  arr 自动重载值 (0 ~ 65535)
 * @param  cpv1 CH1 比较值 (PWM 输出控制，0 ~ arr)
 * @param  cpv2 CH2 比较值 (DMA 触发时刻，0 ~ arr)
 * @note   参数在下一个 TIM3 更新事件生效（预装载机制）
 * @retval BSP_Status_t BSP_OK: 设置成功, BSP_ERROR: 参数错误
 */
BSP_Status_t BSP_TIM3_PWM0_SetParams(uint32_t psc, uint32_t arr, uint32_t cpv1)
{
  // 参数有效性检查
  if (psc > TIM3_Config.TIM_MAX_PSC || arr > TIM3_Config.TIM_MAX_ARR || cpv1 > arr)
  {
    return BSP_ERROR;
  }

  LL_TIM_SetPrescaler(TIM3, psc);      // 设置预分频器
  LL_TIM_SetAutoReload(TIM3, arr);     // 设置自动重载值
  LL_TIM_OC_SetCompareCH1(TIM3, cpv1); // 设置 CH1 比较值（PWM 占空比）
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
  if (duty_percent < 0.0f || duty_percent > 100.0f) // 参数范围检查：0% ~ 100%
  {
    return BSP_ERROR; // 占空比超出范围
  }

  uint32_t arr = LL_TIM_GetAutoReload(TIM3);                    // 获取当前 ARR 值
  uint32_t ccr = (uint32_t)((arr + 1) * duty_percent / 100.0f); // 计算新的 CCR1 值
  LL_TIM_OC_SetCompareCH1(TIM3, ccr);                           // 更新比较值（下一个周期生效）

  return BSP_OK;
}

// ========================================================================== TIM2 ==========================================================================

void BSP_TIM2_PULSE_Init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct = {0};
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  // 使能时钟
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);

  // 配置 GPIO (PA1 -> TIM2_CH2)
  GPIO_InitStruct.Pin = LL_GPIO_PIN_1;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH; // 提高带宽
  // GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  // GPIO_InitStruct.Pull       = LL_GPIO_PULL_UP;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // 配置 TIM2 基本参数
  TIM_InitStruct.Prescaler = 0;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 0xFFFFFFFF; // TIM2 是 32 位定时器，全量程计数
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  LL_TIM_Init(TIM2, &TIM_InitStruct);

  LL_TIM_DisableARRPreload(TIM2);

  // 配置外部时钟模式 1 (Slave Mode: External Clock Mode 1)
  // 选择 TI2FP2 (通道2 经过滤波器和极性选择后的信号) 作为触发源
  LL_TIM_SetTriggerInput(TIM2, LL_TIM_TS_TI2FP2);
  LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_EXT_MODE1);

  // 配置通道 2 的输入特性
  LL_TIM_IC_SetActiveInput(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_ACTIVEINPUT_DIRECTTI); // ***设置为直接输入
  LL_TIM_IC_SetFilter(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_FILTER_FDIV1);
  LL_TIM_IC_SetPolarity(TIM2, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_RISING);

  LL_TIM_CC_EnableChannel(TIM2, LL_TIM_CHANNEL_CH2);

  // 强制更新以同步配置并清零计数器
  LL_TIM_GenerateEvent_UPDATE(TIM2);
  LL_TIM_ClearFlag_UPDATE(TIM2);
  LL_TIM_SetCounter(TIM2, 0); // 确保从 0 开始计数

  // 其他辅助设置
  LL_TIM_DisableIT_TRIG(TIM2);
  LL_TIM_DisableDMAReq_TRIG(TIM2);
  LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_RESET);
  LL_TIM_DisableMasterSlaveMode(TIM2);
}

/**
 * @brief  启动 TIM2 脉冲计数
 * @note   必须在 BSP_TIM2_PULSE_Init() 之后调用
 * @retval None
 */
void BSP_TIM2_PULSE_Start(void)
{
  LL_TIM_SetCounter(TIM2, 0);    // 清零计数器
  LL_TIM_ClearFlag_UPDATE(TIM2); // 清除更新标志
  LL_TIM_EnableCounter(TIM2);    // 使能计数器
}

/**
 * @brief  停止 TIM2 脉冲计数
 * @retval None
 */
void BSP_TIM2_PULSE_Stop(void)
{
  LL_TIM_DisableCounter(TIM2); // 禁用计数器
}

/**
 * @brief  读取 TIM2 脉冲计数值
 * @retval uint32_t 当前脉冲计数值（0 ~ 0xFFFFFFFF）
 */
uint32_t BSP_TIM2_PULSE_GetCount(void)
{
  return LL_TIM_GetCounter(TIM2);
}

/**
 * @brief  清除 TIM2 脉冲计数值（归零）
 * @retval None
 */
void BSP_TIM2_PULSE_ClearCount(void)
{
  LL_TIM_SetCounter(TIM2, 0);
}

/**
 * @brief  读取并清除 TIM2 脉冲计数值（原子操作）
 * @note   读取当前计数值后立即清零，适用于周期性采样场景
 * @retval uint32_t 清零前的脉冲计数值
 */
uint32_t BSP_TIM2_PULSE_GetAndClearCount(void)
{
  uint32_t count = LL_TIM_GetCounter(TIM2);
  LL_TIM_SetCounter(TIM2, 0);
  return count;
}

/**
 * @brief  检查 TIM2 计数器是否正在运行
 * @retval uint32_t 1: 正在运行, 0: 已停止
 */
uint32_t BSP_TIM2_PULSE_IsRunning(void)
{
  return LL_TIM_IsEnabledCounter(TIM2);
}

// ========================================================================== TIM4 ==========================================================================

/**
 * @brief  初始化 TIM4 为计数定时器（中断模式）
 * @note   配置参数：
 *         - 预分频器 (Prescaler) = 2399（分频系数 2400）
 *         - 自动重载值 (Autoreload) = 999（计数到 1000 复位）
 *         - 时钟源 = 内部时钟（APB1_CLK × 2 = 120MHz × 2 = 240MHz）
 *         - 中断周期 = (2400 × 1000) / 240MHz = 10ms
 *         
 *         初始化后需要调用 BSP_TIM4_COUNT_Start() 启动计数器
 * @retval None
 */
void BSP_TIM4_COUNT_Init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct = {0};
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);// 使能 TIM4 外设时钟（APB1 总线）
  
  NVIC_SetPriority(TIM4_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));// 配置 NVIC 中断优先级（抢占优先级 5，子优先级 0）
  NVIC_EnableIRQ(TIM4_IRQn);// 使能 TIM4 全局中断

  // 配置定时器基本参数
  TIM_InitStruct.Prescaler = 2399;                             // 预分频器：2400（1 ~ 65536）
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;          // 计数方向：向上
  TIM_InitStruct.Autoreload = 999;                             // 自动重载：1000
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;    // 时钟分频：1（无分频）

  LL_TIM_Init(TIM4, &TIM_InitStruct);
  LL_TIM_EnableARRPreload(TIM4);// 使能 ARR 预装载（修改在下一个更新事件生效
  LL_TIM_SetClockSource(TIM4, LL_TIM_CLOCKSOURCE_INTERNAL);// 设置时钟源为内部时钟
  LL_TIM_SetTriggerOutput(TIM4, LL_TIM_TRGO_RESET);// 配置主模式：触发输出为复位信号
  LL_TIM_DisableMasterSlaveMode(TIM4);// 禁用主从模式
  LL_TIM_EnableIT_UPDATE(TIM4);// 启用 TIM4 更新中断（计数器复位时产生中断）
}

/**
 * @brief  启动 TIM4 计数器
 * @note   必须在 BSP_TIM4_COUNT_Init() 之后调用
 *         启动后每 10ms 产生一次更新中断（调用 TIM4_IRQHandler）
 * @retval None
 */
void BSP_TIM4_COUNT_Start(void)
{
  LL_TIM_SetCounter(TIM4, 0);// 清零计数器
  LL_TIM_ClearFlag_UPDATE(TIM4);// 清除更新中断标志
  LL_TIM_EnableCounter(TIM4);// 启动计数器
}

/**
 * @brief  停止 TIM4 计数器
 * @note   停止后计数器暂停，不再产生中断
 *         可通过 BSP_TIM4_COUNT_Start() 重新启动
 * @retval None
 */
void BSP_TIM4_COUNT_Stop(void)
{
  LL_TIM_DisableCounter(TIM4);// 禁用计数器（停止计数）
}

/**
 * @brief  设置 TIM4 计数定时器参数（预分频器和自动重载值）
 * @param  psc 预分频器值 (0 ~ 65535)
 *         分频系数 = psc + 1
 *         范围：1 ~ 65536
 * @param  arr 自动重载值 (0 ~ 65535)
 *         计数范围 = 0 ~ arr
 *         中断在计数器达到 arr 后复位时产生
 * @note   中断周期计算公式：
 *         T_interrupt = (psc + 1) × (arr + 1) / TIM4_CLK
 *         其中 TIM4_CLK = 240MHz
 *         
 *         示例：
 *         - psc=2399, arr=999 → T = 2400 × 1000 / 240MHz = 10ms
 *         - psc=2399, arr=1999 → T = 2400 × 2000 / 240MHz = 20ms
 *         - psc=4799, arr=999 → T = 4800 × 1000 / 240MHz = 20ms
 *         
 *         参数在下一个 UPDATE 事件时生效（预装载机制）
 * @retval BSP_Status_t
 *         BSP_OK    - 设置成功
 *         BSP_ERROR - 参数超出范围
 */
BSP_Status_t BSP_TIM4_COUNT_SetParams(uint32_t psc, uint32_t arr)
{
  // 参数有效性检查（16 位定时器，最大值 65535）
  if (psc > 0xFFFF || arr > 0xFFFF)
  {
    return BSP_ERROR;
  }

  // 获取当前计数器运行状态
  uint32_t is_running = LL_TIM_IsEnabledCounter(TIM4);
  
  // 如果计数器运行中，暂停以更新参数（防止参数更新时产生异常）
  if (is_running)
  {
    LL_TIM_DisableCounter(TIM4);
  }

  // 更新预分频器和自动重载值
  LL_TIM_SetPrescaler(TIM4, psc);
  LL_TIM_SetAutoReload(TIM4, arr);
  
  // 生成 UPDATE 事件强制立即加载新参数（不需等待下一个计数周期）
  LL_TIM_GenerateEvent_UPDATE(TIM4);
  
  // 清除 UPDATE 中断标志，防止强制 UPDATE 产生中断
  LL_TIM_ClearFlag_UPDATE(TIM4);

  // 恢复计数器运行状态
  if (is_running)
  {
    LL_TIM_EnableCounter(TIM4);
  }

  return BSP_OK;
}
