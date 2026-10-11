#ifndef __BOOT_IMAGE_INFO_H__
#define __BOOT_IMAGE_INFO_H__

#include "boot_protocol.h"
#include <stdint.h>

/*
 * 镜像有效性采用“两阶段提交”：
 *
 * 1. 先写 BootImageInfo_t，记录镜像长度和 CRC；
 * 2. 再写 App 的首个 Flash Word（其中包含向量表）；
 * 3. 最后写 BootImageCommit_t，宣布镜像正式有效。
 *
 * 如果任何一步中途掉电，最终提交记录不存在或与信息记录不一致，
 * Boot_App_IsValid() 就不会启动这个 App。
 */

/* 元数据结构的格式版本；将来改变结构含义时需要递增。 */
#define BOOT_IMAGE_INFO_FORMAT_VERSION   1UL

/*
 * 最终提交标记。STM32 为小端，因此内存中的4个字节依次为：
 * 'B' 'O' 'O' 'T'
 */
#define BOOT_IMAGE_COMMIT_MAGIC          0x544F4F42UL

/*
 * 目前升级协议还没有传递固件版本，
 * 暂时写入0，后面再扩展。
 */
#define BOOT_IMAGE_VERSION_UNKNOWN       0UL


/*
 * 固件信息记录，必须正好占用一个 Flash Word：32字节。
 *
 * 写入地址：0x081FFFC0。
 * 该记录回答“镜像应该有多长、正确 CRC 是多少、从哪里启动”。
 */
typedef struct
{
    /* 当前结构格式，防止新旧 Boot 对元数据产生不同理解。 */
    uint32_t format_version;

    /* 预留给应用版本；当前协议还没有传版本，因此写0。 */
    uint32_t firmware_version;

    /* App BIN 的真实字节数，不包含尾部两个元数据 Flash Word。 */
    uint32_t image_size;

    /* 上位机针对整个 BIN 计算的 CRC32。 */
    uint32_t image_crc32;

    /* 固定为0x08100000，防止拿错分区的元数据被接受。 */
    uint32_t app_base_address;

    /* 预留扩展字段，写入时保持擦除态0xFFFFFFFF。 */
    uint32_t reserved[2];

    /*
     * 前28字节的CRC32。
     */
    uint32_t info_crc32;

} BootImageInfo_t;


/*
 * 最终提交记录，必须正好占用一个 Flash Word：32字节。
 *
 * 写入地址：0x081FFFE0，并且必须是 END 阶段最后一次 Flash 写入。
 * 每个关键字段同时保存原值和按位取反值，避免掉电只写入部分位时
 * 恰好被误判为有效。
 */
typedef struct
{
    /* 固定字符串“BOOT”及其按位取反值。 */
    uint32_t magic;
    uint32_t magic_inverse;

    /* 绑定前一个 Flash Word 中的 BootImageInfo_t。 */
    uint32_t info_crc32;
    uint32_t info_crc32_inverse;

    /* 再保存一次镜像长度，必须与信息记录一致。 */
    uint32_t image_size;
    uint32_t image_size_inverse;

    /* 再保存一次镜像 CRC，必须与信息记录一致。 */
    uint32_t image_crc32;
    uint32_t image_crc32_inverse;

} BootImageCommit_t;


/*
 * 写入固件信息记录。
 * 应在写入 App 向量表之前调用；此函数成功不代表 App 已经有效。
 */
BootError_t Boot_ImageInfo_WriteInfo(
    uint32_t image_size,
    uint32_t image_crc32,
    uint32_t firmware_version
);


/*
 * 写入最终提交标记。
 * 应在 App 向量表成功写入后最后调用；成功后镜像才具备启动资格。
 */
BootError_t Boot_ImageInfo_WriteCommit(
    uint32_t image_size,
    uint32_t image_crc32
);


/*
 * 检查固件信息和最终提交标记是否存在、结构正确且相互一致。
 *
 * 这里只检查元数据，不计算整个 App 的 CRC32；完整 App CRC 由
 * Boot_App_IsValid() 在每次启动前计算。
 *
 * 三个输出指针都允许传 NULL；调用者只获取自己关心的信息即可。
 */
uint8_t Boot_ImageInfo_ReadValid(
    uint32_t *image_size,
    uint32_t *image_crc32,
    uint32_t *firmware_version
);


#endif
