#ifndef __BOOT_PROTOCOL_H__
#define __BOOT_PROTOCOL_H__

#include <stdint.h>

/*
 * 本文件定义“上位机和 Boot 如何说话”，不负责擦除或写入 Flash。
 * boot_protocol.c 只完成组帧、拆帧和帧 CRC 校验；命令的实际执行由
 * boot_service.c 分发给 boot_upgrade.c。
 *
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
 *
 * 所有多字节整数均采用小端格式：低 8 位先发送。
 * 例如数值 0x12345678 在线路上的顺序是 78 56 34 12。
 */

/* 固定帧头，用于从连续串口字节流中寻找一帧的起点。 */
#define BOOT_FRAME_MAGIC_0           0x55U
#define BOOT_FRAME_MAGIC_1           0xAAU

/* 上位机与 Boot 必须使用相同版本，否则回复版本错误。 */
#define BOOT_PROTOCOL_VERSION        0x01U

/* 单帧 Payload 上限；BootFrame_t 的 RAM 缓冲区也按此大小分配。 */
#define BOOT_PROTOCOL_MAX_PAYLOAD    1024U

/* STM32H743 内部 Flash Bank2 的起止地址，App 独占 Bank2。 */
#define BOOT_APP_BASE_ADDRESS        0x08100000UL
#define BOOT_APP_END_ADDRESS         0x08200000UL

/*
 * Bank2最后两个Flash Word保留给固件有效信息。
 *
 * 0x081FFFC0～0x081FFFDF：固件信息
 * 0x081FFFE0～0x081FFFFF：最终提交标记
 *
 * 两块各为32字节，正好对应STM32H743的一次Flash Word编程单位。
 */
#define BOOT_IMAGE_INFO_ADDRESS      0x081FFFC0UL
#define BOOT_IMAGE_COMMIT_ADDRESS    0x081FFFE0UL

#define BOOT_APP_MAX_SIZE            \
    (BOOT_IMAGE_INFO_ADDRESS -       \
     BOOT_APP_BASE_ADDRESS)

/*
 * 命令方向和 Payload：
 *
 * 上位机 -> Boot
 *   HELLO：空 Payload，查询协议和 App 区参数。
 *   START：image_size(4) + image_crc32(4)，开始新升级并擦除 Bank2。
 *   DATA ：offset(4) + firmware_data(N)，按偏移写入固件。
 *   END  ：空 Payload，要求设备做整体 CRC 并提交镜像。
 *   ABORT：预留命令，当前版本尚未处理。
 *
 * Boot -> 上位机
 *   RSP_HELLO：协议版本(1) + App首地址(4) + 容量(4) + 最大Payload(2)。
 *   ACK      ：被确认的命令(1) + next_offset(4)。
 *   NACK     ：错误码(1)。
 */
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

/*
 * NACK 中携带的错误码。
 * 帧级错误说明串口数据本身有问题；IMAGE/FLASH 错误说明命令格式正确，
 * 但升级状态、镜像信息或 Flash 操作不满足要求。
 */
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

/*
 * 协议层解析完成后的内存表示。
 * magic、version 和 CRC 不需要上层使用，因此不保存在该结构体中。
 */
typedef struct
{
    /* 当前命令，例如 BOOT_CMD_DATA。 */
    uint8_t command;

    /* 由上位机生成；响应必须原样带回，用来匹配请求和响应。 */
    uint16_t sequence;

    /* payload[] 中实际有效的字节数。 */
    uint16_t payload_length;

    /* 最大约1KB，因此接收此结构体时应避免把它放在较小的函数栈中。 */
    uint8_t payload[BOOT_PROTOCOL_MAX_PAYLOAD];

} BootFrame_t;

/* ReceiveFrame() 的解析结果，只表示“整帧能否交给命令层”。 */
typedef enum
{
    BOOT_FRAME_OK = 0,
    BOOT_FRAME_TIMEOUT,
    BOOT_FRAME_VERSION_ERROR,
    BOOT_FRAME_LENGTH_ERROR,
    BOOT_FRAME_CRC_ERROR

} BootFrameStatus_t;

/**
 * @brief 从 USART1 字节流中寻找并接收一帧
 * @param frame 成功或部分失败时保存已解析的命令、序号和 Payload
 * @param byte_timeout_ms 等待每个字节的最大时间，单位：ms
 * @return 帧接收状态；只有 BOOT_FRAME_OK 才能执行 frame 中的命令
 */
BootFrameStatus_t Boot_Protocol_ReceiveFrame(
    BootFrame_t *frame,
    uint32_t byte_timeout_ms
);

/**
 * @brief 按统一格式发送一帧，并自动计算帧 CRC32
 */
void Boot_Protocol_SendFrame(
    uint8_t command,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t payload_length
);

/** @brief 回复 HELLO，告诉上位机当前协议和可升级区域参数 */
void Boot_Protocol_SendHelloResponse(
    uint16_t sequence
);

/** @brief 回复 NACK，说明当前请求失败及失败原因 */
void Boot_Protocol_SendNack(
    uint16_t sequence,
    BootError_t error
);

/**
 * @brief 回复 ACK
 * @param acknowledged_command 本次确认的是 START、DATA 还是 END
 * @param next_offset DATA时表示下一期望偏移；START为0；END返回完成镜像长度
 */
void Boot_Protocol_SendAck(
    uint16_t sequence,
    uint8_t acknowledged_command,
    uint32_t next_offset
);

#endif
