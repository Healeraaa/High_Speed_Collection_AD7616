#include "boot_upgrade.h"
#include "boot_crc32.h"
#include "main.h"
#include <string.h>

#define BOOT_FLASH_WORD_SIZE  32U
#define BOOT_DATA_OFFSET_SIZE 4U
#define BOOT_DATA_MAX_SIZE    992U


typedef enum
{
    BOOT_UPGRADE_STATE_IDLE = 0,
    BOOT_UPGRADE_STATE_RECEIVING,
    BOOT_UPGRADE_STATE_COMPLETE

} BootUpgradeState_t;

typedef struct
{
    BootUpgradeState_t state;

    uint32_t image_size;
    uint32_t expected_crc32;

    uint32_t received_size;
    uint32_t write_address;

} BootUpgradeContext_t;

static BootUpgradeContext_t s_upgrade_context;

static uint32_t Boot_Upgrade_ReadU32LE(
    const uint8_t *data
)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

void Boot_Upgrade_Init(void)
{
    s_upgrade_context.state = BOOT_UPGRADE_STATE_IDLE;
    s_upgrade_context.image_size = 0U;
    s_upgrade_context.expected_crc32 = 0U;
    s_upgrade_context.received_size = 0U;
    s_upgrade_context.write_address = BOOT_APP_BASE_ADDRESS;
}

BootError_t Boot_Upgrade_HandleStart(
    const BootFrame_t *frame
)
{
    FLASH_EraseInitTypeDef erase_init = {0};
    HAL_StatusTypeDef hal_status;
    uint32_t sector_error = 0xFFFFFFFFUL;
    uint32_t image_size;
    uint32_t image_crc32;

    if (frame == 0)
    {
        return BOOT_ERROR_COMMAND;
    }

    if (frame->payload_length != 8U)
    {
        return BOOT_ERROR_FRAME_LENGTH;
    }

    image_size = Boot_Upgrade_ReadU32LE(
        &frame->payload[0]
    );

    image_crc32 = Boot_Upgrade_ReadU32LE(
        &frame->payload[4]
    );

    if ((image_size == 0U) ||
        (image_size > BOOT_APP_MAX_SIZE))
    {
        return BOOT_ERROR_IMAGE_SIZE;
    }

    /*
     * 一旦收到新的 START，先使旧升级会话失效。
     */
    Boot_Upgrade_Init();

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return BOOT_ERROR_FLASH_ERASE;
    }

    /*
     * 清除 Bank2 之前可能遗留的错误标志。
     */
    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_ALL_ERRORS_BANK2
    );

    /*
     * STM32H743 的 App 独占整个 Bank2：
     * 0x08100000～0x081FFFFF。
     *
     * 最简单版本直接擦除整个 Bank2，
     * 不会影响位于 Bank1 的 Boot。
     */
    erase_init.TypeErase = FLASH_TYPEERASE_MASSERASE;
    erase_init.Banks = FLASH_BANK_2;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    hal_status = HAL_FLASHEx_Erase(
        &erase_init,
        &sector_error
    );

    HAL_FLASH_Lock();

    if (hal_status != HAL_OK)
    {
        Boot_Upgrade_Init();
        return BOOT_ERROR_FLASH_ERASE;
    }

    s_upgrade_context.state =
        BOOT_UPGRADE_STATE_RECEIVING;

    s_upgrade_context.image_size =
        image_size;

    s_upgrade_context.expected_crc32 =
        image_crc32;

    s_upgrade_context.received_size =
        0U;

    s_upgrade_context.write_address =
        BOOT_APP_BASE_ADDRESS;

    return BOOT_ERROR_NONE;
}

BootError_t Boot_Upgrade_HandleData(
    const BootFrame_t *frame,
    uint32_t *next_offset
)
{
    uint32_t offset;
    uint32_t data_length;
    uint32_t processed;
    uint32_t copy_length;
    uint32_t address;

    uint32_t flash_word[8];

    HAL_StatusTypeDef hal_status;

    if ((frame == 0) ||
        (next_offset == 0))
    {
        return BOOT_ERROR_COMMAND;
    }

    *next_offset =
        s_upgrade_context.received_size;

    if (s_upgrade_context.state !=
        BOOT_UPGRADE_STATE_RECEIVING)
    {
        return BOOT_ERROR_COMMAND;
    }

    if ((frame->payload_length <=
         BOOT_DATA_OFFSET_SIZE) ||
        (frame->payload_length >
         (BOOT_DATA_OFFSET_SIZE +
          BOOT_DATA_MAX_SIZE)))
    {
        return BOOT_ERROR_FRAME_LENGTH;
    }

    offset = Boot_Upgrade_ReadU32LE(
        &frame->payload[0]
    );

    data_length =
        frame->payload_length -
        BOOT_DATA_OFFSET_SIZE;

    /*
     * 第一版只允许严格顺序写入。
     */
    if (offset !=
        s_upgrade_context.received_size)
    {
        return BOOT_ERROR_SEQUENCE;
    }

    /*
     * 每个包的起始地址必须满足
     * STM32H743 FlashWord 32 字节对齐。
     */
    if ((offset &
         (BOOT_FLASH_WORD_SIZE - 1U)) != 0U)
    {
        return BOOT_ERROR_SEQUENCE;
    }

    if ((offset >=
         s_upgrade_context.image_size) ||
        (data_length >
         (s_upgrade_context.image_size -
          offset)))
    {
        return BOOT_ERROR_IMAGE_SIZE;
    }

    /*
     * 不是最后一包时，数据长度必须是
     * 32 字节的整数倍。
     *
     * 最后一包允许不足 32 字节，
     * 剩余位置使用 0xFF 填充。
     */
    if (((offset + data_length) <
         s_upgrade_context.image_size) &&
        ((data_length %
          BOOT_FLASH_WORD_SIZE) != 0U))
    {
        return BOOT_ERROR_FRAME_LENGTH;
    }

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_ALL_ERRORS_BANK2
    );

    processed = 0U;
    hal_status = HAL_OK;

    while (processed < data_length)
    {
        memset(
            flash_word,
            0xFF,
            sizeof(flash_word)
        );

        copy_length =
            data_length - processed;

        if (copy_length >
            BOOT_FLASH_WORD_SIZE)
        {
            copy_length =
                BOOT_FLASH_WORD_SIZE;
        }

        memcpy(
            flash_word,
            &frame->payload[
                BOOT_DATA_OFFSET_SIZE +
                processed],
            copy_length
        );

        address =
            BOOT_APP_BASE_ADDRESS +
            offset +
            processed;

        hal_status = HAL_FLASH_Program(
            FLASH_TYPEPROGRAM_FLASHWORD,
            address,
            (uint32_t)flash_word
        );

        if (hal_status != HAL_OK)
        {
            break;
        }

        /*
         * 立即回读整个 FlashWord。
         */
        if (memcmp(
                (const void *)address,
                flash_word,
                BOOT_FLASH_WORD_SIZE) != 0)
        {
            hal_status = HAL_ERROR;
            break;
        }

        processed += copy_length;
    }

    HAL_FLASH_Lock();

    if (hal_status != HAL_OK)
    {
        /*
         * 写入失败后禁止继续发送 DATA。
         * 必须重新 START 并重新擦除。
         */
        Boot_Upgrade_Init();

        return BOOT_ERROR_FLASH_PROGRAM;
    }

    s_upgrade_context.received_size +=
        data_length;

    s_upgrade_context.write_address =
        BOOT_APP_BASE_ADDRESS +
        s_upgrade_context.received_size;

    *next_offset =
        s_upgrade_context.received_size;

    return BOOT_ERROR_NONE;
}

BootError_t Boot_Upgrade_HandleEnd(
    const BootFrame_t *frame,
    uint32_t *image_crc32,
    uint32_t *image_size
)
{
    uint32_t calculated_crc32;

    if ((frame == 0) ||
        (image_crc32 == 0) ||
        (image_size == 0))
    {
        return BOOT_ERROR_COMMAND;
    }

    *image_crc32 = 0U;
    *image_size = 0U;

    /*
     * END 命令不携带 Payload。
     */
    if (frame->payload_length != 0U)
    {
        return BOOT_ERROR_FRAME_LENGTH;
    }

    /*
     * 只有 START 成功且正在接收固件时，
     * 才允许执行 END。
     */
    if (s_upgrade_context.state !=
        BOOT_UPGRADE_STATE_RECEIVING)
    {
        return BOOT_ERROR_COMMAND;
    }

    /*
     * 已接收长度必须与 START 中声明的
     * 固件总长度完全一致。
     */
    if (s_upgrade_context.received_size !=
        s_upgrade_context.image_size)
    {
        return BOOT_ERROR_IMAGE_SIZE;
    }

    /*
     * 确保之前的 Flash 写入已经完成，
     * 然后直接读取 Bank2 中的数据计算 CRC32。
     */
    __DSB();
    __ISB();

    calculated_crc32 =
        Boot_CRC32_Calculate(
            (const uint8_t *)
                BOOT_APP_BASE_ADDRESS,
            s_upgrade_context.image_size
        );

    *image_crc32 = calculated_crc32;

    /*
     * 与 START 中上位机发送的整包 CRC32 比较。
     */
    if (calculated_crc32 !=
        s_upgrade_context.expected_crc32)
    {
        /*
         * CRC 错误后结束当前升级会话。
         * 必须重新发送 START 并重新擦除。
         */
        s_upgrade_context.state =
            BOOT_UPGRADE_STATE_IDLE;

        return BOOT_ERROR_IMAGE_CRC;
    }

    *image_size =
        s_upgrade_context.image_size;

    s_upgrade_context.state =
        BOOT_UPGRADE_STATE_COMPLETE;

    return BOOT_ERROR_NONE;
}