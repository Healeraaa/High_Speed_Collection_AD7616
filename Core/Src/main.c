#include "main.h"
#include "bsp.h"
#include "bsp_boot_usart.h"
#include "boot_protocol.h"
#include "boot_upgrade.h"

#define APP_BASE_ADDRESS 0x08100000UL
#define APP_END_ADDRESS 0x08200000UL

#define DTCM_START_ADDRESS 0x20000000UL
#define DTCM_END_ADDRESS 0x20020000UL

typedef void (*AppEntry_t)(void);

/**
 * @brief 检查 App 向量表是否合法
 */
static uint8_t Boot_IsAppValid(void)
{
    uint32_t app_msp;
    uint32_t app_reset;
    uint32_t reset_address;

    app_msp = *(__IO uint32_t *)APP_BASE_ADDRESS;
    app_reset = *(__IO uint32_t *)(APP_BASE_ADDRESS + 4UL);

    /* 擦除后的 Flash */
    if ((app_msp == 0xFFFFFFFFUL) ||
        (app_reset == 0xFFFFFFFFUL))
    {
        return 0U;
    }

    /*
     * 当前 App 的栈位于 DTCM：
     * 0x20000000～0x20020000
     *
     * 初始 MSP 允许等于 RAM 末端地址。
     */
    if ((app_msp < DTCM_START_ADDRESS) ||
        (app_msp > DTCM_END_ADDRESS))
    {
        return 0U;
    }

    /* Cortex-M7 栈要求至少按 8 字节对齐 */
    if ((app_msp & 0x7UL) != 0UL)
    {
        return 0U;
    }

    /* Reset_Handler 必须是 Thumb 地址 */
    if ((app_reset & 0x1UL) == 0UL)
    {
        return 0U;
    }

    reset_address = app_reset & ~0x1UL;

    /* Reset_Handler 必须位于 App Flash 范围内 */
    if ((reset_address < APP_BASE_ADDRESS) ||
        (reset_address >= APP_END_ADDRESS))
    {
        return 0U;
    }

    return 1U;
}

/**
 * @brief 跳转到 App
 */
static void Boot_JumpToApp(void)
{
    uint32_t app_msp;
    uint32_t app_reset;
    AppEntry_t app_entry;
    uint32_t index;

    app_msp = *(__IO uint32_t *)APP_BASE_ADDRESS;
    app_reset = *(__IO uint32_t *)(APP_BASE_ADDRESS + 4UL);
    app_entry = (AppEntry_t)app_reset;

    __disable_irq();

    /* 停止 Cortex-M SysTick */
    SysTick->CTRL = 0UL;
    SysTick->LOAD = 0UL;
    SysTick->VAL = 0UL;

    /* 关闭并清除所有外部中断 */
    for (index = 0U; index < 8U; index++)
    {
        NVIC->ICER[index] = 0xFFFFFFFFUL;
        NVIC->ICPR[index] = 0xFFFFFFFFUL;
    }

    /* 清除 PendSV 和 SysTick 挂起状态 */
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk |
                SCB_ICSR_PENDSTCLR_Msk;

    /* 立即切换到 App 向量表 */
    SCB->VTOR = APP_BASE_ADDRESS;

    __DSB();
    __ISB();

    /* 使用特权线程模式和 MSP */
    __set_CONTROL(0UL);
    __set_MSP(app_msp);

    __DSB();
    __ISB();

    /*
     * 前面已经关闭、清除所有 NVIC 中断，
     * 此处恢复全局中断，避免 App 带着 PRIMASK=1 启动。
     */
    __enable_irq();

    app_entry();

    /* 正常情况下不会返回 */
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