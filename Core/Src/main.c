#include "main.h"
#include "bsp.h"
#include "boot_app.h"
#include "boot_service.h"

int main(void)
{
    BSP_Init();
    BSP_GPIO_BOOT_KEY_Init();

    /* 上电时按住 K1，强制停留在串口升级模式。 */
    if (BSP_GPIO_BOOT_KEY_IsPressed())
    {
        Boot_Service_Run();
    }

    /* 未按 K1 时，仅允许启动通过完整校验的 App。 */
    if (Boot_App_IsValid())
    {
        Boot_App_Jump();
    }

    /* 没有有效 App：留在 Boot，并用 LED 指示当前状态。 */
    while (1)
    {
        LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_13);
        BSP_DWT_Delay_ms(1000U);
    }
}
