#include "main.h"
#include "memorymap.h"
#include "sys.h"
#include "bsp.h"
#include "bsp_gpio.h"



void BSP_Init(void)
{
  
  MPU_Config();//配置MPU 
  SCB_EnableICache();// 使能指令缓存 I-Cache 
  SCB_EnableDCache();// 使能数据缓存 D-Cache

  
  LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_SYSCFG);// 使能SYSCFG时钟
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);//  配置NVIC中断优先级分组为4，即4位抢占优先级，0位子优先级 
  NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),15, 0));// 设置SysTick中断优先级为最低15

  SystemClock_Config();//系统时钟配置

#if Enable_EventRecorder == 1 
  EventRecorderInitialize(EventRecordAll, 1U); //初始化事件记录器  
  EventRecorderStart(); //启动事件记录器
#endif

  /* 初始化GPIO时钟 */
  BSP_GPIO_System_Clock_Init();
  /* 初始化LED相关的GPIO */
  BSP_GPIO_LED_Init();
}
