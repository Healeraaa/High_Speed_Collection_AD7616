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
#define AD7616_SAMPLE_FREQUENCY_HZ    10000  // 10 kHz

/* 数据包结构体（与 App_WaveCollectionTask.c 一致） */
typedef struct
{
    float *pIVBuffer;
    uint32_t validCount;
} IVData_t;

/* 外部队列句柄 */
extern QueueHandle_t xIVDataQueue;

void App_VofaDataUploadTask(void *argument)
{
    IVData_t rxData;  // 接收的数据包

    while (1)
    {
        /* 等待接收电压数据包 (阻塞等待) */
        if (xQueueReceive(xIVDataQueue, &rxData, portMAX_DELAY) == pdTRUE)
        {
            float    *p_iv_data = rxData.pIVBuffer;
            uint32_t  valid_count = rxData.validCount;

            /* ========== 严格的数据验证 ========== */
            if (p_iv_data == NULL)
            {
                printf("ERROR: Null pointer in IVData\r\n");
                continue;
            }

            if (valid_count == 0)
            {
                printf("ERROR: Empty data package\r\n");
                continue;
            }

            /* 防止超过单帧最大容量 (2048 浮点数 = 8192 字节) */
            #define MAX_FRAME_DATA 2048
            if (valid_count > MAX_FRAME_DATA)
            {
                printf("ERROR: Data count (%lu) exceeds max frame size (%d)\r\n", 
                       valid_count, MAX_FRAME_DATA);
                continue;
            }

            /* ========== 使用二进制协议发送数据 ========== */
            /**
             * 协议帧结构:
             * 帧头(2) | 帧长(2) | 设备ID(1) | 采样频率(4) | 时间戳(4) | 
             * 数据量(2) | 数据(4*N) | CRC16(2) | 帧尾(2)
             * 
             * 这样做的优势:
             * 1. 一次性发送完整帧，避免逐行发送导致的丢包
             * 2. 帧长度字段解决大包传输问题
             * 3. CRC16 校验确保数据完整性
             * 4. 时间戳用于数据同步和对齐
             * 5. 采样频率用于上位机重建原始时间序列
             */
            int32_t ret = Module_TransmitUpper_SendVoltageData(
                p_iv_data,
                (uint16_t)valid_count,
                AD7616_SAMPLE_FREQUENCY_HZ);

            if (ret != 0)
            {
                /* 发送失败处理 - 可能是缓冲区溢出或其他错误 */
                printf("ERROR: Failed to send frame (count=%lu, ret=%ld)\r\n", 
                       valid_count, ret);
            }

            /* 发送延时 - 避免 CPU 饱和 */
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
