#include "boot_service.h"

#include "bsp_boot_usart.h"
#include "boot_protocol.h"
#include "boot_upgrade.h"

/**
 * @brief 处理一帧已经通过协议层校验的命令
 */
static void Boot_Service_DispatchFrame(
    const BootFrame_t *frame)
{
    BootError_t command_error;
    uint32_t next_offset;
    uint32_t image_crc32;
    uint32_t completed_image_size;

    if ((frame->command == BOOT_CMD_HELLO) &&
        (frame->payload_length == 0U))
    {
        Boot_Protocol_SendHelloResponse(frame->sequence);
        return;
    }

    if (frame->command == BOOT_CMD_START)
    {
        command_error = Boot_Upgrade_HandleStart(frame);

        if (command_error == BOOT_ERROR_NONE)
        {
            Boot_Protocol_SendAck(
                frame->sequence,
                BOOT_CMD_START,
                0U);
        }
        else
        {
            Boot_Protocol_SendNack(
                frame->sequence,
                command_error);
        }
        return;
    }

    if (frame->command == BOOT_CMD_DATA)
    {
        command_error = Boot_Upgrade_HandleData(
            frame,
            &next_offset);

        if (command_error == BOOT_ERROR_NONE)
        {
            Boot_Protocol_SendAck(
                frame->sequence,
                BOOT_CMD_DATA,
                next_offset);
        }
        else
        {
            Boot_Protocol_SendNack(
                frame->sequence,
                command_error);
        }
        return;
    }

    if (frame->command == BOOT_CMD_END)
    {
        command_error = Boot_Upgrade_HandleEnd(
            frame,
            &image_crc32,
            &completed_image_size);

        if (command_error == BOOT_ERROR_NONE)
        {
            Boot_Protocol_SendAck(
                frame->sequence,
                BOOT_CMD_END,
                completed_image_size);
        }
        else
        {
            Boot_Protocol_SendNack(
                frame->sequence,
                command_error);
        }
        return;
    }

    Boot_Protocol_SendNack(
        frame->sequence,
        BOOT_ERROR_COMMAND);
}

void Boot_Service_Run(void)
{
    /* 约 1 KB，必须为 static，避免占用函数栈。 */
    static BootFrame_t received_frame;
    BootFrameStatus_t status;

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

        Boot_Service_DispatchFrame(&received_frame);
    }
}
