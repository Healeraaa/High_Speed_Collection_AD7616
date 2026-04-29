#ifndef __MODULE_RECEIVEUPPER_H__
#define __MODULE_RECEIVEUPPER_H__

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

/* USART1 接收缓冲区定义 */
#define USART1_RX_BUFFER_SIZE  256
#define USART1_MAX_FLOAT_DATA  16   // 最多支持 16 个浮点数（可扩展）

/**
 * @brief  接收数据结构体 (支持可变长度浮点数)
 * 
 * 协议格式: "ceio:%d,%f,%f,%f,...,%f\n"
 * 其中：
 *   %d = 模式标识符 (mode)
 *   %f = 浮点数数据（支持1~16个）
 */
typedef struct {
    uint32_t mode;                           // 模式标识符（对应 Serial_SendPacket 的第一个参数）
    float    float_data[USART1_MAX_FLOAT_DATA];  // 浮点数数据数组
    uint32_t float_count;                    // 实际接收的浮点数个数
    uint8_t  is_valid;                       // 数据是否有效
} USART1_RxData_t;

/**
 * @brief  USART1 接收单个字符 (在中断处理函数中调用)
 * @param  ch: 接收到的字节
 * @retval None
 */
void Module_USART1_ReceiveChar(uint8_t ch);

/**
 * @brief  判断 USART1 接收缓冲区中是否有完整数据包
 * @param  None
 * @retval 1 - 有完整数据包; 0 - 数据不完整或无数据
 * 
 * @note   快速检查，不进行解析，适合轮询或作为解析前的预检查
 */
uint8_t Module_USART1_IsDataReady(void);

/**
 * @brief  解析 USART1 接收缓冲区中的数据 (格式: "ceio:%d,%f,%f,...\n")
 * @param  pRxData: 指向接收数据结构体的指针
 * @retval BSP_OK - 解析成功; BSP_BUSY - 数据不完整; BSP_ERROR - 解析失败
 * 
 * @note   支持可变长度的浮点数数据（1~16个），自动检测实际数据个数
 */
BSP_Status_t Module_USART1_ParseData(USART1_RxData_t *pRxData);

#endif 

