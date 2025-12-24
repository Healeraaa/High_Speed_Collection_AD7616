#include "bsp_timer.h"
#include "bsp.h"
#include "Module.h"


// ========================================================================== DWT ==========================================================================
/**
 * @brief 初始化DWT CYCCNT
 * @note 需要在使用延迟函数前调用一次
 */
void BSP_DWT_DelayInit(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;  // 使能DWT
    DWT->CYCCNT = 0;                                  // 清零计数器
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;             // 使能CYCCNT计数器
} 

/**
 * @brief 微秒级延迟函数
 * @param us 延迟时间(微秒)
 * @note 基于DWT CYCCNT实现的精确延迟
 */
void BSP_DWT_Delay_us(uint32_t us)
{
    uint32_t start_ticks = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);  // 计算需要的时钟周期数
    
    while ((DWT->CYCCNT - start_ticks) < ticks);
}

/**
 * @brief 毫秒级延迟函数
 * @param ms 延迟时间(毫秒)
 */
void BSP_DWT_Delay_ms(uint32_t ms)
{
    uint32_t start_ticks = DWT->CYCCNT;
    uint32_t ticks = ms * (SystemCoreClock / 1000);  // 计算需要的时钟周期数
    
    while ((DWT->CYCCNT - start_ticks) < ticks);
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


