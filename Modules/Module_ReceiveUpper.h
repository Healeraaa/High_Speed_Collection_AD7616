#ifndef __MODULE_RECEIVEUPPER_H__
#define __MODULE_RECEIVEUPPER_H__

#include "main.h"

#ifndef MODULE_STATUS_T_DEFINED
#define MODULE_STATUS_T_DEFINED
typedef enum {
    Module_OK    = 0x00,
    Module_ERROR = 0x01,
    Module_BUSY  = 0x02
} Module_Status_t;
#endif

/* USART1 接收缓冲区定义 */
#define USART1_RX_BUFFER_SIZE  256
#define USART1_MAX_FLOAT_DATA  16   // 最多支持 16 个浮点数（可扩展）
#define USART1_MAX_INT_DATA    16   // 最多支持 16 个整数（可扩展）

/**
 * @brief  接收数据结构体 (支持可变长度浮点数和整数)
 * 
 * 协议格式: 
 *   "ceiod:%d,%d,%d,%d,%d\n"  - 整数数据（5个）
 *   "ceiof:%f,%f,%f,%f,%f,%f\n" - 浮点数数据（6个）
 * 
 * 其中：
 *   ceiod: 第1个%d为模式（对应 Serial_SendPacket 的第一个参数）
 *          后4个%d对应 data_converter[6].u8_array[0-3]
 *   ceiof: 6个%f为浮点数数据（对应 data_converter[0-5]）
 */
typedef struct {
    uint32_t mode;                           // 模式标识符（对应 Serial_SendPacket 的第一个参数）
    float    float_data[USART1_MAX_FLOAT_DATA];  // 浮点数数据数组
    uint32_t float_count;                    // 实际接收的浮点数个数
    uint32_t int_data[USART1_MAX_INT_DATA];  // 整数数据数组
    uint32_t int_count;                      // 实际接收的整数个数
    uint8_t  is_valid;                       // 数据是否有效
} USART1_RxData_t;

/**
 * @brief  USART1 接收单个字符 (在中断处理函数中调用)
 * @param  ch: 接收到的字节
 * @retval None
 * 
 * @note   当同时接收到完整的 "ceiod:...\\n" 和 "ceiof:...\\n" 时，置位接收完成标志
 */
void Module_USART1_ReceiveChar(uint8_t ch);

/**
 * @brief  判断 USART1 接收缓冲区中是否有完整数据包
 * @param  None
 * @retval 1 - 同时接收到两条完整命令; 0 - 数据不完整或无数据
 * 
 * @note   必须同时包含 "ceiod:...\\n" 和 "ceiof:...\\n" 才返回1
 */
uint8_t Module_USART1_IsDataReady(void);

/**
 * @brief  解析 USART1 接收缓冲区中的数据 
 * 同时支持两种格式（必须同时存在）:
 *   "ceiod:%d,%d,...\\n" - 整数数据（个数不定）
 *   "ceiof:%f,%f,...\\n" - 浮点数数据（个数不定）
 * 
 * 完整协议示例:
 *   "ceiod:1,0,1,2,3\nceiof:2.5,3.5,4.5,5.5,6.5,7.5\n"
 * 
 * @param  pRxData: 指向接收数据结构体的指针
 * @retval Module_OK - 两条命令都解析成功; 
 *         Module_BUSY - 数据不完整; 
 *         Module_ERROR - 解析失败或命令不完整
 * 
 * @note   必须同时解析到两条完整命令才返回 Module_OK
 */
Module_Status_t Module_USART1_ParseData(USART1_RxData_t *pRxData);

#endif 

