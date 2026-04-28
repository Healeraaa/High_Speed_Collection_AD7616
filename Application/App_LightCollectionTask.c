#include "App_LightCollectionTask.h"
#include "App_TasksInit.h"
#include "main.h"
#include "Module_LightCounting.h"

uint32_t g_light_count = 0;// 全局变量用于存储光脉冲计数值

/**
 * @brief  TIM4 全局中断服务程序
 * @note   None
 * @retval None
 */
void TIM4_IRQHandler(void)
{
  static uint32_t tick_count = 0; // 定义一个静态变量用于记录中断次数（全局计数器）

  if (LL_TIM_IsActiveFlag_UPDATE(TIM4))  // 检查是否是 UPDATE 中断（计数器复位时产生）
  {
    g_light_count = Module_LightCounting_GetAndClearCount(); // 读取并清除计数值

    LL_TIM_ClearFlag_UPDATE(TIM4);
  }
}