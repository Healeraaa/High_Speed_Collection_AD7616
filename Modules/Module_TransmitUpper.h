#ifndef __MODULE_TRANSMITUPPER_H__
#define __MODULE_TRANSMITUPPER_H__

#include "main.h"

/* ==================== 设备类型定义 ==================== */
#define DEVICE_TYPE_VOLTAGE     0x01  /*!< 电压数据 */
#define DEVICE_TYPE_CURRENT     0x02  /*!< 电流数据 */

/**
 * 发送数据帧 (通用接口)
 * 
 * @param device_id     设备类型 (0x01: 电压, 0x02: 电流)
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
 * 发送电压数据 (简化接口)
 * 
 * @param p_data        电压数据指针 (float 数组)
 * @param count         数据个数
 * @param sample_freq   采样频率 (Hz)
 * @return              0: 成功, -1: 失败
 */
int32_t Module_TransmitUpper_SendVoltageData(
    const float *p_data,
    uint16_t count,
    uint32_t sample_freq);

/**
 * 发送电流数据 (简化接口)
 * 
 * @param p_data        电流数据指针 (float 数组)
 * @param count         数据个数
 * @param sample_freq   采样频率 (Hz)
 * @return              0: 成功, -1: 失败
 */
int32_t Module_TransmitUpper_SendCurrentData(
    const float *p_data,
    uint16_t count,
    uint32_t sample_freq);

#endif 

