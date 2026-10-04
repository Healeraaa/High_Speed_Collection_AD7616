#ifndef __BOOT_UPGRADE_H__
#define __BOOT_UPGRADE_H__

#include "boot_protocol.h"

void Boot_Upgrade_Init(void);

BootError_t Boot_Upgrade_HandleStart(
    const BootFrame_t *frame
);

BootError_t Boot_Upgrade_HandleData(
    const BootFrame_t *frame,
    uint32_t *next_offset
);

BootError_t Boot_Upgrade_HandleEnd(
    const BootFrame_t *frame,
    uint32_t *image_crc32,
    uint32_t *image_size
);
#endif
