#include "boot_app.h"

#include "main.h"
#include "boot_crc32.h"
#include "boot_image_info.h"
#include "boot_protocol.h"

/*
 * App 启动管理
 * ============
 *
 * Cortex-M7上电后通常从0x08000000的Boot向量表启动。Boot确认App有效后，
 * 需要模拟一次“从App复位启动”的关键CPU状态：
 *
 *   App向量表[0] -> 初始MSP（主栈顶）
 *   App向量表[1] -> Reset_Handler入口地址
 *   SCB->VTOR     -> App向量表地址0x08100000
 *
 * 直接调用0x08100000是不正确的，因为该地址存放的是MSP数值，不是代码。
 */

/* 当前App的链接脚本把主栈放在128KB DTCM中。 */
#define BOOT_APP_DTCM_START_ADDRESS  0x20000000UL
#define BOOT_APP_DTCM_END_ADDRESS    0x20020000UL

/**
 * @brief 跳转前清理 Boot 使用过的系统资源
 */
static void Boot_App_PrepareForJump(void)
{
    uint32_t index;

    /*
     * 第一步：恢复系统时钟。
     * HAL_RCC_DeInit()内部需要HAL Tick判断等待是否超时，因此必须在
     * __disable_irq()和停止Tick之前调用。执行后系统回到以HSI为主的
     * 接近复位状态，App随后会按自己的SystemClock_Config()重新配置。
     */
    (void)HAL_RCC_DeInit();

    /*
     * 第二步：复位AHB/APB外设寄存器，避免USART、TIM、GPIO等Boot状态
     * 泄漏到App。App会重新初始化自己真正需要的外设。
     */
    (void)HAL_DeInit();

    /* 从这里开始不再调用任何依赖 HAL Tick 或外设的函数。 */
    __disable_irq();

    /*
     * HAL_DeInit()不会保证所有外设时钟使能位都关闭，因此显式关闭
     * Boot路径确定用过的USART1、TIM6和GPIO端口时钟。
     */
    __HAL_RCC_USART1_CLK_DISABLE();
    __HAL_RCC_TIM6_CLK_DISABLE();
    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOC_CLK_DISABLE();
    __HAL_RCC_GPIOH_CLK_DISABLE();

    /*
     * 即使当前HAL Tick来自TIM6，也清空SysTick，防止以前的配置在App
     * 开中断后产生意外SysTick异常。
     */
    SysTick->CTRL = 0UL;
    SysTick->LOAD = 0UL;
    SysTick->VAL = 0UL;

    /* DWT由Boot的毫秒延时使用，App需要时会重新开启并清零。 */
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0UL;

    /*
     * 如果以后Boot启用了D-Cache/I-Cache，跳转前关闭，避免App误认为
     * Cache仍处于复位状态。CMSIS关闭D-Cache时会完成必要的Clean操作。
     */
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0UL)
    {
        SCB_DisableDCache();
    }

    if ((SCB->CCR & SCB_CCR_IC_Msk) != 0UL)
    {
        SCB_DisableICache();
    }

    /* App的BSP_Init()会按自己的内存布局重新配置MPU。 */
    HAL_MPU_Disable();

    /*
     * NVIC外部中断共有多组32位寄存器。ICER负责禁用，ICPR负责清除
     * 已挂起状态；否则App一开总中断就可能先进入Boot遗留的中断。
     */
    for (index = 0U; index < 8U; index++)
    {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }

    /* PendSV/SysTick属于系统异常，不在上面的NVIC外部中断寄存器中。 */
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk |
                SCB_ICSR_PENDSTCLR_Msk;

    /* 恢复分组，App随后会设置自己使用的NVIC_PRIORITYGROUP。 */
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

    /* CONTROL=0：线程模式使用MSP并保持特权级，与复位后状态一致。 */
    MSR     CONTROL, R2

    /* 清除优先级屏蔽和Fault屏蔽。PRIMASK最后通过CPSIE I清除。 */
    MSR     BASEPRI, R2
    MSR     FAULTMASK, R2

    /* App启动阶段只使用MSP，PSP先恢复为0。 */
    MSR     PSP, R2

    DSB
    ISB

    /*
     * MSP一旦换成App栈，当前Boot函数的返回地址和局部变量所在栈就失效，
     * 因此后面只能执行不依赖C栈的汇编指令并直接BX跳转。
     */
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

    /*
     * 第一关：检查尾部两个元数据Flash Word。
     * START会擦除它们；只有END全部成功才会最后重写提交标记，因此升级
     * 中途掉电会在这里直接失败，后续不会尝试执行残缺App。
     */
    if (Boot_ImageInfo_ReadValid(
            &image_size,
            &expected_crc32,
            0) == 0U)
    {
        return 0U;
    }

    /*
     * Cortex-M向量表前8字节：
     * [0x08100000] 初始MSP；[0x08100004] Reset_Handler。
     */
    app_msp = *(__IO uint32_t *)BOOT_APP_BASE_ADDRESS;
    app_reset = *(__IO uint32_t *)(BOOT_APP_BASE_ADDRESS + 4UL);

    /* Flash擦除态全为1，出现0xFFFFFFFF说明向量表没有被成功写入。 */
    if ((app_msp == 0xFFFFFFFFUL) ||
        (app_reset == 0xFFFFFFFFUL))
    {
        return 0U;
    }

    /*
     * 第二关：MSP必须落在App链接脚本使用的DTCM范围内。
     * 栈从高地址向低地址增长，因此初始MSP等于RAM末地址也是合法的。
     */
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

    /* Cortex-M只执行Thumb指令，函数地址bit0必须为1。 */
    if ((app_reset & 0x1UL) == 0UL)
    {
        return 0U;
    }

    reset_address = app_reset & ~0x1UL;

    /*
     * 清除Thumb标志后得到真实代码地址。它必须位于App代码空间，且不能
     * 指向0x081FFFC0之后的元数据保留区。
     */
    if ((reset_address < BOOT_APP_BASE_ADDRESS) ||
        (reset_address >= BOOT_IMAGE_INFO_ADDRESS))
    {
        return 0U;
    }

    /*
     * 第三关：对Flash中的真实App重新计算完整CRC。
     * 这一步能发现元数据虽然完整、但App正文被意外改写的情况。
     */
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

    /*
     * 必须先把两个入口值读入CPU寄存器/局部变量；清理时会改变时钟、
     * 中断和Cache状态，不应在清理完成后再执行复杂的Flash访问流程。
     */
    app_msp = *(__IO uint32_t *)BOOT_APP_BASE_ADDRESS;
    app_reset = *(__IO uint32_t *)(BOOT_APP_BASE_ADDRESS + 4UL);

    Boot_App_PrepareForJump();

    /*
     * VTOR决定中断向量表从哪里读取。若不修改，App中断仍会跳到Boot
     * 位于0x08000000的中断处理函数。
     */
    SCB->VTOR = BOOT_APP_BASE_ADDRESS;
    __DSB();
    __ISB();

    Boot_App_Start(app_msp, app_reset);

    /* 正常情况下不会执行到这里。 */
    while (1)
    {
    }
}
