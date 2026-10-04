#include "boot_crc32.h"


/**
 * @brief 更新 CRC32/IEEE
 *
 * 多项式：0xEDB88320
 * 初始值由调用者传入
 * 不在本函数中执行最终异或
 */
uint32_t Boot_CRC32_Update(
    uint32_t crc,
    const uint8_t *data,
    uint32_t length
)
{
    uint32_t index;
    uint32_t bit;

    if (data == 0)
    {
        return crc;
    }

    for (index = 0U; index < length; index++)
    {
        crc ^= data[index];

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x00000001UL) != 0UL)
            {
                crc =
                    (crc >> 1U) ^
                    0xEDB88320UL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}


/**
 * @brief 完整计算 CRC32/IEEE
 *
 * 与 Python zlib.crc32() 结果一致。
 */
uint32_t Boot_CRC32_Calculate(
    const uint8_t *data,
    uint32_t length
)
{
    uint32_t crc;

    crc = 0xFFFFFFFFUL;

    crc = Boot_CRC32_Update(
        crc,
        data,
        length
    );

    return crc ^ 0xFFFFFFFFUL;
}