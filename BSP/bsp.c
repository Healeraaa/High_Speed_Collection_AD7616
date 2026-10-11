#include "main.h"
#include "memorymap.h"
#include "sys.h"
#include "bsp.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
/*
 * Boot实际使用的最小公共BSP初始化：
 * HAL/Tick -> 系统时钟 -> DWT延时 -> GPIO时钟 -> LED。
 *
 * 这里没有初始化USB、FreeRTOS和采集模块。USART1也不在这里初始化，
 * 只有确认进入升级模式后才由BSP_BOOT_USART1_Init()单独开启。
 */
void BSP_Init(void)
{
    /* 初始化HAL底层和1ms时基。 */
    HAL_Init();

    /* 配置STM32H743系统主频及各总线时钟。 */
    SystemClock_Config();

    /* 打开Cortex-M7 DWT计数器，供K1消抖和无效App状态灯延时使用。 */
    BSP_DWT_DelayInit();

    /* 开启基础GPIO端口时钟并配置PC13状态LED。 */
    BSP_GPIO_System_Clock_Init();

    BSP_GPIO_LED_Init();
}

void BSP_RunPer10ms(void)
{
}
