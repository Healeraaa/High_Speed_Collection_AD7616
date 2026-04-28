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


