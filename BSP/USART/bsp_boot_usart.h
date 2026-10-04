#ifndef __BSP_BOOT_USART_H__
#define __BSP_BOOT_USART_H__

#include "main.h"

void BSP_BOOT_USART1_Init(void);

uint8_t BSP_BOOT_USART1_ReceiveByte(
    uint8_t *data,
    uint32_t timeout_ms
);

void BSP_BOOT_USART1_SendByte(uint8_t data);

void BSP_BOOT_USART1_SendBuffer(
    const uint8_t *data,
    uint32_t length
);

#endif
