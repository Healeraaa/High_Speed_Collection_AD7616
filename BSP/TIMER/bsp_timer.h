#ifndef __BSP_TIMER_H__
#define __BSP_TIMER_H__


#include "main.h"

// ========================================================================== DWT ==========================================================================
void BSP_DWT_DelayInit(void);
void BSP_DWT_Delay_us(uint32_t us);
void BSP_DWT_Delay_ms(uint32_t ms);
void BSP_DWT_Delay_s(uint32_t s);
uint32_t BSP_DWT_GetCycles(void);

// ========================================================================== TIM3 ==========================================================================
void BSP_TIM3_PWM0_Init(void);
void BSP_TIM3_PWM0_Start(void);
void BSP_TIM3_PWM0_Stop(void);

#endif 

