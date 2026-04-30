#include "Module.h"
#include "bsp.h"
#include "stdio.h"
#include "string.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"

/* ==================== 通信协议定义 ==================== */

/**
 * 自定义二进制协议帧格式：
 * 帧头(2) | 帧长(2) | 设备ID(1) | 采样频率(4) | 时间戳(4) | 数据量(2) | 数据负载(4*N) | CRC16(2) | 帧尾(2)
 * 0x55 0xAA | FrameLen | DeviceID | Fs | Timestamp | Count | Data | CRC | 0x0D 0x0A
 */

#define PROTOCOL_HEADER_H 0x55
#define PROTOCOL_HEADER_L 0xAA
#define PROTOCOL_FOOTER_H 0x0D
#define PROTOCOL_FOOTER_L 0x0A

#define DEVICE_TYPE_VOLTAGE 0x01
#define DEVICE_TYPE_CURRENT 0x02

/* 最大单帧数据个数（防止超出缓冲） */
#define MAX_DATA_PER_FRAME 2048

/* 协议帧结构 */
typedef struct
{
  uint8_t header_h;      // 0x55
  uint8_t header_l;      // 0xAA
  uint16_t frame_length; // 整个帧长度（从帧头到帧尾）
  uint8_t device_id;     // 设备类型 (0x01: 电压, 0x02: 电流)
  uint32_t sample_freq;  // 采样频率 (Hz)
  uint32_t timestamp;    // 时间戳 (ms)
  uint16_t data_count;   // 数据个数
  float *p_data;         // 数据指针 (动态)
  uint16_t crc16;        // CRC16 校验码
  uint8_t footer_h;      // 0x0D
  uint8_t footer_l;      // 0x0A
} Protocol_Frame_t;

/* 发送缓冲 - 足够容纳最大帧: 19 + 4*1024 + 4 = 4120 字节 */
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint8_t tx_buffer[1024 * 8]; 

/* ==================== CRC16 计算 ==================== */
/**
 * CRC16-CCITT 实现 (多项式: 0x1021)
 * 初始值: 0xFFFF
 */
static uint16_t CRC16_CCITT(const uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xFFFF;

  for (uint16_t i = 0; i < length; i++)
  {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t j = 0; j < 8; j++)
    {
      if (crc & 0x8000)
        crc = (crc << 1) ^ 0x1021;
      else
        crc = crc << 1;
    }
  }

  return crc;
}

/* ==================== 打包和发送函数 ==================== */
/**
 * 将原始数据打包成协议帧
 *
 * @param device_id     设备类型 (0x01: 电压, 0x02: 电流)
 * @param sample_freq   采样频率 (Hz)
 * @param p_data        浮点数数组指针
 * @param count         数据个数
 * @param p_frame_len   输出帧长度
 * @return              返回缓冲指针，长度在 p_frame_len 中
 */
static uint8_t *Module_TransmitUpper_PackFrame(
    uint8_t device_id,
    uint32_t sample_freq,
    const float *p_data,
    uint16_t count,
    uint16_t *p_frame_len)
{
  /* *** 输入验证 *** */
  if (!p_data || !p_frame_len)
    return NULL;

  if (count == 0 || count > MAX_DATA_PER_FRAME)
    return NULL;

  /* 检查采样频率是否合理 */
  if (sample_freq == 0)
    return NULL;

  uint16_t offset = 0;

  /* 帧头 */
  tx_buffer[offset++] = PROTOCOL_HEADER_H;
  tx_buffer[offset++] = PROTOCOL_HEADER_L;

  /* 帧长度占位（后续填充） */
  uint16_t frame_len_pos = offset;
  offset += 2;

  /* 设备ID */
  tx_buffer[offset++] = device_id;

  /* 采样频率 (小端序) */
  tx_buffer[offset++] = (uint8_t)(sample_freq & 0xFF);
  tx_buffer[offset++] = (uint8_t)((sample_freq >> 8) & 0xFF);
  tx_buffer[offset++] = (uint8_t)((sample_freq >> 16) & 0xFF);
  tx_buffer[offset++] = (uint8_t)((sample_freq >> 24) & 0xFF);

  /* 时间戳 (系统时间，小端序) */
  uint32_t timestamp = osKernelGetTickCount();
  tx_buffer[offset++] = (uint8_t)(timestamp & 0xFF);
  tx_buffer[offset++] = (uint8_t)((timestamp >> 8) & 0xFF);
  tx_buffer[offset++] = (uint8_t)((timestamp >> 16) & 0xFF);
  tx_buffer[offset++] = (uint8_t)((timestamp >> 24) & 0xFF);

  /* 数据个数 (小端序) */
  tx_buffer[offset++] = (uint8_t)(count & 0xFF);
  tx_buffer[offset++] = (uint8_t)((count >> 8) & 0xFF);

  /* 数据负载 (浮点数，IEEE 754 格式) */
  uint8_t *p_float_bytes = (uint8_t *)p_data;
  uint16_t data_bytes = count * 4; // 每个 float 4 个字节


  // memcpy(&tx_buffer[offset], p_float_bytes, data_bytes);
  for (uint16_t i = 0; i < data_bytes; i++)
  {
    tx_buffer[offset + i] = p_float_bytes[i];
  }
  offset += data_bytes;

  /* 计算 CRC16 (从帧头到数据负载) */
  uint16_t crc16 = CRC16_CCITT(tx_buffer, offset);
  tx_buffer[offset++] = (uint8_t)(crc16 & 0xFF);
  tx_buffer[offset++] = (uint8_t)((crc16 >> 8) & 0xFF);

  /* 帧尾 */
  tx_buffer[offset++] = PROTOCOL_FOOTER_H;
  tx_buffer[offset++] = PROTOCOL_FOOTER_L;

  /* 回填帧长度 (从帧头到帧尾) */
  uint16_t total_len = offset;
  tx_buffer[frame_len_pos] = (uint8_t)(total_len & 0xFF);
  tx_buffer[frame_len_pos + 1] = (uint8_t)((total_len >> 8) & 0xFF);

  *p_frame_len = total_len;
  return tx_buffer;
}

/**
 * 通过 USART 发送打包好的帧
 *
 * @param device_id     设备类型
 * @param sample_freq   采样频率
 * @param p_data        数据指针
 * @param count         数据个数
 * @return              0: 成功, 非0: 失败
 */
int32_t Module_TransmitUpper_SendFrame(
    uint8_t device_id,
    uint32_t sample_freq,
    const float *p_data,
    uint16_t count)
{
  uint16_t frame_len = 0;
  uint8_t *p_frame = Module_TransmitUpper_PackFrame(
      device_id, sample_freq, p_data, count, &frame_len);

  if (!p_frame || frame_len == 0)
    return -1;

  /* 通过 USART1 发送 (通过 printf 重定向到 USART1) */
  for (uint16_t i = 0; i < frame_len; i++)
  {
    /* 可选: 如果有底层发送接口，使用它 */
    BSP_USART1_SendByte(p_frame[i]);
  }

  /* 使用 fwrite 发送到标准输出 (需要重定向) */
  // fwrite(p_frame, 1, frame_len, stdout);
  // fflush(stdout);

  return 0;
}

/**
 * 简化接口：发送电压数据
 */
int32_t Module_TransmitUpper_SendVoltageData(
    const float *p_data,
    uint16_t count,
    uint32_t sample_freq)
{
  return Module_TransmitUpper_SendFrame(
      DEVICE_TYPE_VOLTAGE, sample_freq, p_data, count);
}

/**
 * 简化接口：发送电流数据
 */
int32_t Module_TransmitUpper_SendCurrentData(
    const float *p_data,
    uint16_t count,
    uint32_t sample_freq)
{
  return Module_TransmitUpper_SendFrame(
      DEVICE_TYPE_CURRENT, sample_freq, p_data, count);
}
