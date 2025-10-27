#include "bsp_timer.h"
#include "bsp.h"
#include "Module.h"


// ==================== SysTick ====================
void BSP_TIMER_SysTick_Init(void)
{
  SysTick_Config(SystemCoreClock / 1000);// 配置SysTick定时器中断时间为1ms
}

void BSP_SysTick_ISR(void)
{
	static uint8_t s_count = 0;

	if (++s_count >= 10)
	{
		s_count = 0;
		BSP_RunPer10ms();
    Module_RunPer10ms();
	}
}

void SysTick_Handler(void)
{
  BSP_SysTick_ISR();
}

