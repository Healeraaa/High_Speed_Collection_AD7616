#include "boot_image_info.h"
#include "boot_crc32.h"
#include "main.h"
#include <string.h>

/*
 * Bank2尾部元数据管理
 * ===================
 *
 * 0x081FFFC0：BootImageInfo_t   固件信息
 * 0x081FFFE0：BootImageCommit_t 最终提交记录
 *
 * 这两个地址属于Bank2最后64字节，不包含在App BIN和BOOT_APP_MAX_SIZE中。
 * START擦除整个Bank2后两块记录都消失；只有END验证成功才重新写回。
 */

/* STM32H743的最小Flash编程单位，不是普通的4字节Word。 */
#define BOOT_FLASH_WORD_SIZE 32U

/*
 * 编译期检查两个结构体必须正好为32字节。
 *
 * C89/ARMCC5没有统一的static_assert，因此使用数组长度技巧：条件为真时
 * 长度为1；如果结构体因字段变化或对齐变成非32字节，长度为-1并直接
 * 触发编译错误，避免把错误布局写入Flash。
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

    /* Flash Word写入地址必须32字节对齐。 */
    if ((address &
         (BOOT_FLASH_WORD_SIZE - 1U)) != 0U)
    {
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    /* Flash默认锁定，避免程序跑飞时意外修改。 */
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return BOOT_ERROR_FLASH_PROGRAM;
    }

    /* 清除Bank2上一次操作可能遗留的错误标志。 */
    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_ALL_ERRORS_BANK2
    );

    /* 一次把完整32字节结构体编程到指定地址。 */
    hal_status = HAL_FLASH_Program(
        FLASH_TYPEPROGRAM_FLASHWORD,
        address,
        (uint32_t)data
    );

    /* 无论成功或失败，操作后都立即重新锁住Flash。 */
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

    /* 保证写入及回读完成后再执行后续提交步骤。 */
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

    /* 结构版本不匹配时，当前Boot不能安全解释其它字段。 */
    if (info->format_version !=
        BOOT_IMAGE_INFO_FORMAT_VERSION)
    {
        return 0U;
    }

    /* 本Boot只允许启动固定Bank2 App地址。 */
    if (info->app_base_address !=
        BOOT_APP_BASE_ADDRESS)
    {
        return 0U;
    }

    /* 镜像必须至少含首个Flash Word，且不能覆盖元数据区。 */
    if ((info->image_size < 32U) ||
        (info->image_size >
         BOOT_APP_MAX_SIZE))
    {
        return 0U;
    }

    /*
     * info_crc32字段位于结构体最后4字节，因此这里只计算前28字节。
     * 如果掉电导致信息结构只写入一部分，CRC大概率不会匹配。
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
     * 整个结构先填0xFF，使reserved字段保持Flash自然擦除态。
     * 这样以后增加字段时也容易识别“旧版本没有写过”的内容。
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

    /* 最后计算前28字节元数据自身的CRC。 */
    info.info_crc32 =
        Boot_CRC32_Calculate(
            (const uint8_t *)&info,
            sizeof(BootImageInfo_t) -
                sizeof(uint32_t)
        );

    /* 写入第一个保留Flash Word，但此时还没有Commit，App仍无效。 */
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

    /* STM32 Flash是内存映射的，可以像只读结构体一样直接访问。 */
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

    /*
     * Commit的每个重要字段都同时保存原值和按位反值。
     * ReadValid()要求 value_inverse == ~value，掉电产生的半写数据很难
     * 同时满足这组关系。
     */
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

    /* 这是END流程最后一次Flash写入；成功后App才正式有效。 */
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

    /*
     * 输出参数允许传NULL。非NULL时先清零，保证任何失败路径都不会把
     * 上一次调用留下的长度或CRC误当成有效值。
     */
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

    /* 两个结构体直接映射到Bank2最后两个Flash Word。 */
    info = (const BootImageInfo_t *)
        BOOT_IMAGE_INFO_ADDRESS;

    commit = (const BootImageCommit_t *)
        BOOT_IMAGE_COMMIT_ADDRESS;

    /* 第一组检查：Info内部字段、范围和Info自身CRC。 */
    if (Boot_ImageInfo_IsInfoValid(info) == 0U)
    {
        return 0U;
    }

    /* 第二组检查：Commit标志和它的反码。擦除态0xFFFFFFFF不会通过。 */
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

    /* Commit必须绑定当前Info记录，并且原值/反值同时匹配。 */
    if ((commit->info_crc32 !=
         info->info_crc32) ||
        (commit->info_crc32_inverse !=
         (~info->info_crc32)))
    {
        return 0U;
    }

    /* Commit保存的长度必须与Info一致。 */
    if ((commit->image_size !=
         info->image_size) ||
        (commit->image_size_inverse !=
         (~info->image_size)))
    {
        return 0U;
    }

    /* Commit保存的镜像CRC也必须与Info一致。 */
    if ((commit->image_crc32 !=
         info->image_crc32) ||
        (commit->image_crc32_inverse !=
         (~info->image_crc32)))
    {
        return 0U;
    }

    /* 所有检查通过后才把元数据交给调用者。 */
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
