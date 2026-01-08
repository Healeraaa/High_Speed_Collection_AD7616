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

void BSP_USART3_Init(void);
uint8_t BSP_USART3_SendByte(uint8_t ch);


#endif /* __BSP_USART_H */
