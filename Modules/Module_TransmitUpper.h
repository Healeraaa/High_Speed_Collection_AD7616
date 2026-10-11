#ifndef __MODULE_TRANSMITUPPER_H__
#define __MODULE_TRANSMITUPPER_H__

#include "main.h"

/* ==================== 设备类型定义 ==================== */
#define DEVICE_TYPE_IV          0x01  /*!< IV 数据 */
#define DEVICE_TYPE_LIGHT       0x02  /*!< 光数据 */

typedef struct
{
    uint32_t sent_frames;
    uint32_t not_configured_drops;
    uint32_t busy_failures;
    uint32_t submit_failures;
    uint32_t completion_timeouts;
    uint32_t pack_failures;
} UpperTx_Stats_t;

/** 在创建上传任务前初始化USB发送互斥锁和完成信号量。 */
int32_t Module_TransmitUpper_Init(void);

/** USB CDC发送完成回调使用；只能由USB中断调用链调用。 */
void Module_TransmitUpper_NotifyUSBCompleteFromISR(void);

/** 读取USB上传统计信息。 */
void Module_TransmitUpper_GetStats(UpperTx_Stats_t *p_stats);

/**
 * 将原始数据打包成协议帧
 * 此函数将数据打包到内部缓冲区，缓冲区由USB发送互斥锁保护
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
 * 发送数据帧（兼容旧函数名，实际通过USB CDC发送）
 * 
 * @param device_id     设备类型 (0x01: IV, 0x02: 光)
 * @param sample_freq   采样频率 (Hz)
 * @param p_data        浮点数数组指针
 * @param count         数据个数 (最多 2048 个)
 * @return              0: 成功, 负数: 失败
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
 * 通过USB CDC发送数据帧（等待互斥锁及本帧发送完成）
 *
 * @return 0: 整帧发送完成, -1: 打包失败, -2: USB未配置,
 *         -3: USB忙, -4: USB提交失败, -5: 发送完成超时,
 *         -6: 同步对象未初始化
 */
int32_t Module_USB_TransmitUpper_SendFrame(
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
 * @return              0: 成功, 负数: 失败
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
 * @return              0: 成功, 负数: 失败
 */
int32_t Module_TransmitUpper_SendLightData(
    const float *p_data,
    uint16_t count,
    uint32_t sample_freq);

#endif 

