#ifndef __BSP_BOOT_USART_H__
#define __BSP_BOOT_USART_H__

#include "main.h"

/** @brief 初始化 Boot 专用 USART1：PA9/PA10，115200-8-N-1，轮询模式 */
void BSP_BOOT_USART1_Init(void);

/**
 * @brief 在指定时间内轮询接收一个字节
 * @param data 接收结果保存地址
 * @param timeout_ms 等待超时时间，单位：ms
 * @return 1U：收到字节；0U：超时或参数错误
 */
uint8_t BSP_BOOT_USART1_ReceiveByte(
    uint8_t *data,
    uint32_t timeout_ms
);

/** @brief 阻塞发送一个字节，返回时该字节已经发送完成 */
void BSP_BOOT_USART1_SendByte(uint8_t data);

/**
 * @brief 阻塞发送一段连续数据
 * @param data 数据首地址
 * @param length 数据长度，单位：字节
 */
void BSP_BOOT_USART1_SendBuffer(
    const uint8_t *data,
    uint32_t length
);

#endif
