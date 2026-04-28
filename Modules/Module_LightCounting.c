#include "Module_LightCounting.h"
#include "bsp_gpio.h"
#include "stdio.h"

void Module_LightCounting_Init(void)
{
    BSP_TIM2_PULSE_Init();// 初始化TIM2脉冲计数器
}

void Module_LightCounting_Start(void)
{
    BSP_TIM2_PULSE_Start();
}

void Module_LightCounting_Stop(void)
{
    BSP_TIM2_PULSE_Stop();
}

uint32_t Module_LightCounting_GetCount(void)
{
    return BSP_TIM2_PULSE_GetCount();
}
void Module_LightCounting_ClearCount(void)
{
    BSP_TIM2_PULSE_ClearCount();
}

uint32_t Module_LightCounting_GetAndClearCount(void)
{
    return BSP_TIM2_PULSE_GetAndClearCount();
}


/**
 * @brief  TIM4 全局中断服务程序
 * @note   由 NVIC 自动调用（中断向量表中注册）
 *         执行周期：每 10ms 执行一次（根据 Prescaler 和 Autoreload 参数）
 *         应该在此函数中添加用户的 10ms 周期处理逻辑
 * @retval None
 */
void TIM4_IRQHandler(void)
{
  static uint32_t tick_count = 0; // 定义一个静态变量用于记录中断次数（全局计数器）
  // 检查是否是 UPDATE 中断（计数器复位时产生）
  if (LL_TIM_IsActiveFlag_UPDATE(TIM4))
  {

    LL_TIM_ClearFlag_UPDATE(TIM4);
  }
}