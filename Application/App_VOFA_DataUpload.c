#include "App_VOFA_DataUpload.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp.h"
#include "stdio.h"
#include "Module_LightCounting.h"
#include "Module_TransmitUpper.h"
// #include "usb_device.h"
// #include "usbd_cdc_if.h"

/* ==================== 采样配置 ==================== */
/**
 * 采样频率 (Hz) - 根据 AD7616 FMC 访问周期配置
 * 默认 10kHz，如需调整请修改此宏
 *
 * 计算方式：
 * - FMC 时钟 = 200MHz (HCLK)
 * - 假设每次 AD7616 读取耗时 ~100ns (20 个时钟周期)
 * - 最大采样率 ≈ 10MHz，但实际受 FMC 总线仲裁影响
 * - 建议保守设置为 10-100kHz
 */
#define AD7616_SAMPLE_FREQUENCY_HZ 100 // 100Hz
#define Light_SAMPLE_FREQUENCY_HZ 100 // 100Hz

/* 数据包结构体（与 App_WaveCollectionTask.c 一致） */
typedef struct
{
    float *pIVBuffer;
    uint32_t validCount;
} IVData_t;

/* 外部队列句柄 */
extern QueueHandle_t xIVDataQueue;

typedef struct
{
    float *pLightBuffer;
    uint32_t validCount;
} LightData_t;

extern QueueHandle_t xLightDataQueue;

void App_IVDataUploadTask(void *argument)
{
    IVData_t rxData;            // 接收的数据包
    uint16_t frame_len = 0;     // 打包后的帧长度
    uint8_t *p_frame = NULL;    // 打包后的帧缓冲指针
    SemaphoreHandle_t tx_mutex; // USART1 发送互斥锁

    tx_mutex = App_GetUSART1_TxMutex();

    while (1)
    {
        /* 等待接收电压数据包 (阻塞等待) */
        if (xQueueReceive(xIVDataQueue, &rxData, portMAX_DELAY) == pdTRUE)
        {
            float *p_iv_data = rxData.pIVBuffer;
            uint32_t valid_count = rxData.validCount;

            /* 第一步：打包数据帧（不需要加锁，因为使用的是内部缓冲） */
            frame_len = 0;
            p_frame = Module_TransmitUpper_PackFrame(
                DEVICE_TYPE_IV,
                AD7616_SAMPLE_FREQUENCY_HZ,
                p_iv_data,
                (uint16_t)valid_count,
                &frame_len);

            if (p_frame != NULL && frame_len > 0)
            {
                /* 第二步：加互斥锁后发送数据 (死等) */
                xSemaphoreTake(tx_mutex, portMAX_DELAY);
                /* 临界区：发送缓冲数据 */
                Module_TransmitUpper_SendBuffer(p_frame, frame_len);
                /* 释放互斥锁 */
                xSemaphoreGive(tx_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

void App_LightDataUploadTask(void *argument)
{
    LightData_t rxData;            // 接收的数据包
    uint16_t frame_len = 0;     // 打包后的帧长度
    uint8_t *p_frame = NULL;    // 打包后的帧缓冲指针
    SemaphoreHandle_t tx_mutex; // USART1 发送互斥锁

    tx_mutex = App_GetUSART1_TxMutex();

    while (1)
    {
        /* 等待接收光数据包 (阻塞等待) */
        if (xQueueReceive(xLightDataQueue, &rxData, portMAX_DELAY) == pdTRUE)
        {
            float *p_iv_data = rxData.pLightBuffer;
            uint32_t valid_count = rxData.validCount;

            /* 第一步：打包数据帧（不需要加锁，因为使用的是内部缓冲） */
            frame_len = 0;
            p_frame = Module_TransmitUpper_PackFrame(
                DEVICE_TYPE_LIGHT,
                Light_SAMPLE_FREQUENCY_HZ,
                p_iv_data,
                (uint16_t)valid_count,
                &frame_len);

            if (p_frame != NULL && frame_len > 0)
            {
                /* 第二步：加互斥锁后发送数据 (死等) */
                xSemaphoreTake(tx_mutex, portMAX_DELAY);
                /* 临界区：发送缓冲数据 */
                Module_TransmitUpper_SendBuffer(p_frame, frame_len);
                /* 释放互斥锁 */
                xSemaphoreGive(tx_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

// /* 正弦波参数 */
// #define SINE_AMPLITUDE  5.0f    // 振幅
// #define SINE_OFFSET     0.0f    // 偏移
// #define SAMPLE_RATE     100     // 采样率 (Hz)

// /* 四个通道的频率 (Hz) */
// #define CH0_FREQ        1.0f    // 通道0: 1Hz
// #define CH1_FREQ        2.0f    // 通道1: 2Hz
// #define CH2_FREQ        5.0f    // 通道2: 5Hz
// #define CH3_FREQ        10.0f   // 通道3: 10Hz

// #define PI              3.14159265358979f

// void App_VofaDataUploadTask(void *argument)
// {
//     float ch0_value, ch1_value, ch2_value, ch3_value;
//     float phase0 = 0.0f, phase1 = 0.0f, phase2 = 0.0f, phase3 = 0.0f;
//     float delta_phase0, delta_phase1, delta_phase2, delta_phase3;
//     char tx_buffer[128];
//     uint16_t len;

//     MX_USB_DEVICE_Init();

//     /* 计算每个采样周期的相位增量 */
//     delta_phase0 = (2.0f * PI * CH0_FREQ) / SAMPLE_RATE;
//     delta_phase1 = (2.0f * PI * CH1_FREQ) / SAMPLE_RATE;
//     delta_phase2 = (2.0f * PI * CH2_FREQ) / SAMPLE_RATE;
//     delta_phase3 = (2.0f * PI * CH3_FREQ) / SAMPLE_RATE;

//     /* 等待 USB 枚举完成 */
//     vTaskDelay(pdMS_TO_TICKS(1000));

//     while (1)
//     {
//         /* 计算四个通道的正弦波值 */
//         ch0_value = SINE_AMPLITUDE * sinf(phase0) + SINE_OFFSET;
//         ch1_value = SINE_AMPLITUDE * sinf(phase1) + SINE_OFFSET;
//         ch2_value = SINE_AMPLITUDE * sinf(phase2) + SINE_OFFSET;
//         ch3_value = SINE_AMPLITUDE * sinf(phase3) + SINE_OFFSET;

//         /* 更新各通道相位 */
//         phase0 += delta_phase0;
//         phase1 += delta_phase1;
//         phase2 += delta_phase2;
//         phase3 += delta_phase3;

//         /* 相位归一化 (防止溢出) */
//         if (phase0 >= 2.0f * PI) phase0 -= 2.0f * PI;
//         if (phase1 >= 2.0f * PI) phase1 -= 2.0f * PI;
//         if (phase2 >= 2.0f * PI) phase2 -= 2.0f * PI;
//         if (phase3 >= 2.0f * PI) phase3 -= 2.0f * PI;

//         /* 按照 FireWater 协议格式化: "ceio:ch0,ch1,ch2,ch3\n" */
//         len = sprintf(tx_buffer, "ceio:%.3f,%.3f,%.3f,%.3f\n",
//                       ch0_value, ch1_value, ch2_value, ch3_value);

//         CDC_Transmit_HS((uint8_t *)tx_buffer, len);

//         /* 控制发送频率 (10ms 一次，100Hz 采样率) */
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }
