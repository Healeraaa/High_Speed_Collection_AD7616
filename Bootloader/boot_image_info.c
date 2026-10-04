#include "boot_image_info.h"
#include "boot_crc32.h"
#include "main.h"
#include <string.h>


#define BOOT_FLASH_WORD_SIZE 32U


/*
 * 编译期检查两个结构体必须正好为32字节。
 */
typedef char BootImageInfoSizeCheck[
    (sizeof(BootImageInfo_t) ==
     BOOT_FLASH_WORD_SIZE) ? 1 : -1
];

typedef char BootImageCommitSizeCheck[
    (sizeof(BootImageCommit_t) ==
     BOOT_FLASH_WORD_SIZE) ? 1 : -1
];


static BootError_t Boot_ImageInfo_ProgramFlashWord(
    uint32_t address,
    const void *data
)
{
    HAL_StatusTypeDef hal_status;

    if (data == 0)
    {
        return BOOT_ERROR_COMMAND;
    }

    if ((address &
         (BOOT_FLASH_WORD_SIZE - 1U)) != 0U)
    {
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_ALL_ERRORS_BANK2
    );

    hal_status = HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_FLASHWORD,
        address,
        (uint32_t)data
    );

    HAL_FLASH_Lock();

    if (hal_status != HAL_OK)
    {
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    /*
     * 写入后立即回读验证整个Flash Word。
     */
    if (memcmp(
            (const void *)address,
            data,
            BOOT_FLASH_WORD_SIZE) != 0)
    {
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    __DSB();
    __ISB();

    return BOOT_ERROR_NONE;
}


static uint8_t Boot_ImageInfo_IsInfoValid(
    const BootImageInfo_t *info
)
{
    uint32_t calculated_crc32;

    if (info == 0)
    {
        return 0U;
    }

    if (info->format_version !=
        BOOT_IMAGE_INFO_FORMAT_VERSION)
    {
        return 0U;
    }

    if (info->app_base_address !=
        BOOT_APP_BASE_ADDRESS)
    {
        return 0U;
    }

    if ((info->image_size < 32U) ||
        (info->image_size >
         BOOT_APP_MAX_SIZE))
    {
        return 0U;
    }

    /*
     * info_crc32字段位于结构体最后4字节，
     * 因此这里只计算前28字节。
     */
    calculated_crc32 =
        Boot_CRC32_Calculate(
            (const uint8_t *)info,
            sizeof(BootImageInfo_t) -
                sizeof(uint32_t)
        );

    if (calculated_crc32 !=
        info->info_crc32)
    {
        return 0U;
    }

    return 1U;
}



BootError_t Boot_ImageInfo_WriteInfo(
    uint32_t image_size,
    uint32_t image_crc32,
    uint32_t firmware_version
)
{
    BootImageInfo_t info;

    if ((image_size < 32U) ||
        (image_size > BOOT_APP_MAX_SIZE))
    {
        return BOOT_ERROR_IMAGE_SIZE;
    }

    /*
     * 未使用字段保持Flash擦除态0xFFFFFFFF。
     */
    memset(
        &info,
        0xFF,
        sizeof(info)
    );

    info.format_version =
        BOOT_IMAGE_INFO_FORMAT_VERSION;

    info.firmware_version =
        firmware_version;

    info.image_size =
        image_size;

    info.image_crc32 =
        image_crc32;

    info.app_base_address =
        BOOT_APP_BASE_ADDRESS;

    info.info_crc32 =
        Boot_CRC32_Calculate(
            (const uint8_t *)&info,
            sizeof(BootImageInfo_t) -
                sizeof(uint32_t)
        );

    return Boot_ImageInfo_ProgramFlashWord(
        BOOT_IMAGE_INFO_ADDRESS,
        &info
    );
}


BootError_t Boot_ImageInfo_WriteCommit(
    uint32_t image_size,
    uint32_t image_crc32
)
{
    const BootImageInfo_t *info;
    BootImageCommit_t commit;

    info = (const BootImageInfo_t *)
        BOOT_IMAGE_INFO_ADDRESS;

    /*
     * 写提交标记前，再确认固件信息记录完整。
     */
    if (Boot_ImageInfo_IsInfoValid(info) == 0U)
    {
        return BOOT_ERROR_IMAGE_CRC;
    }

    if ((info->image_size != image_size) ||
        (info->image_crc32 != image_crc32))
    {
        return BOOT_ERROR_IMAGE_CRC;
    }

    commit.magic =
        BOOT_IMAGE_COMMIT_MAGIC;

    commit.magic_inverse =
        ~BOOT_IMAGE_COMMIT_MAGIC;

    commit.info_crc32 =
        info->info_crc32;

    commit.info_crc32_inverse =
        ~info->info_crc32;

    commit.image_size =
        image_size;

    commit.image_size_inverse =
        ~image_size;

    commit.image_crc32 =
        image_crc32;

    commit.image_crc32_inverse =
        ~image_crc32;

    return Boot_ImageInfo_ProgramFlashWord(
        BOOT_IMAGE_COMMIT_ADDRESS,
        &commit
    );
}


uint8_t Boot_ImageInfo_ReadValid(
    uint32_t *image_size,
    uint32_t *image_crc32,
    uint32_t *firmware_version
)
{
    const BootImageInfo_t *info;
    const BootImageCommit_t *commit;

    if (image_size != 0)
    {
        *image_size = 0U;
    }

    if (image_crc32 != 0)
    {
        *image_crc32 = 0U;
    }

    if (firmware_version != 0)
    {
        *firmware_version = 0U;
    }

    info = (const BootImageInfo_t *)
        BOOT_IMAGE_INFO_ADDRESS;

    commit = (const BootImageCommit_t *)
        BOOT_IMAGE_COMMIT_ADDRESS;

    if (Boot_ImageInfo_IsInfoValid(info) == 0U)
    {
        return 0U;
    }

    if (commit->magic !=
        BOOT_IMAGE_COMMIT_MAGIC)
    {
        return 0U;
    }

    if (commit->magic_inverse !=
        (~BOOT_IMAGE_COMMIT_MAGIC))
    {
        return 0U;
    }

    if ((commit->info_crc32 !=
         info->info_crc32) ||
        (commit->info_crc32_inverse !=
         (~info->info_crc32)))
    {
        return 0U;
    }

    if ((commit->image_size !=
         info->image_size) ||
        (commit->image_size_inverse !=
         (~info->image_size)))
    {
        return 0U;
    }

    if ((commit->image_crc32 !=
         info->image_crc32) ||
        (commit->image_crc32_inverse !=
         (~info->image_crc32)))
    {
        return 0U;
    }

    if (image_size != 0)
    {
        *image_size =
            info->image_size;
    }

    if (image_crc32 != 0)
    {
        *image_crc32 =
            info->image_crc32;
    }

    if (firmware_version != 0)
    {
        *firmware_version =
            info->firmware_version;
    }

    return 1U;
}
