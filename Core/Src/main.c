#include "main.h"
#include "bsp.h"
#include "bsp_boot_usart.h"
#include "boot_protocol.h"
#include "boot_upgrade.h"
#include "boot_image_info.h"
#include "boot_crc32.h"

#define APP_BASE_ADDRESS 0x08100000UL
#define APP_END_ADDRESS 0x08200000UL

#define DTCM_START_ADDRESS 0x20000000UL
#define DTCM_END_ADDRESS 0x20020000UL

typedef void (*AppEntry_t)(void);

/**
 * @brief 检查App元数据、向量表和完整CRC32
 */
static uint8_t Boot_IsAppValid(void)
{
    uint32_t app_msp;
    uint32_t app_reset;
    uint32_t reset_address;

    uint32_t image_size;
    uint32_t expected_crc32;
    uint32_t calculated_crc32;

    /*
     * 第一步：检查固件信息和最终提交标记。
     *
     * 升级未完成、元数据损坏或提交标记不存在时，
     * 都不允许启动App。
     */
    if (Boot_ImageInfo_ReadValid(
            &image_size,
            &expected_crc32,
            0) == 0U)
    {
        return 0U;
    }

    app_msp =
        *(__IO uint32_t *)APP_BASE_ADDRESS;

    app_reset =
        *(__IO uint32_t *)
            (APP_BASE_ADDRESS + 4UL);

    /*
     * 擦除后的Flash。
     */
    if ((app_msp == 0xFFFFFFFFUL) ||
        (app_reset == 0xFFFFFFFFUL))
    {
        return 0U;
    }

    /*
     * 当前App栈位于DTCM：
     * 0x20000000～0x20020000。
     *
     * 初始MSP允许等于RAM末端地址。
     */
    if ((app_msp < DTCM_START_ADDRESS) ||
        (app_msp > DTCM_END_ADDRESS))
    {
        return 0U;
    }

    /*
     * Cortex-M7栈至少8字节对齐。
     */
    if ((app_msp & 0x7UL) != 0UL)
    {
        return 0U;
    }

    /*
     * Reset_Handler必须设置Thumb位。
     */
    if ((app_reset & 0x1UL) == 0UL)
    {
        return 0U;
    }

    reset_address =
        app_reset & ~0x1UL;

    /*
     * Reset_Handler必须位于App有效区域，
     * 不能进入最后64字节元数据区。
     */
    if ((reset_address < APP_BASE_ADDRESS) ||
        (reset_address >=
         BOOT_IMAGE_INFO_ADDRESS))
    {
        return 0U;
    }

    /*
     * 最后检查整个App的CRC32。
     *
     * 只有Flash中的实际内容与END阶段保存的
     * 长度和CRC完全一致时，才允许启动。
     */
    calculated_crc32 =
        Boot_CRC32_Calculate(
            (const uint8_t *)
                APP_BASE_ADDRESS,
            image_size
        );

    if (calculated_crc32 !=
        expected_crc32)
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief 跳转前清理 Boot 使用过的系统资源
 */
static void Boot_PrepareForJump(void)
{
    uint32_t index;

    /*
     * HAL_RCC_DeInit()内部需要使用HAL Tick判断超时，
     * 因此必须在关闭中断、停止Tick之前调用。
     *
     * 恢复为：
     * HSI作为系统时钟；
     * 关闭HSE、PLL1/2/3；
     * AHB/APB恢复默认分频。
     */
    (void)HAL_RCC_DeInit();

    /*
     * 复位AHB/APB总线上的外设。
     * 包括USART、TIM、GPIO、DMA等外设寄存器。
     */
    (void)HAL_DeInit();

    /*
     * 从这里开始不再调用任何依赖HAL Tick或外设的函数。
     */
    __disable_irq();

    /*
     * HAL_DeInit()会复位外设，但不会清除所有外设时钟使能位。
     * 关闭当前Boot明确使用过的外设时钟。
     */
    __HAL_RCC_USART1_CLK_DISABLE();
    __HAL_RCC_TIM6_CLK_DISABLE();

    __HAL_RCC_GPIOA_CLK_DISABLE();
    __HAL_RCC_GPIOC_CLK_DISABLE();
    __HAL_RCC_GPIOH_CLK_DISABLE();

    /*
     * 停止Cortex-M7 SysTick。
     * 当前HAL Tick使用TIM6，但仍然显式清理SysTick。
     */
    SysTick->CTRL = 0UL;
    SysTick->LOAD = 0UL;
    SysTick->VAL  = 0UL;

    /*
     * 停止Boot使用的DWT周期计数器。
     * App后续会通过BSP_DWT_DelayInit()重新初始化。
     */
    DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0UL;

    /*
     * 如果以后Boot启用了D-Cache/I-Cache，
     * 跳转前清理并关闭，交给App重新配置。
     *
     * 当前Boot没有启用Cache，因此目前通常不会进入。
     */
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0UL)
    {
        SCB_DisableDCache();
    }

    if ((SCB->CCR & SCB_CCR_IC_Msk) != 0UL)
    {
        SCB_DisableICache();
    }

    /*
     * 恢复MPU到禁用状态。
     * App的BSP_Init()会重新执行MPU_Config()。
     */
    HAL_MPU_Disable();

    /*
     * 禁用并清除所有NVIC外部中断。
     * STM32H743使用不到全部8组，但全部清理更稳妥。
     */
    for (index = 0U; index < 8U; index++)
    {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }

    /*
     * 清除系统异常挂起状态。
     */
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk |
                SCB_ICSR_PENDSTCLR_Msk;

    /*
     * 恢复NVIC优先级分组到复位值。
     * App稍后会重新设置为NVIC_PRIORITYGROUP_4。
     */
    NVIC_SetPriorityGrouping(0U);

    __DSB();
    __ISB();
}


/**
 * @brief 最终设置CPU运行状态并进入App
 * @note  ARMCC 5汇编函数，切换MSP后不再执行普通C代码
 *
 * R0 = App初始MSP
 * R1 = App Reset_Handler地址
 */
__asm void Boot_StartApp(
    uint32_t app_msp,
    uint32_t app_reset
)
{
    MOVS    R2, #0

    /* 使用特权线程模式和MSP */
    MSR     CONTROL, R2

    /* 清除中断屏蔽状态 */
    MSR     BASEPRI, R2
    MSR     FAULTMASK, R2

    /* PSP恢复为复位值 */
    MSR     PSP, R2

    DSB
    ISB

    /* 从这里开始不能再访问Boot的栈和局部变量 */
    MSR     MSP, R0

    /* 恢复全局中断，匹配CPU复位后的状态 */
    CPSIE   I

    DSB
    ISB

    /* 跳入App的Reset_Handler，不再返回 */
    BX      R1
}


/**
 * @brief 清理Boot环境并跳转到App
 */
static void Boot_JumpToApp(void)
{
    uint32_t app_msp;
    uint32_t app_reset;

    /*
     * 必须在清理环境之前读取App向量表。
     */
    app_msp =
        *(__IO uint32_t *)APP_BASE_ADDRESS;

    app_reset =
        *(__IO uint32_t *)(APP_BASE_ADDRESS + 4UL);

    Boot_PrepareForJump();

    /*
     * App中断向量表起始地址。
     */
    SCB->VTOR = APP_BASE_ADDRESS;

    __DSB();
    __ISB();

    /*
     * 进入汇编函数后不会返回。
     */
    Boot_StartApp(
        app_msp,
        app_reset
    );

    /*
     * 理论上永远不会运行到这里。
     */
    while (1)
    {
    }
}

static void Boot_UpgradeMode(void)
{
    /*
     * 约 1 KB，必须声明为 static，
     * 避免占用函数栈。
     */
    static BootFrame_t received_frame;

    BootFrameStatus_t status;
    BootError_t command_error;

    uint32_t next_offset;
    uint32_t image_crc32;
    uint32_t completed_image_size;

    BSP_BOOT_USART1_Init();
    Boot_Upgrade_Init();

    while (1)
    {
        status = Boot_Protocol_ReceiveFrame(
            &received_frame,
            1000U);

        if (status == BOOT_FRAME_TIMEOUT)
        {
            continue;
        }

        if (status == BOOT_FRAME_VERSION_ERROR)
        {
            Boot_Protocol_SendNack(
                received_frame.sequence,
                BOOT_ERROR_FRAME_VERSION);

            continue;
        }

        if (status == BOOT_FRAME_LENGTH_ERROR)
        {
            Boot_Protocol_SendNack(
                received_frame.sequence,
                BOOT_ERROR_FRAME_LENGTH);

            continue;
        }

        if (status == BOOT_FRAME_CRC_ERROR)
        {
            Boot_Protocol_SendNack(
                received_frame.sequence,
                BOOT_ERROR_FRAME_CRC);

            continue;
        }

        if ((received_frame.command ==
             BOOT_CMD_HELLO) &&
            (received_frame.payload_length == 0U))
        {
            Boot_Protocol_SendHelloResponse(
                received_frame.sequence);
        }
        else if (received_frame.command ==
                 BOOT_CMD_START)
        {
            command_error =
                Boot_Upgrade_HandleStart(
                    &received_frame);

            if (command_error == BOOT_ERROR_NONE)
            {
                Boot_Protocol_SendAck(
                    received_frame.sequence,
                    BOOT_CMD_START,
                    0U);
            }
            else
            {
                Boot_Protocol_SendNack(
                    received_frame.sequence,
                    command_error);
            }
        }
        else if (received_frame.command ==
                 BOOT_CMD_DATA)
        {
            command_error =
                Boot_Upgrade_HandleData(
                    &received_frame,
                    &next_offset);

            if (command_error == BOOT_ERROR_NONE)
            {
                Boot_Protocol_SendAck(
                    received_frame.sequence,
                    BOOT_CMD_DATA,
                    next_offset);
            }
            else
            {
                Boot_Protocol_SendNack(
                    received_frame.sequence,
                    command_error);
            }
        }
        else if (received_frame.command ==
                 BOOT_CMD_END)
        {
            command_error =
                Boot_Upgrade_HandleEnd(
                    &received_frame,
                    &image_crc32,
                    &completed_image_size);

            if (command_error == BOOT_ERROR_NONE)
            {
                Boot_Protocol_SendAck(
                    received_frame.sequence,
                    BOOT_CMD_END,
                    completed_image_size);
            }
            else
            {
                Boot_Protocol_SendNack(
                    received_frame.sequence,
                    command_error);
            }
        }
        else
        {
            Boot_Protocol_SendNack(
                received_frame.sequence,
                BOOT_ERROR_COMMAND);
        }
    }
}

// int main(void)
// {
//   BSP_Init();
//   // MX_USB_DEVICE_Init();
//   // Module_Config();
//   // App_Init();
//   while (1)
//   {
//   }

// }

int main(void)
{
    BSP_Init();
    BSP_GPIO_BOOT_KEY_Init();

    if (BSP_GPIO_BOOT_KEY_IsPressed())
    {
        Boot_UpgradeMode();
    }

    /*
     * 没按 K1：
     * App 合法则跳转。
     */
    if (Boot_IsAppValid())
    {
        Boot_JumpToApp();
    }

    /*
     * App 不存在或向量表非法：
     * 暂时慢速闪烁并停留在 Boot。
     */
    while (1)
    {
        LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_13);
        BSP_DWT_Delay_ms(1000U);
    }
}