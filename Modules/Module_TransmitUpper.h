#ifndef __MODULE_TRANSMITUPPER_H__
#define __MODULE_TRANSMITUPPER_H__

#include "main.h"

/* ==================== 设备类型定义 ==================== */
#define DEVICE_TYPE_IV          0x01  /*!< IV 数据 */
#define DEVICE_TYPE_LIGHT       0x02  /*!< 光数据 */

/**
 * 将原始数据打包成协议帧
 * 此函数将数据打包到内部缓冲区，必须立即跟随 Module_TransmitUpper_SendBuffer() 调用
 *
 * @param device_id     设备类型 (0x01: IV, 0x02: 光)
 * @param sample_freq   采样频率 (Hz)
 * @param p_data        浮点数数组指针
 * @param count         数据个数 (最多 2048 个)
 * @param p_frame_len   输出帧长度
 * @return              返回缓冲指针（内部静态缓冲），长度在 p_frame_len 中。失败返回 NULL
 *
 * @note 返回的缓冲指针为静态缓冲，必须在下一次调用 PackFrame 前完成发送
 */
uint8_t *Module_TransmitUpper_PackFrame(
    uint8_t device_id,
    uint32_t sample_freq,
    const float *p_data,
    uint16_t count,
    uint16_t *p_frame_len);

/**
 * 通过 USART1 逐字节发送缓冲数据
 *
 * @param p_buffer  数据缓冲指针
 * @param length    数据长度
 * @return          无
 */
void Module_TransmitUpper_SendBuffer(const uint8_t *p_buffer, uint16_t length);

/**
 * 发送数据帧 (通用接口)
 * 
 * @param device_id     设备类型 (0x01: IV, 0x02: 光)
 * @param sample_freq   采样频率 (Hz)
 * @param p_data        浮点数数组指针
 * @param count         数据个数 (最多 2048 个)
 * @return              0: 成功, -1: 失败
 * 
 * @note 协议格式: 0x55 0xAA | FrameLen(2) | DeviceID(1) | SampleFreq(4) | 
 *                  Timestamp(4) | Count(2) | FloatData(4*N) | CRC16(2) | 0x0D 0x0A
 */
int32_t Module_TransmitUpper_SendFrame(
    uint8_t device_id,
    uint32_t sample_freq,
    const float *p_data,
    uint16_t count);

/**
 * 发送 IV 数据 (简化接口)
 * 
 * @param p_data        IV 数据指针 (float 数组)
 * @param count         数据个数
 * @param sample_freq   采样频率 (Hz)
 * @return              0: 成功, -1: 失败
 */
int32_t Module_TransmitUpper_SendIVData(
    const float *p_data,
    uint16_t count,
    uint32_t sample_freq);

/**
 * 发送光数据 (简化接口)
 * 
 * @param p_data        光数据指针 (float 数组)
 * @param count         数据个数
 * @param sample_freq   采样频率 (Hz)
 * @return              0: 成功, -1: 失败
 */
int32_t Module_TransmitUpper_SendLightData(
    const float *p_data,
    uint16_t count,
    uint32_t sample_freq);

#endif 

