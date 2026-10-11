#ifndef __BOOT_CRC32_H__
#define __BOOT_CRC32_H__

#include <stdint.h>

/**
 * @brief 一次性计算一段连续数据的 CRC32/IEEE
 * @param data 数据首地址
 * @param length 数据长度，单位：字节
 * @return 与上位机 qChecksum/zlib.crc32 对应算法一致的 32 位 CRC
 */
uint32_t Boot_CRC32_Calculate(
    const uint8_t *data,
    uint32_t length
);

/**
 * @brief 在已有 CRC 中间值的基础上继续追加数据
 *
 * 用于“数据不连续但逻辑上属于同一段”的情况，例如 END 阶段：
 * App 前 32 字节还在 RAM，其余内容已经在 Flash，可分两次 Update 后再统一异或。
 *
 * @param crc 上一次的 CRC 中间值；第一次通常传 0xFFFFFFFF
 * @param data 本次追加的数据
 * @param length 本次追加的字节数
 * @return 更新后的中间值；本函数不执行最后的异或
 */
uint32_t Boot_CRC32_Update(
    uint32_t crc,
    const uint8_t *data,
    uint32_t length
);

#endif
