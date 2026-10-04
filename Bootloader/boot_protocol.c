#include "boot_protocol.h"
#include "boot_crc32.h"
#include "bsp_boot_usart.h"


#define BOOT_PROTOCOL_HEADER_SIZE     6U
#define BOOT_PROTOCOL_CRC_SIZE        4U


static uint16_t Boot_ReadU16LE(
    const uint8_t *data
)
{
    return (uint16_t)data[0] |
           ((uint16_t)data[1] << 8U);
}


static uint32_t Boot_ReadU32LE(
    const uint8_t *data
)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}


static void Boot_WriteU16LE(
    uint8_t *data,
    uint16_t value
)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)((value >> 8U) & 0xFFU);
}


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


static uint8_t Boot_ReceiveExact(
    uint8_t *data,
    uint32_t length,
    uint32_t byte_timeout_ms
)
{
    uint32_t index;

    for (index = 0U; index < length; index++)
    {
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
            if (data == BOOT_FRAME_MAGIC_0)
            {
                found_first = 1U;
            }
        }
        else
        {
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
    uint8_t header[BOOT_PROTOCOL_HEADER_SIZE];
    uint8_t crc_bytes[BOOT_PROTOCOL_CRC_SIZE];

    uint32_t calculated_crc;
    uint32_t received_crc;

    if (frame == 0)
    {
        return BOOT_FRAME_LENGTH_ERROR;
    }

    frame->command = 0U;
    frame->sequence = 0U;
    frame->payload_length = 0U;

    if (!Boot_FindFrameMagic(byte_timeout_ms))
    {
        return BOOT_FRAME_TIMEOUT;
    }

    if (!Boot_ReceiveExact(
            header,
            sizeof(header),
            byte_timeout_ms))
    {
        return BOOT_FRAME_TIMEOUT;
    }

    frame->command = header[1];

    frame->sequence =
        Boot_ReadU16LE(&header[2]);

    frame->payload_length =
        Boot_ReadU16LE(&header[4]);

    if (header[0] != BOOT_PROTOCOL_VERSION)
    {
        return BOOT_FRAME_VERSION_ERROR;
    }

    if (frame->payload_length >
        BOOT_PROTOCOL_MAX_PAYLOAD)
    {
        return BOOT_FRAME_LENGTH_ERROR;
    }

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

    if (!Boot_ReceiveExact(
            crc_bytes,
            sizeof(crc_bytes),
            byte_timeout_ms))
    {
        return BOOT_FRAME_TIMEOUT;
    }

    received_crc =
        Boot_ReadU32LE(crc_bytes);

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
    uint8_t payload[11];

    payload[0] = BOOT_PROTOCOL_VERSION;

    Boot_WriteU32LE(
        &payload[1],
        BOOT_APP_BASE_ADDRESS
    );

    Boot_WriteU32LE(
        &payload[5],
        BOOT_APP_MAX_SIZE
    );

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
