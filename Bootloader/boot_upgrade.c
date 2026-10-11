#include "boot_upgrade.h"
#include "boot_crc32.h"
#include "boot_image_info.h"
#include "main.h"
#include <string.h>

/*
 * Flash升级核心
 * ==========
 *
 * 一次完整升级严格按以下顺序进行：
 *
 *   START -> 擦除整个Bank2并保存镜像长度/CRC
 *   DATA  -> 按offset连续写入，返回下一个期望offset
 *   END   -> 计算整体CRC，写Info，补写首32字节，最后写Commit
 *
 * 关键安全设计：DATA阶段不写App最前面的32字节。因为前8字节就是MSP和
 * Reset_Handler，只要它们保持擦除态，即使升级中途掉电也不会形成一个
 * 看似可启动的向量表。只有END整体CRC正确后才补写这32字节。
 */

/* STM32H743内部Flash一次只能按256bit，也就是32字节Flash Word编程。 */
#define BOOT_FLASH_WORD_SIZE 32U

/* 每个DATA Payload最前面的4字节用于保存固件offset。 */
#define BOOT_DATA_OFFSET_SIZE 4U

/* 最大Payload为1024字节，减去4字节offset，实际固件数据最多992字节。 */
#define BOOT_DATA_MAX_SIZE 992U

/* 一次升级会话的状态机。 */
typedef enum
{
    /* 尚未成功START，不能接收DATA或END。 */
    BOOT_UPGRADE_STATE_IDLE = 0,

    /* Bank2已擦除，正在按顺序接收DATA。 */
    BOOT_UPGRADE_STATE_RECEIVING,

    /* END和提交记录均成功，镜像已具备启动条件。 */
    BOOT_UPGRADE_STATE_COMPLETE

} BootUpgradeState_t;

typedef struct
{
    /* 当前状态决定哪些命令可以执行。 */
    BootUpgradeState_t state;

    /* START提供的BIN总长度和期望CRC32。 */
    uint32_t image_size;
    uint32_t expected_crc32;

    /* 已连续接收的字节数，以及下一个写地址。 */
    uint32_t received_size;
    uint32_t write_address;

    /*
     * App首个Flash Word包含向量表开头，其中最重要的是：
     *   Word[0]：初始MSP
     *   Word[1]：Reset_Handler
     * DATA阶段只保存在RAM，END校验成功后才写入Flash。
     */
    uint32_t first_flash_word[8];
    uint8_t first_flash_word_received;

} BootUpgradeContext_t;

static BootUpgradeContext_t s_upgrade_context;

/** @brief 从DATA/START的Payload中读取一个小端uint32_t */
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
    /*
     * 这里只清空RAM会话，不操作Flash。
     * 清成IDLE以后，即使上位机继续发旧DATA，也会收到COMMAND错误。
     */
    s_upgrade_context.state =
        BOOT_UPGRADE_STATE_IDLE;

    s_upgrade_context.image_size = 0U;
    s_upgrade_context.expected_crc32 = 0U;
    s_upgrade_context.received_size = 0U;

    s_upgrade_context.write_address =
        BOOT_APP_BASE_ADDRESS;

    /* 未收到首包前，RAM暂存区模拟Flash擦除态0xFF。 */
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

    /* START Payload必须严格为：image_size(4)+image_crc32(4)。 */
    if (frame->payload_length != 8U)
    {
        return BOOT_ERROR_FRAME_LENGTH;
    }

    /* 上位机发来的多字节字段采用小端。 */
    image_size = Boot_Upgrade_ReadU32LE(
        &frame->payload[0]);

    image_crc32 = Boot_Upgrade_ReadU32LE(
        &frame->payload[4]);

    /*
     * 至少要包含一个Flash Word；最大不能覆盖0x081FFFC0后的元数据区。
     */
    if ((image_size < BOOT_FLASH_WORD_SIZE) ||
        (image_size > BOOT_APP_MAX_SIZE))
    {
        return BOOT_ERROR_IMAGE_SIZE;
    }

    /* 新START总是放弃旧会话，避免新旧镜像状态混合。 */
    Boot_Upgrade_Init();

    /* STM32写/擦Flash前必须解锁，操作结束后立即重新上锁。 */
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return BOOT_ERROR_FLASH_ERASE;
    }

    /* 清除上一次Flash操作可能遗留的Bank2错误标志。 */
    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_ALL_ERRORS_BANK2);

    /*
     * STM32H743的App独占整个Bank2：
     * 0x08100000～0x081FFFFF。
     *
     * 当前V1直接Mass Erase整个Bank2，逻辑最简单，也不会影响Bank1中的Boot。
     * 擦除后App、Info和Commit全部变成0xFF，旧App立即失去启动资格。
     */
    erase_init.TypeErase = FLASH_TYPEERASE_MASSERASE;
    erase_init.Banks = FLASH_BANK_2;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    hal_status = HAL_FLASHEx_Erase(
        &erase_init,
        &sector_error);

    /* 无论擦除成功还是失败都重新锁住Flash。 */
    HAL_FLASH_Lock();

    if (hal_status != HAL_OK)
    {
        Boot_Upgrade_Init();
        return BOOT_ERROR_FLASH_ERASE;
    }

    /* 只有Bank2完整擦除成功后才进入RECEIVING。 */
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
 *
 * 串口可能出现“设备写成功且发出ACK，但上位机没有收到ACK”的情况。
 * 上位机会重发上一包。只有重发内容与已保存内容逐字节完全相同，设备
 * 才重新ACK；这样既能恢复通信，又不会对同一Flash Word重复编程。
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

    /* 已完成比较的长度，后续Flash比较从这里继续。 */
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
        /* Bank2为内存映射Flash，可以直接用memcmp回读比较。 */
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

    /*
     * 即使后续返回错误，也先把当前期望偏移告诉调用者，便于形成NACK后
     * 的诊断信息；当前NACK协议只返回错误码，成功ACK会返回该偏移。
     */
    *next_offset =
        s_upgrade_context.received_size;

    /* 没有成功START，或者会话已经失败/完成，都不能再写DATA。 */
    if (s_upgrade_context.state !=
        BOOT_UPGRADE_STATE_RECEIVING)
    {
        return BOOT_ERROR_COMMAND;
    }

    /* Payload必须至少包含4字节offset和1字节数据，且不能超过1024。 */
    if ((frame->payload_length <=
         BOOT_DATA_OFFSET_SIZE) ||
        (frame->payload_length >
         (BOOT_DATA_OFFSET_SIZE +
          BOOT_DATA_MAX_SIZE)))
    {
        return BOOT_ERROR_FRAME_LENGTH;
    }

    /* DATA Payload布局：[offset 4字节][firmware data N字节]。 */
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

    /* 包的起点和终点都必须落在START声明的真实镜像长度内。 */
    if ((offset >= s_upgrade_context.image_size) || (data_length > (s_upgrade_context.image_size - offset)))
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
    if (((offset + data_length) < s_upgrade_context.image_size) && ((data_length % BOOT_FLASH_WORD_SIZE) != 0U))
    {
        return BOOT_ERROR_FRAME_LENGTH;
    }

    /*
     * 正常新包：
     *
     * offset == received_size：
     * 这是设备当前正期待的新包。
     *
     * 上一包重发：
     *
     * offset < received_size，并且该包结束位置
     * 正好等于当前received_size。
     *
     * 这通常表示设备已经完成Flash写入，
     * 但上一次ACK在串口中丢失。
     */
    if (offset != s_upgrade_context.received_size)
    {
        if ((offset < s_upgrade_context.received_size) && (data_length == (s_upgrade_context.received_size - offset)) &&
            (Boot_Upgrade_IsRepeatedDataValid(frame, offset, data_length) != 0U))
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

    /* 通过所有顺序/长度检查后才解锁Flash，减少误写机会。 */
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
     * offset=0的数据包含App向量表。
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

        /* 只复制到RAM，processed=32使后续循环从第33字节开始写Flash。 */
        memcpy(
            s_upgrade_context.first_flash_word,
            &frame->payload[BOOT_DATA_OFFSET_SIZE],
            BOOT_FLASH_WORD_SIZE);

        processed = BOOT_FLASH_WORD_SIZE;
        contains_first_flash_word = 1U;
    }

    /* 把本包剩余数据按32字节Flash Word逐块写入。 */
    while (processed < data_length)
    {
        /*
         * 最后一包可能不足32字节。先填0xFF再覆盖真实数据，未使用部分
         * 保持擦除态，且仍满足HAL一次写满32字节的要求。
         */
        memset(flash_word, 0xFF, sizeof(flash_word));

        copy_length = data_length - processed;

        if (copy_length > BOOT_FLASH_WORD_SIZE)
        {
            copy_length = BOOT_FLASH_WORD_SIZE;
        }

        memcpy(flash_word, &frame->payload[BOOT_DATA_OFFSET_SIZE + processed], copy_length);

        /* App首地址 + BIN内偏移 + 当前包内已处理长度。 */
        address =
            BOOT_APP_BASE_ADDRESS +
            offset +
            processed;

        /* HAL要求data参数传入包含8个uint32_t的32字节缓冲区地址。 */
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

    /* 本包处理结束后立即重新锁Flash。 */
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
    /* 首32字节只有在本包其余Flash内容都成功后才标记为已接收。 */
    if (contains_first_flash_word != 0U)
    {
        s_upgrade_context.first_flash_word_received = 1U;
    }

    /* 只在整包成功后推进会话偏移，保证ACK表示整包已经可靠保存。 */
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

    /* 先清空输出，失败返回时调用者不会读到旧结果。 */
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

    /* 必须恰好收满START声明的镜像，少一个字节也不能提交。 */
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

    /* 确保此前Flash写入已经完成，再从内存映射地址读取并计算CRC。 */
    __DSB();
    __ISB();

    /*
     * 计算完整固件CRC：
     *
     * 前32字节来自RAM暂存区；
     * 其余数据直接从Bank2 Flash读取。
     */
    /* 分两段Update，逻辑结果等同于对完整BIN一次性Calculate。 */
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
     * Boot_App_IsValid()将拒绝启动这个App。
     */
    if (calculated_crc32 !=
        s_upgrade_context.expected_crc32)
    {
        Boot_Upgrade_Init();
        return BOOT_ERROR_IMAGE_CRC;
    }

    /*
     * 掉电安全提交第1步：写入固件信息。
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
     * 掉电安全提交第2步：写入包含向量表的
     * 第一个32字节Flash Word。
     */
    /* 单独解锁，只写首个Flash Word，随后立即锁定。 */
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
     * 掉电安全提交第3步：最后写入提交标记。
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

    /* 到这里三步均成功，才向上层返回最终镜像长度并标记COMPLETE。 */
    *image_size =
        s_upgrade_context.image_size;

    s_upgrade_context.state =
        BOOT_UPGRADE_STATE_COMPLETE;

    return BOOT_ERROR_NONE;
}
