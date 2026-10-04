#include "main.h"
#include "memorymap.h"
#include "sys.h"
#include "bsp.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"




void BSP_Init(void)
{
    HAL_Init();

    SystemClock_Config();

    BSP_DWT_DelayInit();

    BSP_GPIO_System_Clock_Init();

    BSP_GPIO_LED_Init();
}

void BSP_RunPer10ms(void)
{
}
