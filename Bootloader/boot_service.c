#include "boot_service.h"

#include "bsp_boot_usart.h"
#include "boot_protocol.h"
#include "boot_upgrade.h"

/*
 * 升级服务层
 * ==========
 *
 * boot_protocol.c 负责确认“这一帧有没有收对”；
 * boot_upgrade.c 负责“这一条升级命令怎样操作Flash”；
 * 本文件位于两者中间，负责根据command选择处理函数，并把结果翻译成
 * ACK或NACK发回上位机。
 *
 * 这种分层让串口帧格式和Flash升级流程互不混杂，main.c也只需调用
 * Boot_Service_Run()即可进入升级模式。
 */

/**
 * @brief 处理一帧已经通过协议层校验的命令
 * @param frame 已经通过版本、长度上限和帧CRC检查的完整协议帧
 */
static void Boot_Service_DispatchFrame(
    const BootFrame_t *frame)
{
    BootError_t command_error;
    uint32_t next_offset;
    uint32_t image_crc32;
    uint32_t completed_image_size;

    /*
     * HELLO只用于查询设备能力，不改变升级状态。
     * 当前协议规定HELLO必须是空Payload。
     */
    if ((frame->command == BOOT_CMD_HELLO) &&
        (frame->payload_length == 0U))
    {
        Boot_Protocol_SendHelloResponse(frame->sequence);
        return;
    }

    /* START会校验镜像参数并擦除整个Bank2，成功后下一偏移固定为0。 */
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

    /*
     * DATA写入一个分包。next_offset由升级层返回，告诉上位机设备已经
     * 连续接收到哪里，也用于识别丢失ACK后的重复包。
     */
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

    /*
     * END执行完整CRC、写元数据、补写向量表并提交镜像。
     * ACK中的32位字段复用为最终确认的镜像长度。
     */
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

    /* 未实现命令、带Payload的HELLO以及预留ABORT都走命令错误。 */
    Boot_Protocol_SendNack(
        frame->sequence,
        BOOT_ERROR_COMMAND);
}

void Boot_Service_Run(void)
{
    /*
     * BootFrame_t 内含1024字节Payload。如果定义为普通局部变量，会长期
     * 占用Boot函数栈；定义为static后放在ZI/BSS区，栈只保存少量状态。
     */
    static BootFrame_t received_frame;
    BootFrameStatus_t status;

    /* 初始化115200-8-N-1轮询串口，并清空上一升级会话的RAM状态。 */
    BSP_BOOT_USART1_Init();
    Boot_Upgrade_Init();

    while (1)
    {
        /* 每个字节最多等待1秒；没有数据时超时并重新开始下一轮。 */
        status = Boot_Protocol_ReceiveFrame(
            &received_frame,
            1000U);

        /*
         * 超时不是协议错误：可能只是上位机尚未发送数据，因此不回复NACK。
         */
        if (status == BOOT_FRAME_TIMEOUT)
        {
            continue;
        }

        /* 已读到序号时，把具体帧错误用相同序号回复给上位机。 */
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

        /* 只有完整且CRC正确的帧才能真正执行升级命令。 */
        Boot_Service_DispatchFrame(&received_frame);
    }
}
