#ifndef __MODULE_SERIAL411_H__
#define __MODULE_SERIAL411_H__

#include "stdint.h"

#ifndef MODULE_STATUS_T_DEFINED
#define MODULE_STATUS_T_DEFINED
typedef enum
{
  Module_OK       = 0x00,
  Module_ERROR    = 0x01,
  Module_BUSY     = 0x02,
  Module_TIMEOUT  = 0x03
} Module_Status_t;
#endif

// 协议定义
#define SERIAL411_PACKET_HEADER    0xFF
#define SERIAL411_PACKET_TAIL      0xFE
#define SERIAL411_DATA_LENGTH      10      // 数据长度（double）
#define SERIAL411_TIMEOUT_MS       100

// 数据转换联合体
typedef union {
    double      double_val;                 
    uint8_t     u8_array[sizeof(double)];   
} Serial411_DoubleConverter_t;

typedef struct {
    uint8_t hrader;                  // 包头
    uint8_t tail;                    // 包尾
    uint8_t command;                     // 命令字
    Serial411_DoubleConverter_t write_buffer[SERIAL411_DATA_LENGTH];        // 写缓冲区
} Serial411_Packet_t;

Module_Status_t Serial_SendPacket(uint8_t command, double *data);

#endif 

