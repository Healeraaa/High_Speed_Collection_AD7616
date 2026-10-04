#ifndef __BOOT_CRC32_H__
#define __BOOT_CRC32_H__

#include <stdint.h>

uint32_t Boot_CRC32_Calculate(
    const uint8_t *data,
    uint32_t length
);

uint32_t Boot_CRC32_Update(
    uint32_t crc,
    const uint8_t *data,
    uint32_t length
);

#endif