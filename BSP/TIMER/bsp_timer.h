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


// ========================================================================== TIM2 ==========================================================================

/**
  * @brief  初始化 TIM2 为外部脉冲计数模式
  * @note   使用 PA1 (TIM2_CH2) 作为外部脉冲输入
  *         TIM2 是 32 位定时器，计数范围 0 ~ 0xFFFFFFFF
  * @retval None
  */
void BSP_TIM2_PULSE_Init(void);

/**
  * @brief  启动 TIM2 脉冲计数
  * @note   必须在 BSP_TIM2_PULSE_Init() 之后调用
  * @retval None
  */
void BSP_TIM2_PULSE_Start(void);

/**
  * @brief  停止 TIM2 脉冲计数
  * @retval None
  */
void BSP_TIM2_PULSE_Stop(void);

/**
  * @brief  读取 TIM2 脉冲计数值
  * @retval uint32_t 当前脉冲计数值（0 ~ 0xFFFFFFFF）
  */
uint32_t BSP_TIM2_PULSE_GetCount(void);

/**
  * @brief  清除 TIM2 脉冲计数值（归零）
  * @retval None
  */
void BSP_TIM2_PULSE_ClearCount(void);

/**
  * @brief  读取并清除 TIM2 脉冲计数值（原子操作）
  * @note   读取当前计数值后立即清零，适用于周期性采样场景
  * @retval uint32_t 清零前的脉冲计数值
  */
uint32_t BSP_TIM2_PULSE_GetAndClearCount(void);

/**
  * @brief  检查 TIM2 计数器是否正在运行
  * @retval uint32_t 1: 正在运行, 0: 已停止
  */
uint32_t BSP_TIM2_PULSE_IsRunning(void);

#endif 

