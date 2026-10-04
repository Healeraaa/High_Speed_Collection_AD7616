#ifndef __BOOT_IMAGE_INFO_H__
#define __BOOT_IMAGE_INFO_H__

#include "boot_protocol.h"
#include <stdint.h>


#define BOOT_IMAGE_INFO_FORMAT_VERSION   1UL

/*
 * 内存中的4个字节依次为：
 * 'B' 'O' 'O' 'T'
 */
#define BOOT_IMAGE_COMMIT_MAGIC          0x544F4F42UL

/*
 * 目前升级协议还没有传递固件版本，
 * 暂时写入0，后面再扩展。
 */
#define BOOT_IMAGE_VERSION_UNKNOWN       0UL


/*
 * 固件信息记录，必须正好占用一个Flash Word：32字节。
 *
 * 写入地址：
 * 0x081FFFC0
 */
typedef struct
{
    uint32_t format_version;
    uint32_t firmware_version;

    uint32_t image_size;
    uint32_t image_crc32;

    uint32_t app_base_address;

    uint32_t reserved[2];

    /*
     * 前28字节的CRC32。
     */
    uint32_t info_crc32;

} BootImageInfo_t;


/*
 * 最终提交记录，必须正好占用一个Flash Word：32字节。
 *
 * 写入地址：
 * 0x081FFFE0
 */
typedef struct
{
    uint32_t magic;
    uint32_t magic_inverse;

    uint32_t info_crc32;
    uint32_t info_crc32_inverse;

    uint32_t image_size;
    uint32_t image_size_inverse;

    uint32_t image_crc32;
    uint32_t image_crc32_inverse;

} BootImageCommit_t;


/*
 * 写入固件信息记录。
 * 应在写入App向量表之前调用。
 */
BootError_t Boot_ImageInfo_WriteInfo(
    uint32_t image_size,
    uint32_t image_crc32,
    uint32_t firmware_version
);


/*
 * 写入最终提交标记。
 * 应在App向量表成功写入后最后调用。
 */
BootError_t Boot_ImageInfo_WriteCommit(
    uint32_t image_size,
    uint32_t image_crc32
);


/*
 * 检查固件信息和最终提交标记是否一致。
 *
 * 这里只检查元数据，不计算整个App的CRC32。
 */
uint8_t Boot_ImageInfo_ReadValid(
    uint32_t *image_size,
    uint32_t *image_crc32,
    uint32_t *firmware_version
);


#endif