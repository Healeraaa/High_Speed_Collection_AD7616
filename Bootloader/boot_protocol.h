#ifndef __BOOT_PROTOCOL_H__
#define __BOOT_PROTOCOL_H__

#include <stdint.h>

/*
 * 帧格式：
 *
 * Byte 0       0x55
 * Byte 1       0xAA
 * Byte 2       协议版本
 * Byte 3       命令
 * Byte 4~5     序号，uint16_t，小端
 * Byte 6~7     Payload 长度，uint16_t，小端
 * Byte 8...    Payload
 * 最后 4 字节  CRC32，小端
 *
 * CRC32 计算范围：
 * 从“协议版本”开始，到 Payload 最后一个字节结束。
 * 不包含 0x55、0xAA，也不包含 CRC32 本身。
 */

#define BOOT_FRAME_MAGIC_0           0x55U
#define BOOT_FRAME_MAGIC_1           0xAAU

#define BOOT_PROTOCOL_VERSION        0x01U

#define BOOT_PROTOCOL_MAX_PAYLOAD    1024U

#define BOOT_APP_BASE_ADDRESS        0x08100000UL
#define BOOT_APP_END_ADDRESS         0x08200000UL
#define BOOT_APP_MAX_SIZE            0x00100000UL


typedef enum
{
    BOOT_CMD_HELLO       = 0x01U,
    BOOT_CMD_START       = 0x02U,
    BOOT_CMD_DATA        = 0x03U,
    BOOT_CMD_END         = 0x04U,
    BOOT_CMD_ABORT       = 0x05U,

    BOOT_RSP_HELLO       = 0x81U,
    BOOT_RSP_ACK         = 0x82U,
    BOOT_RSP_NACK        = 0x83U

} BootCommand_t;


typedef enum
{
    BOOT_ERROR_NONE              = 0x00U,
    BOOT_ERROR_FRAME_TIMEOUT     = 0x01U,
    BOOT_ERROR_FRAME_HEADER      = 0x02U,
    BOOT_ERROR_FRAME_VERSION     = 0x03U,
    BOOT_ERROR_FRAME_LENGTH      = 0x04U,
    BOOT_ERROR_FRAME_CRC         = 0x05U,
    BOOT_ERROR_COMMAND           = 0x06U,
    BOOT_ERROR_SEQUENCE          = 0x07U,
    BOOT_ERROR_IMAGE_SIZE        = 0x08U,
    BOOT_ERROR_IMAGE_CRC         = 0x09U,
    BOOT_ERROR_FLASH_ERASE       = 0x0AU,
    BOOT_ERROR_FLASH_PROGRAM     = 0x0BU

} BootError_t;


typedef struct
{
    uint8_t command;

    uint16_t sequence;

    uint16_t payload_length;

    uint8_t payload[BOOT_PROTOCOL_MAX_PAYLOAD];

} BootFrame_t;

typedef enum
{
    BOOT_FRAME_OK = 0,
    BOOT_FRAME_TIMEOUT,
    BOOT_FRAME_VERSION_ERROR,
    BOOT_FRAME_LENGTH_ERROR,
    BOOT_FRAME_CRC_ERROR

} BootFrameStatus_t;


BootFrameStatus_t Boot_Protocol_ReceiveFrame(
    BootFrame_t *frame,
    uint32_t byte_timeout_ms
);

void Boot_Protocol_SendFrame(
    uint8_t command,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length
);

void Boot_Protocol_SendHelloResponse(
    uint16_t sequence
);

void Boot_Protocol_SendNack(
    uint16_t sequence,
    BootError_t error
);

void Boot_Protocol_SendAck(
    uint16_t sequence,
    uint8_t acknowledged_command,
    uint32_t next_offset
);

#endif