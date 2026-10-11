#include "Module.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "task.h"
#include "semphr.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

extern USBD_HandleTypeDef hUsbDeviceHS;

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

#define DEVICE_TYPE_IV 0x01
#define DEVICE_TYPE_LIGHT 0x02

/* 最大单帧数据个数（防止超出缓冲） */
#define MAX_DATA_PER_FRAME 2048
#define PROTOCOL_FRAME_OVERHEAD 19U
#define TX_BUFFER_SIZE ((MAX_DATA_PER_FRAME * 4U) + PROTOCOL_FRAME_OVERHEAD)
#define USB_TX_COMPLETE_TIMEOUT_MS 100U

/* 协议帧结构 */
typedef struct
{
  uint8_t header_h;      // 0x55
  uint8_t header_l;      // 0xAA
  uint16_t frame_length; // 整个帧长度（从帧头到帧尾）
  uint8_t device_id;     // 设备类型 (0x01: IV, 0x02: 光)
  uint32_t sample_freq;  // 采样频率 (Hz)
  uint32_t timestamp;    // 时间戳 (ms)
  uint16_t data_count;   // 数据个数
  float *p_data;         // 数据指针 (动态)
  uint16_t crc16;        // CRC16 校验码
  uint8_t footer_h;      // 0x0D
  uint8_t footer_l;      // 0x0A
} Protocol_Frame_t;

/* 发送缓冲：固定字段19字节 + 最多2048个float数据。 */
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint8_t tx_buffer[TX_BUFFER_SIZE];

static SemaphoreHandle_t usb_tx_mutex = NULL;
static SemaphoreHandle_t usb_tx_complete = NULL;
static UpperTx_Stats_t upper_tx_stats = {0};

int32_t Module_TransmitUpper_Init(void)
{
  if (usb_tx_mutex == NULL)
  {
    usb_tx_mutex = xSemaphoreCreateMutex();
  }

  if (usb_tx_complete == NULL)
  {
    usb_tx_complete = xSemaphoreCreateBinary();
  }

  if ((usb_tx_mutex == NULL) || (usb_tx_complete == NULL))
  {
    return -1;
  }

  return 0;
}

void Module_TransmitUpper_NotifyUSBCompleteFromISR(void)
{
  BaseType_t higher_priority_task_woken = pdFALSE;

  if (usb_tx_complete != NULL)
  {
    (void)xSemaphoreGiveFromISR(usb_tx_complete, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
  }
}

void Module_TransmitUpper_GetStats(UpperTx_Stats_t *p_stats)
{
  if (p_stats != NULL)
  {
    *p_stats = upper_tx_stats;
  }
}


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
 * @param device_id     设备类型 (0x01: IV, 0x02: 光)
 * @param sample_freq   采样频率 (Hz)
 * @param p_data        浮点数数组指针
 * @param count         数据个数
 * @param p_frame_len   输出帧长度
 * @return              返回缓冲指针，长度在 p_frame_len 中
 */
uint8_t *Module_TransmitUpper_PackFrame(
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

  /* 帧长度占位（先填充为 0x00，待后续回填） */
  uint16_t frame_len_pos = offset;
  tx_buffer[offset++] = 0x00;
  tx_buffer[offset++] = 0x00;

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

  for (uint16_t i = 0; i < data_bytes; i++)
  {
    tx_buffer[offset + i] = p_float_bytes[i];
  }
  offset += data_bytes;

  /* ============ 关键改动：先计算帧长，再计算 CRC ============
   * 帧结构: [帧头(2)] [帧长(2)] [设备ID(1)] [频率(4)] [时间戳(4)] [数据量(2)] [数据] [CRC(2)] [帧尾(2)]
   */
  
  /* 1. 计算完整帧长度（从帧头到帧尾：2+2+1+4+4+2+数据+2+2） */
  uint16_t total_len = offset + 2 + 2;  // offset 已包含到数据，加上 CRC(2) + 帧尾(2)
  
  /* 2. 填充帧长度 */
  tx_buffer[frame_len_pos] = (uint8_t)(total_len & 0xFF);
  tx_buffer[frame_len_pos + 1] = (uint8_t)((total_len >> 8) & 0xFF);

  /* 3. 计算 CRC16 (从帧头到数据负载末尾，包括已填充的帧长) */
  uint16_t crc16 = CRC16_CCITT(tx_buffer, offset);
  
  /* 4. 填充 CRC16 */
  tx_buffer[offset++] = (uint8_t)(crc16 & 0xFF);
  tx_buffer[offset++] = (uint8_t)((crc16 >> 8) & 0xFF);

  /* 5. 填充帧尾 */
  tx_buffer[offset++] = PROTOCOL_FOOTER_H;
  tx_buffer[offset++] = PROTOCOL_FOOTER_L;

  *p_frame_len = total_len;
  return tx_buffer;
}

/**
 * 发送打包好的帧（兼容旧接口，实际传输介质为USB CDC）
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
  return Module_USB_TransmitUpper_SendFrame(
      device_id, sample_freq, p_data, count);
}

int32_t Module_USB_TransmitUpper_SendFrame(
    uint8_t device_id,
    uint32_t sample_freq,
    const float *p_data,
    uint16_t count)
{
  uint16_t frame_len = 0;
  uint8_t usb_status;
  uint8_t *p_frame;
  USBD_CDC_HandleTypeDef *hcdc;
  int32_t result;

  if ((usb_tx_mutex == NULL) || (usb_tx_complete == NULL))
  {
    return -6;
  }

  /* IV、光数据和测试发送共用此锁，锁覆盖整个USB异步发送周期。 */
  if (xSemaphoreTake(usb_tx_mutex, portMAX_DELAY) != pdTRUE)
  {
    return -6;
  }

  result = -4;

  /* 清除上一次超时后可能残留的完成通知。 */
  (void)xSemaphoreTake(usb_tx_complete, 0U);

  /* USB CDC必须在主机完成枚举后才能发送。 */
  if ((hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED) ||
      (hUsbDeviceHS.pClassData == NULL))
  {
    upper_tx_stats.not_configured_drops++;
    result = -2;
    goto exit;
  }

  hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceHS.pClassData;

  /* 正常情况下锁已保证空闲；超时后的迟到完成在这里等待一次。 */
  if (hcdc->TxState != 0U)
  {
    if (xSemaphoreTake(
            usb_tx_complete,
            pdMS_TO_TICKS(USB_TX_COMPLETE_TIMEOUT_MS)) != pdTRUE)
    {
      upper_tx_stats.busy_failures++;
      result = -3;
      goto exit;
    }

    if ((hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED) ||
        (hUsbDeviceHS.pClassData == NULL))
    {
      upper_tx_stats.not_configured_drops++;
      result = -2;
      goto exit;
    }
  }

  /* 不允许旧发送完成信号被下一帧误消费。 */
  (void)xSemaphoreTake(usb_tx_complete, 0U);

  p_frame = Module_TransmitUpper_PackFrame(
      device_id, sample_freq, p_data, count, &frame_len);

  if ((p_frame == NULL) || (frame_len == 0U))
  {
    upper_tx_stats.pack_failures++;
    result = -1;
    goto exit;
  }

  usb_status = CDC_Transmit_HS(p_frame, frame_len);

  if (usb_status == USBD_OK)
  {
    if (xSemaphoreTake(
            usb_tx_complete,
            pdMS_TO_TICKS(USB_TX_COMPLETE_TIMEOUT_MS)) == pdTRUE)
    {
      upper_tx_stats.sent_frames++;
      result = 0;
    }
    else
    {
      upper_tx_stats.completion_timeouts++;
      result = -5;
    }

    goto exit;
  }

  if (usb_status == USBD_BUSY)
  {
    upper_tx_stats.busy_failures++;
    result = -3;
    goto exit;
  }

  upper_tx_stats.submit_failures++;
  result = -4;

exit:
  (void)xSemaphoreGive(usb_tx_mutex);
  return result;
}

/**
 * 简化接口：发送 IV 数据
 */
int32_t Module_TransmitUpper_SendIVData(
    const float *p_data,
    uint16_t count,
    uint32_t sample_freq)
{
  return Module_USB_TransmitUpper_SendFrame(
      DEVICE_TYPE_IV, sample_freq, p_data, count);
}

/**
 * 简化接口：发送光数据
 */
int32_t Module_TransmitUpper_SendLightData(
    const float *p_data,
    uint16_t count,
    uint32_t sample_freq)
{
  return Module_USB_TransmitUpper_SendFrame(
      DEVICE_TYPE_LIGHT, sample_freq, p_data, count);
}
