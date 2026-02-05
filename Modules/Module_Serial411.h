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

/* WE通道定义*/
typedef enum {
    WE_CHANNEL_4 = 0x00,  // PB4=0, PB3=0
    WE_CHANNEL_3 = 0x01,  // PB4=0, PB3=1
    WE_CHANNEL_1 = 0x02,  // PB4=1, PB3=0
    WE_CHANNEL_2 = 0x03   // PB4=1, PB3=1
} WE_Channel_TypeDef;

/* IV转换倍数定义*/
typedef enum {
    IV_GAIN_1K   = 0x00,  // PB6=0, PB5=0, 1K
    IV_GAIN_100K = 0x01,  // PB6=0, PB5=1, 100K
    IV_GAIN_10M  = 0x02,  // PB6=1, PB5=0, 10M
    IV_GAIN_100M = 0x03   // PB6=1, PB5=1, 100M
} IV_Gain_TypeDef;

/* 第一级电压放大倍数定义*/
typedef enum {
    VOLTAGE_GAIN_STAGE1_5X  = 0x00,  // PB7=0, 5倍
    VOLTAGE_GAIN_STAGE1_20X = 0x01   // PB7=1, 20倍
} Voltage_Gain_Stage1_TypeDef;

/* 第二级电压放大倍数定义*/
typedef enum {
    VOLTAGE_GAIN_STAGE2_1X  = 0x00,  // PB9=0, PB8=0, 1倍
    VOLTAGE_GAIN_STAGE2_5X  = 0x01,  // PB9=0, PB8=1, 5倍
    VOLTAGE_GAIN_STAGE2_20X = 0x02,  // PB9=1, PB8=0, 20倍
    VOLTAGE_GAIN_STAGE2_50X = 0x03   // PB9=1, PB8=1, 50倍
} Voltage_Gain_Stage2_TypeDef;

/* 反馈选择定义*/
typedef enum {
    FEEDBACK_GND = 0x00,  // 选择GND
    FEEDBACK_FB  = 0x01   // 选择FB
} Feedback_Select_TypeDef;
/* DAC通道定义 */
typedef enum {
    DAC_CH_A   = 0x00,  // 通道A（D23-D20=0000）
    DAC_CH_B   = 0x01,  // 通道B（D23-D20=0001）
    DAC_CH_C   = 0x02,  // 通道C（D23-D20=0010）
    DAC_CH_D   = 0x03,  // 通道D（D23-D20=0011）
    DAC_CH_ALL = 0x0F   // 所有通道（D23-D20=1111）
} DAC_Channel_TypeDef;


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

