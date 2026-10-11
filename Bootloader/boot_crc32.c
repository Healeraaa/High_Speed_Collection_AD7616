#include "boot_crc32.h"

/*
 * CRC32 在本工程中有三处用途：
 * 1. 每个串口协议帧自带 CRC，发现传输中的比特错误；
 * 2. END 阶段校验整个 BIN 与上位机发送前是否一致；
 * 3. 每次启动 App 前再次校验 Flash 中的完整镜像。
 *
 * 这里使用纯软件算法，不依赖 STM32 CRC 外设。这样上位机、Boot 和
 * 启动校验使用完全相同的 CRC32/IEEE 参数，避免硬件 CRC 配置差异。
 */

/**
 * @brief 更新 CRC32/IEEE
 *
 * 多项式：0xEDB88320（标准多项式0x04C11DB7的反射形式）
 * 初始值由调用者传入；不在本函数中执行最终异或。
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
        /* 把当前输入字节合并到 CRC 的最低8位。 */
        crc ^= data[index];

        /* 每个字节包含8个比特，因此循环更新8次。 */
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x00000001UL) != 0UL)
            {
                /* 最低位为1：右移后还要与生成多项式异或。 */
                crc =
                    (crc >> 1U) ^
                    0xEDB88320UL;
            }
            else
            {
                /* 最低位为0：只需要右移。 */
                crc >>= 1U;
            }
        }
    }

    return crc;
}


/**
 * @brief 完整计算 CRC32/IEEE
 *
 * 完整算法参数：初始值0xFFFFFFFF，最终再异或0xFFFFFFFF。
 * 与 Python zlib.crc32() 和 Qt 上位机中的计算结果一致。
 */
uint32_t Boot_CRC32_Calculate(
    const uint8_t *data,
    uint32_t length
)
{
    uint32_t crc;

    /* CRC32/IEEE 的标准初始值。 */
    crc = 0xFFFFFFFFUL;

    crc = Boot_CRC32_Update(
        crc,
        data,
        length
    );

    /* CRC32/IEEE 要求输出前对全部32位取反。 */
    return crc ^ 0xFFFFFFFFUL;
}
