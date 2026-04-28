#include "main.h"
#include "memorymap.h"
#include "sys.h"
#include "bsp.h"




void BSP_Init(void)
{
  MPU_Config();//配置MPU 

  SCB_EnableICache();// 使能指令缓存 I-Cache 
  SCB_EnableDCache();// 使能数据缓存 D-Cache
	
	HAL_Init();
  SRAM_ClockEnable();// 使能SRAM时钟
  LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_SYSCFG);// 使能SYSCFG时钟
  
  NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);//  配置NVIC中断优先级分组为4，即4位抢占优先级，0位子优先级 
  NVIC_SetPriority(SysTick_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),15, 0));// 设置SysTick中断优先级为最低15

  SystemClock_Config();//系统时钟配置

  BSP_DWT_DelayInit();// 初始化DWT延迟函数

  BSP_GPIO_System_Clock_Init();// 初始化GPIO时钟

  BSP_GPIO_LED_Init();// 初始化LED相关的GPIO 
  BSP_USART1_Init();// 初始化USART1
  BSP_USART3_Init();// 初始化USART3
  BSP_DWT_Delay_ms(1000);

  BSP_TIM4_COUNT_Init();// 初始化 TIM4 计数定时器（中断模式）
  BSP_TIM4_COUNT_Start();// 启动 TIM4 计数器（每 10ms 产生一次中断）

  
  

  

  
}

void BSP_RunPer10ms(void)
{
}
