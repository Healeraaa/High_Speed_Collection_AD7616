#include "main.h"
#include "bsp.h"
#include "boot_app.h"
#include "boot_service.h"

/*
 * Boot 的顶层启动流程
 * ===================
 *
 * 上电后先完成最基本的时钟、HAL、DWT 延时和 LED 初始化，然后读取 K1：
 *
 *   K1 按下
 *      └─进入 Boot_Service_Run()，等待上位机通过 USART1 升级。
 *
 *   K1 未按下
 *      ├─App 完整有效：清理 Boot 环境并跳转到 App。
 *      └─App 无效：留在 Boot，LED 慢闪，避免执行残缺固件。
 *
 * main.c 只负责“选择运行哪条路径”，具体工作分别交给：
 *   boot_service.c：串口升级服务；
 *   boot_app.c：App 校验和跳转。
 */
int main(void)
{
    /*
     * 初始化 HAL、系统时钟、DWT 延时和状态 LED。
     * 这些资源既用于检测 K1，也用于 Boot 模式下的串口超时和状态指示。
     */
    BSP_Init();

    /* PA0 配置为上拉输入：松开为高电平，按下 K1 为低电平。 */
    BSP_GPIO_BOOT_KEY_Init();

    /*
     * K1 连续约 50 ms 都为低电平才认为真正按下，避免机械抖动误触发。
     * Boot_Service_Run() 内部是永久循环，正常情况下不会返回 main()。
     */
    if (BSP_GPIO_BOOT_KEY_IsPressed())
    {
        Boot_Service_Run();
    }

    /*
     * 未按 K1 时尝试正常启动 App。
     * 除了检查向量表，还会检查升级提交标记和整个 App 的 CRC32。
     */
    if (Boot_App_IsValid())
    {
        /* 成功跳转后不会返回。 */
        Boot_App_Jump();
    }

    /*
     * 能运行到这里说明 App 不存在、升级未完成或 CRC 校验失败。
     * 此时绝不能跳入 App，只通过 PC13 LED 慢闪提示，并等待重新上电按 K1升级。
     */
    while (1)
    {
        LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_13);
        BSP_DWT_Delay_ms(100U);
    }
}
