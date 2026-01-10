#ifndef __BSP_TIMER_H__
#define __BSP_TIMER_H__


#include "main.h"
// #include "bsp.h"

#ifndef BSP_STATUS_T_DEFINED
#define BSP_STATUS_T_DEFINED
typedef enum {
    BSP_OK       = 0x00,
    BSP_ERROR    = 0x01,
    BSP_BUSY     = 0x02,
    BSP_TIMEOUT  = 0x03
} BSP_Status_t;
#endif

// ========================================================================== DWT ==========================================================================
void BSP_DWT_DelayInit(void);
void BSP_DWT_Delay_us(uint32_t us);
void BSP_DWT_Delay_ms(uint32_t ms);
void BSP_DWT_Delay_s(uint32_t s);
uint32_t BSP_DWT_GetCycles(void);

// ========================================================================== TIM3 ==========================================================================
typedef struct 
{
    uint32_t TIM_CLK;        // 定时器时钟频率
    uint32_t TIM_MAX_ARR;    // 自动重装载寄存器最大值
    uint32_t TIM_MAX_PSC;    // 预分频器最大值
} BSP_TIM_Config_t;

BSP_TIM_Config_t BSP_Get_TIM3_Config(void);
void BSP_TIM3_PWM0_Init(void);
void BSP_TIM3_PWM0_Start(void);
void BSP_TIM3_PWM0_Stop(void);
BSP_Status_t BSP_TIM3_PWM0_SetParams(uint32_t psc, uint32_t arr, uint32_t cpv1);

#endif 

