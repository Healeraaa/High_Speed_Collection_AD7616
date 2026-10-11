#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "main.h"                    

#ifndef BSP_STATUS_T_DEFINED
#define BSP_STATUS_T_DEFINED
typedef enum {
    BSP_OK       = 0x00,
    BSP_ERROR    = 0x01,
    BSP_BUSY     = 0x02,
    BSP_TIMEOUT  = 0x03
} BSP_Status_t;
#endif

void BSP_USART1_Init(void);
uint8_t BSP_USART1_SendByte(uint8_t ch);
void BSP_USART1_SendBuffer(const uint8_t *data, uint16_t length);

/*
 * 从USART1中断环形缓冲区非阻塞读取一个字节。
 * 返回1表示成功取到数据，返回0表示当前没有数据。
 */
uint8_t BSP_USART1_ReadByte(uint8_t *data);

/* USART1真正的硬件中断处理，由stm32h7xx_it.c转调。 */
void BSP_USART1_IRQHandler(void);

/* 查询因环形缓冲区已满而丢弃的字节数量，便于调试接收压力。 */
uint32_t BSP_USART1_GetRxDropCount(void);

void BSP_USART3_Init(void);
uint8_t BSP_USART3_SendByte(uint8_t ch);


#endif /* __BSP_USART_H */
