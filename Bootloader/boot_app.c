#include "boot_app.h"

#include "main.h"
#include "boot_crc32.h"
#include "boot_image_info.h"
#include "boot_protocol.h"

#define BOOT_APP_DTCM_START_ADDRESS  0x20000000UL
#define BOOT_APP_DTCM_END_ADDRESS    0x20020000UL

/**
 * @brief 跳转前清理 Boot 使用过的系统资源
 */
static void Boot_App_PrepareForJump(void)
{
    uint32_t index;

    /*
     * HAL_RCC_DeInit() 内部需要 HAL Tick，因此必须在关闭中断、
     * 停止 Tick 之前调用。
     */
    (void)HAL_RCC_DeInit();

    /* 复位 AHB/APB 总线外设。 */
    (void)HAL_DeInit();

    /* 从这里开始不再调用任何依赖 HAL Tick 或外设的函数。 */
    __disable_irq();

    /* 关闭 Boot 明确使用过的外设时钟。 */
    __HAL_RCC_USART1_CLK_DISABLE();
    __HAL_RCC_TIM6_CLK_DISABLE();
    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOC_CLK_DISABLE();
    __HAL_RCC_GPIOH_CLK_DISABLE();

    /* 清理 Cortex-M7 SysTick。 */
    SysTick->CTRL = 0UL;
    SysTick->LOAD = 0UL;
    SysTick->VAL = 0UL;

    /* 停止 Boot 使用的 DWT 周期计数器。 */
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0UL;

    /* 如果 Boot 启用了 Cache，则交给 App 重新配置。 */
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0UL)
    {
        SCB_DisableDCache();
    }

    if ((SCB->CCR & SCB_CCR_IC_Msk) != 0UL)
    {
        SCB_DisableICache();
    }

    HAL_MPU_Disable();

    /* 禁用并清除所有 NVIC 外部中断。 */
    for (index = 0U; index < 8U; index++)
    {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }

    /* 清除 PendSV 和 SysTick 挂起状态。 */
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk |
                SCB_ICSR_PENDSTCLR_Msk;

    /* 恢复 NVIC 优先级分组到复位值。 */
    NVIC_SetPriorityGrouping(0U);

    __DSB();
    __ISB();
}

/**
 * @brief 最终设置 CPU 状态并进入 App
 * @note ARMCC 5 汇编函数；切换 MSP 后不能再执行普通 C 代码
 *
 * R0 = App 初始 MSP
 * R1 = App Reset_Handler 地址
 */
__asm void Boot_App_Start(
    uint32_t app_msp,
    uint32_t app_reset)
{
    MOVS    R2, #0

    /* 使用特权线程模式和 MSP。 */
    MSR     CONTROL, R2

    /* 清除中断屏蔽状态。 */
    MSR     BASEPRI, R2
    MSR     FAULTMASK, R2

    /* PSP 恢复为复位值。 */
    MSR     PSP, R2

    DSB
    ISB

    /* 从这里开始不能再访问 Boot 的栈和局部变量。 */
    MSR     MSP, R0

    /* 恢复全局中断，匹配 CPU 复位后的状态。 */
    CPSIE   I

    DSB
    ISB

    /* 跳入 App 的 Reset_Handler，不再返回。 */
    BX      R1
}

uint8_t Boot_App_IsValid(void)
{
    uint32_t app_msp;
    uint32_t app_reset;
    uint32_t reset_address;
    uint32_t image_size;
    uint32_t expected_crc32;
    uint32_t calculated_crc32;

    /* 升级未完成、元数据损坏或提交标记不存在时禁止启动。 */
    if (Boot_ImageInfo_ReadValid(
            &image_size,
            &expected_crc32,
            0) == 0U)
    {
        return 0U;
    }

    app_msp = *(__IO uint32_t *)BOOT_APP_BASE_ADDRESS;
    app_reset = *(__IO uint32_t *)(BOOT_APP_BASE_ADDRESS + 4UL);

    if ((app_msp == 0xFFFFFFFFUL) ||
        (app_reset == 0xFFFFFFFFUL))
    {
        return 0U;
    }

    /* 当前 App 的栈位于 DTCM，初始 MSP 可以等于 RAM 末端。 */
    if ((app_msp < BOOT_APP_DTCM_START_ADDRESS) ||
        (app_msp > BOOT_APP_DTCM_END_ADDRESS))
    {
        return 0U;
    }

    /* Cortex-M7 栈至少 8 字节对齐。 */
    if ((app_msp & 0x7UL) != 0UL)
    {
        return 0U;
    }

    /* Reset_Handler 必须设置 Thumb 位。 */
    if ((app_reset & 0x1UL) == 0UL)
    {
        return 0U;
    }

    reset_address = app_reset & ~0x1UL;

    /* Reset_Handler 不能进入镜像末端的元数据区域。 */
    if ((reset_address < BOOT_APP_BASE_ADDRESS) ||
        (reset_address >= BOOT_IMAGE_INFO_ADDRESS))
    {
        return 0U;
    }

    calculated_crc32 = Boot_CRC32_Calculate(
        (const uint8_t *)BOOT_APP_BASE_ADDRESS,
        image_size);

    if (calculated_crc32 != expected_crc32)
    {
        return 0U;
    }

    return 1U;
}

void Boot_App_Jump(void)
{
    uint32_t app_msp;
    uint32_t app_reset;

    /* 必须在清理环境之前读取 App 向量表。 */
    app_msp = *(__IO uint32_t *)BOOT_APP_BASE_ADDRESS;
    app_reset = *(__IO uint32_t *)(BOOT_APP_BASE_ADDRESS + 4UL);

    Boot_App_PrepareForJump();

    SCB->VTOR = BOOT_APP_BASE_ADDRESS;
    __DSB();
    __ISB();

    Boot_App_Start(app_msp, app_reset);

    /* 正常情况下不会执行到这里。 */
    while (1)
    {
    }
}
