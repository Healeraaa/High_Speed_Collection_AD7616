#include "boot_protocol.h"
#include "boot_crc32.h"
#include "bsp_boot_usart.h"

/*
 * 协议层工作边界
 * ============
 * 输入：USART1 连续收到的原始字节流。
 * 输出：已经通过版本、长度和 CRC 检查的 BootFrame_t。
 *
 * 本文件不知道 DATA 最终写到哪个 Flash 地址，也不改变升级状态；
 * 它只保证交给 boot_service.c 的是一帧结构完整的数据。
 */

/* 版本(1)+命令(1)+序号(2)+Payload长度(2)，不包含0x55AA。 */
#define BOOT_PROTOCOL_HEADER_SIZE     6U

/* 帧末尾 CRC32 固定占4字节。 */
#define BOOT_PROTOCOL_CRC_SIZE        4U

/** @brief 从小端字节数组读取16位无符号整数 */
static uint16_t Boot_ReadU16LE(
    const uint8_t *data
)
{
    return (uint16_t)data[0] |
           ((uint16_t)data[1] << 8U);
}

/** @brief 从小端字节数组读取32位无符号整数 */
static uint32_t Boot_ReadU32LE(
    const uint8_t *data
)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

/** @brief 把16位整数按小端顺序写入字节数组 */
static void Boot_WriteU16LE(
    uint8_t *data,
    uint16_t value
)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

/** @brief 把32位整数按小端顺序写入字节数组 */
static void Boot_WriteU32LE(
    uint8_t *data,
    uint32_t value
)
{
    data[0] = (uint8_t)(value & 0xFFUL);
    data[1] = (uint8_t)((value >> 8U) & 0xFFUL);
    data[2] = (uint8_t)((value >> 16U) & 0xFFUL);
    data[3] = (uint8_t)((value >> 24U) & 0xFFUL);
}

/**
 * @brief 连续接收指定数量的字节
 *
 * 每个字节都单独使用 byte_timeout_ms。只要其中任意一个字节超时，
 * 整段接收立即失败，由上层重新寻找下一帧的0x55 0xAA。
 */
static uint8_t Boot_ReceiveExact(
    uint8_t *data,
    uint32_t length,
    uint32_t byte_timeout_ms
)
{
    uint32_t index;

    for (index = 0U; index < length; index++)
    {
        /* 轮询底层USART，结果直接写入目标缓冲区。 */
        if (!BSP_BOOT_USART1_ReceiveByte(
                &data[index],
                byte_timeout_ms))
        {
            return 0U;
        }
    }

    return 1U;
}


/**
 * @brief 寻找 0x55 0xAA 帧头
 *
 * 串口是连续字节流，连接时可能正好处于旧数据中间，因此不能假设
 * 第一个收到的字节就是帧头。本函数会丢弃无关字节直到找到55 AA。
 */
static uint8_t Boot_FindFrameMagic(
    uint32_t byte_timeout_ms
)
{
    uint8_t data;
    uint8_t found_first = 0U;

    while (1)
    {
        if (!BSP_BOOT_USART1_ReceiveByte(
                &data,
                byte_timeout_ms))
        {
            return 0U;
        }

        if (!found_first)
        {
            /* 尚未找到0x55：当前只关心第一个Magic字节。 */
            if (data == BOOT_FRAME_MAGIC_0)
            {
                found_first = 1U;
            }
        }
        else
        {
            /* 已有0x55，紧随其后的0xAA组成完整帧头。 */
            if (data == BOOT_FRAME_MAGIC_1)
            {
                return 1U;
            }

            /*
             * 连续收到 0x55 时，继续保持已经找到
             * 第一个帧头字节的状态。
             */
            if (data != BOOT_FRAME_MAGIC_0)
            {
                found_first = 0U;
            }
        }
    }
}


BootFrameStatus_t Boot_Protocol_ReceiveFrame(
    BootFrame_t *frame,
    uint32_t byte_timeout_ms
)
{
    /* header从version开始，正好也是协议CRC计算的起点。 */
    uint8_t header[BOOT_PROTOCOL_HEADER_SIZE];
    uint8_t crc_bytes[BOOT_PROTOCOL_CRC_SIZE];

    uint32_t calculated_crc;
    uint32_t received_crc;

    if (frame == 0)
    {
        return BOOT_FRAME_LENGTH_ERROR;
    }

    /*
     * 先清零可见字段。即使后续版本/长度/CRC错误，service层发送NACK时
     * 也不会误用上一次成功帧留下的命令和序号。
     */
    frame->command = 0U;
    frame->sequence = 0U;
    frame->payload_length = 0U;

    /* 第一步：在连续字节流中同步到0x55 0xAA。 */
    if (!Boot_FindFrameMagic(byte_timeout_ms))
    {
        return BOOT_FRAME_TIMEOUT;
    }

    /* 第二步：读取固定6字节Header。 */
    if (!Boot_ReceiveExact(
            header,
            sizeof(header),
            byte_timeout_ms))
    {
        return BOOT_FRAME_TIMEOUT;
    }

    /* Header布局：[version][command][seq_L][seq_H][len_L][len_H]。 */
    frame->command = header[1];

    frame->sequence =
        Boot_ReadU16LE(&header[2]);

    frame->payload_length =
        Boot_ReadU16LE(&header[4]);

    /* 不接受其它协议版本，避免按错误格式解释Payload。 */
    if (header[0] != BOOT_PROTOCOL_VERSION)
    {
        return BOOT_FRAME_VERSION_ERROR;
    }

    /* 在接收Payload前先做上限检查，防止写越界payload[]。 */
    if (frame->payload_length >
        BOOT_PROTOCOL_MAX_PAYLOAD)
    {
        return BOOT_FRAME_LENGTH_ERROR;
    }

    /* 第三步：根据Header声明的长度接收可变Payload。 */
    if (frame->payload_length > 0U)
    {
        if (!Boot_ReceiveExact(
                frame->payload,
                frame->payload_length,
                byte_timeout_ms))
        {
            return BOOT_FRAME_TIMEOUT;
        }
    }

    /* 第四步：读取帧末尾4字节CRC。 */
    if (!Boot_ReceiveExact(
            crc_bytes,
            sizeof(crc_bytes),
            byte_timeout_ms))
    {
        return BOOT_FRAME_TIMEOUT;
    }

    received_crc =
        Boot_ReadU32LE(crc_bytes);

    /*
     * CRC范围不包括0x55 0xAA，也不包括CRC字段自身；先算Header，
     * 再把Payload继续追加到同一个CRC中间值。
     */
    calculated_crc = 0xFFFFFFFFUL;

    calculated_crc = Boot_CRC32_Update(
        calculated_crc,
        header,
        sizeof(header)
    );

    if (frame->payload_length > 0U)
    {
        calculated_crc = Boot_CRC32_Update(
            calculated_crc,
            frame->payload,
            frame->payload_length
        );
    }

    calculated_crc ^= 0xFFFFFFFFUL;

    /* CRC不一致说明帧在传输中损坏，命令层不能执行。 */
    if (calculated_crc != received_crc)
    {
        return BOOT_FRAME_CRC_ERROR;
    }

    return BOOT_FRAME_OK;
}


void Boot_Protocol_SendFrame(
    uint8_t command,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length
)
{
    uint8_t magic[2];
    uint8_t header[BOOT_PROTOCOL_HEADER_SIZE];
    uint8_t crc_bytes[BOOT_PROTOCOL_CRC_SIZE];

    uint32_t crc;

    /* 先保护内部数组和协议上限。 */
    if (payload_length >
        BOOT_PROTOCOL_MAX_PAYLOAD)
    {
        return;
    }

    if ((payload_length > 0U) &&
        (payload == 0))
    {
        return;
    }

    /* 组装固定帧头。 */
    magic[0] = BOOT_FRAME_MAGIC_0;
    magic[1] = BOOT_FRAME_MAGIC_1;

    header[0] = BOOT_PROTOCOL_VERSION;
    header[1] = command;

    Boot_WriteU16LE(
        &header[2],
        sequence
    );

    Boot_WriteU16LE(
        &header[4],
        payload_length
    );

    /* CRC仍然只覆盖Header和Payload，与接收端完全对称。 */
    crc = 0xFFFFFFFFUL;

    crc = Boot_CRC32_Update(
        crc,
        header,
        sizeof(header)
    );

    if (payload_length > 0U)
    {
        crc = Boot_CRC32_Update(
            crc,
            payload,
            payload_length
        );
    }

    crc ^= 0xFFFFFFFFUL;

    Boot_WriteU32LE(
        crc_bytes,
        crc
    );

    /*
     * 分段发送可以避免再申请一个超过1KB的大临时数组。
     * 串口线上仍然是连续的一帧：Magic -> Header -> Payload -> CRC。
     */
    BSP_BOOT_USART1_SendBuffer(
        magic,
        sizeof(magic)
    );

    BSP_BOOT_USART1_SendBuffer(
        header,
        sizeof(header)
    );

    if (payload_length > 0U)
    {
        BSP_BOOT_USART1_SendBuffer(
            payload,
            payload_length
        );
    }

    BSP_BOOT_USART1_SendBuffer(
        crc_bytes,
        sizeof(crc_bytes)
    );
}



void Boot_Protocol_SendHelloResponse(
    uint16_t sequence
)
{
    /* 1+4+4+2=11字节，字段顺序必须与Qt上位机解析顺序一致。 */
    uint8_t payload[11];

    /* Byte 0：当前协议版本。 */
    payload[0] = BOOT_PROTOCOL_VERSION;

    /* Byte 1~4：App向量表/固件写入起始地址。 */
    Boot_WriteU32LE(
        &payload[1],
        BOOT_APP_BASE_ADDRESS
    );

    /* Byte 5~8：最大BIN长度，不包含最后64字节元数据区。 */
    Boot_WriteU32LE(
        &payload[5],
        BOOT_APP_MAX_SIZE
    );

    /* Byte 9~10：设备能接收的最大单帧Payload。 */
    Boot_WriteU16LE(
        &payload[9],
        BOOT_PROTOCOL_MAX_PAYLOAD
    );

    Boot_Protocol_SendFrame(
        BOOT_RSP_HELLO,
        sequence,
        payload,
        sizeof(payload)
    );
}


void Boot_Protocol_SendNack(
    uint16_t sequence,
    BootError_t error
)
{
    /* NACK只需告诉上位机一个明确错误码。 */
    uint8_t payload[1];

    payload[0] = (uint8_t)error;

    Boot_Protocol_SendFrame(
        BOOT_RSP_NACK,
        sequence,
        payload,
        sizeof(payload)
    );
}

void Boot_Protocol_SendAck(
    uint16_t sequence,
    uint8_t acknowledged_command,
    uint32_t next_offset
)
{
    /*
     * ACK固定5字节：被确认命令(1)+next_offset(4)。
     * DATA使用next_offset实现顺序控制和丢ACK后的重发恢复。
     */
    uint8_t payload[5];

    payload[0] = acknowledged_command;

    Boot_WriteU32LE(
        &payload[1],
        next_offset
    );

    Boot_Protocol_SendFrame(
        BOOT_RSP_ACK,
        sequence,
        payload,
        sizeof(payload)
    );
}
