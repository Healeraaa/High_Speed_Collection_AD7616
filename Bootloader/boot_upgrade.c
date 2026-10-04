#include "boot_upgrade.h"
#include "boot_crc32.h"
#include "boot_image_info.h"
#include "main.h"
#include <string.h>

#define BOOT_FLASH_WORD_SIZE 32U
#define BOOT_DATA_OFFSET_SIZE 4U
#define BOOT_DATA_MAX_SIZE 992U

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

    /*
     * App首个Flash Word包含MSP和Reset_Handler。
     * DATA阶段只保存在RAM，END校验成功后才写入Flash。
     */
    uint32_t first_flash_word[8];
    uint8_t first_flash_word_received;

} BootUpgradeContext_t;

static BootUpgradeContext_t s_upgrade_context;

static uint32_t Boot_Upgrade_ReadU32LE(
    const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) |
           ((uint32_t)data[3] << 24U);
}

void Boot_Upgrade_Init(void)
{
    s_upgrade_context.state =
        BOOT_UPGRADE_STATE_IDLE;

    s_upgrade_context.image_size = 0U;
    s_upgrade_context.expected_crc32 = 0U;
    s_upgrade_context.received_size = 0U;

    s_upgrade_context.write_address =
        BOOT_APP_BASE_ADDRESS;

    /*
     * 未完成升级时，暂存区保持擦除态数据。
     */
    memset(
        s_upgrade_context.first_flash_word,
        0xFF,
        sizeof(s_upgrade_context.first_flash_word));

    s_upgrade_context.first_flash_word_received = 0U;
}

BootError_t Boot_Upgrade_HandleStart(
    const BootFrame_t *frame)
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
        &frame->payload[0]);

    image_crc32 = Boot_Upgrade_ReadU32LE(
        &frame->payload[4]);

    if ((image_size < BOOT_FLASH_WORD_SIZE) ||
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
        FLASH_FLAG_ALL_ERRORS_BANK2);

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
        &sector_error);

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

/**
 * @brief 检查重复DATA包是否与已经接收的数据一致
 *
 * @note 首个32字节尚未写入Flash，必须与RAM暂存区比较；
 *       其余内容直接与Flash比较。
 */
static uint8_t Boot_Upgrade_IsRepeatedDataValid(
    const BootFrame_t *frame,
    uint32_t offset,
    uint32_t data_length)
{
    uint32_t checked_length;

    if (frame == 0)
    {
        return 0U;
    }

    checked_length = 0U;

    /*
     * 首包中的前32字节保存在RAM，没有写入Flash。
     */
    if (offset == 0U)
    {
        if ((data_length < BOOT_FLASH_WORD_SIZE) ||
            (s_upgrade_context.first_flash_word_received == 0U))
        {
            return 0U;
        }

        if (memcmp(
                s_upgrade_context.first_flash_word,
                &frame->payload[BOOT_DATA_OFFSET_SIZE],
                BOOT_FLASH_WORD_SIZE) != 0)
        {
            return 0U;
        }

        checked_length =
            BOOT_FLASH_WORD_SIZE;
    }

    /*
     * 比较已经写入Flash的其余数据。
     * 最后一包不足32字节时，只比较实际固件长度。
     */
    if (checked_length < data_length)
    {
        if (memcmp(
                (const void *)(BOOT_APP_BASE_ADDRESS +
                               offset +
                               checked_length),
                &frame->payload[BOOT_DATA_OFFSET_SIZE +
                                checked_length],
                data_length -
                    checked_length) != 0)
        {
            return 0U;
        }
    }

    return 1U;
}

BootError_t Boot_Upgrade_HandleData(
    const BootFrame_t *frame,
    uint32_t *next_offset)
{
    uint32_t offset;
    uint32_t data_length;
    uint32_t processed;
    uint32_t copy_length;
    uint32_t address;

    uint32_t flash_word[8];

    uint8_t contains_first_flash_word;

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
        &frame->payload[0]);

    data_length =
        frame->payload_length -
        BOOT_DATA_OFFSET_SIZE;

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

    /*
     * 正常新包：
     *
     * offset == received_size
     *
     * 上一包重发：
     *
     * offset < received_size，并且该包结束位置
     * 正好等于当前received_size。
     *
     * 这通常表示设备已经完成Flash写入，
     * 但上一次ACK在串口中丢失。
     */
    if (offset !=
        s_upgrade_context.received_size)
    {
        if ((offset <
             s_upgrade_context.received_size) &&
            (data_length ==
             (s_upgrade_context.received_size -
              offset)) &&
            (Boot_Upgrade_IsRepeatedDataValid(
                 frame,
                 offset,
                 data_length) != 0U))
        {
            /*
             * 数据已经正确存在。
             * 不解锁Flash、不重复编程，
             * 直接返回成功，让上层重新发送ACK。
             */
            *next_offset =
                s_upgrade_context.received_size;

            return BOOT_ERROR_NONE;
        }

        /*
         * 不是当前新包，也不是刚才上一包的合法重发。
         */
        return BOOT_ERROR_SEQUENCE;
    }
    
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_ALL_ERRORS_BANK2);

    processed = 0U;
    hal_status = HAL_OK;
    contains_first_flash_word = 0U;

    /*
     * 偏移0的数据包含App向量表。
     *
     * 只复制到RAM，不立即写入0x08100000。
     * 因此升级未完成时，Flash中的初始MSP和
     * Reset_Handler仍然保持0xFFFFFFFF。
     */
    if (offset == 0U)
    {
        if (data_length < BOOT_FLASH_WORD_SIZE)
        {
            HAL_FLASH_Lock();
            return BOOT_ERROR_FRAME_LENGTH;
        }

        memcpy(
            s_upgrade_context.first_flash_word,
            &frame->payload[BOOT_DATA_OFFSET_SIZE],
            BOOT_FLASH_WORD_SIZE);

        processed = BOOT_FLASH_WORD_SIZE;
        contains_first_flash_word = 1U;
    }

    while (processed < data_length)
    {
        memset(
            flash_word,
            0xFF,
            sizeof(flash_word));

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
            &frame->payload[BOOT_DATA_OFFSET_SIZE +
                            processed],
            copy_length);

        address =
            BOOT_APP_BASE_ADDRESS +
            offset +
            processed;

        hal_status = HAL_FLASH_Program(
            FLASH_TYPEPROGRAM_FLASHWORD,
            address,
            (uint32_t)flash_word);

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
    if (contains_first_flash_word != 0U)
    {
        s_upgrade_context.first_flash_word_received = 1U;
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
    uint32_t *image_size)
{
    uint32_t calculated_crc32;
    uint32_t crc;
    HAL_StatusTypeDef hal_status;
    BootError_t image_info_status;

    if ((frame == 0) ||
        (image_crc32 == 0) ||
        (image_size == 0))
    {
        return BOOT_ERROR_COMMAND;
    }

    *image_crc32 = 0U;
    *image_size = 0U;

    /*
     * END命令不携带Payload。
     */
    if (frame->payload_length != 0U)
    {
        return BOOT_ERROR_FRAME_LENGTH;
    }

    if (s_upgrade_context.state !=
        BOOT_UPGRADE_STATE_RECEIVING)
    {
        return BOOT_ERROR_COMMAND;
    }

    if (s_upgrade_context.received_size !=
        s_upgrade_context.image_size)
    {
        return BOOT_ERROR_IMAGE_SIZE;
    }

    /*
     * 必须已经收到并暂存首个Flash Word。
     */
    if (s_upgrade_context.first_flash_word_received == 0U)
    {
        return BOOT_ERROR_IMAGE_SIZE;
    }

    __DSB();
    __ISB();

    /*
     * 计算完整固件CRC：
     *
     * 前32字节来自RAM暂存区；
     * 其余数据直接从Bank2 Flash读取。
     */
    crc = 0xFFFFFFFFUL;

    crc = Boot_CRC32_Update(
        crc,
        (const uint8_t *)
            s_upgrade_context.first_flash_word,
        BOOT_FLASH_WORD_SIZE);

    crc = Boot_CRC32_Update(
        crc,
        (const uint8_t *)(BOOT_APP_BASE_ADDRESS +
                          BOOT_FLASH_WORD_SIZE),
        s_upgrade_context.image_size -
            BOOT_FLASH_WORD_SIZE);

    calculated_crc32 =
        crc ^ 0xFFFFFFFFUL;

    *image_crc32 = calculated_crc32;

    /*
     * 校验失败时不写向量表。
     * 0x08100000仍保持擦除状态，
     * Boot_IsAppValid()将拒绝启动这个App。
     */
    if (calculated_crc32 !=
        s_upgrade_context.expected_crc32)
    {
        Boot_Upgrade_Init();
        return BOOT_ERROR_IMAGE_CRC;
    }

    /*
     * 第一步：写入固件信息。
     *
     * 此时向量表和最终提交标记仍然没有写入，
     * 中途复位时固件仍然不能被认为有效。
     */
    image_info_status =
        Boot_ImageInfo_WriteInfo(
            s_upgrade_context.image_size,
            calculated_crc32,
            BOOT_IMAGE_VERSION_UNKNOWN);

    if (image_info_status !=
        BOOT_ERROR_NONE)
    {
        Boot_Upgrade_Init();
        return image_info_status;
    }

    /*
     * 整体CRC正确后，最后写入包含向量表的
     * 第一个32字节Flash Word。
     */
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        Boot_Upgrade_Init();
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_ALL_ERRORS_BANK2);

    hal_status = HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_FLASHWORD,
        BOOT_APP_BASE_ADDRESS,
        (uint32_t)
            s_upgrade_context.first_flash_word);

    HAL_FLASH_Lock();

    if (hal_status != HAL_OK)
    {
        Boot_Upgrade_Init();
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    /*
     * 回读验证刚写入的向量表。
     */
    if (memcmp(
            (const void *)BOOT_APP_BASE_ADDRESS,
            s_upgrade_context.first_flash_word,
            BOOT_FLASH_WORD_SIZE) != 0)
    {
        Boot_Upgrade_Init();
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    __DSB();
    __ISB();

    /*
     * 第三步：最后写入提交标记。
     *
     * 只有固件信息、完整App和向量表都成功后，
     * 才会存在有效提交记录。
     */
    image_info_status =
        Boot_ImageInfo_WriteCommit(
            s_upgrade_context.image_size,
            calculated_crc32);

    if (image_info_status !=
        BOOT_ERROR_NONE)
    {
        Boot_Upgrade_Init();
        return image_info_status;
    }

    __DSB();
    __ISB();

    *image_size =
        s_upgrade_context.image_size;

    s_upgrade_context.state =
        BOOT_UPGRADE_STATE_COMPLETE;

    return BOOT_ERROR_NONE;
}