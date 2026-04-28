#include "App_VOFA_DataUpload.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp.h"
#include "stdio.h"
#include "Module_LightCounting.h"
// #include "usb_device.h"
// #include "usbd_cdc_if.h"

/* 数据包结构体（与 App_WaveCollectionTask.c 一致） */
typedef struct {
    float    *pBuffer;      // 电压缓冲区指针
    uint32_t  validCount;   // 有效数据数量
} VoltageData_t;

/* 外部队列句柄 */
extern QueueHandle_t xVoltageDataQueue;

void App_VofaDataUploadTask(void *argument)
{
    VoltageData_t rxData;  // 接收的数据包
    // MX_USB_DEVICE_Init();

    while (1)
    {
        /* 等待接收电压数据包 (阻塞等待) */
        if (xQueueReceive(xVoltageDataQueue, &rxData, portMAX_DELAY) == pdTRUE)
        {
            float    *p_voltage_data = rxData.pBuffer;
            uint32_t  valid_count    = rxData.validCount;

            /* ========== 发送数据到 VOFA+ ========== */
            // 根据有效数据量发送（每次发送 2 个通道）
            uint32_t pairs = valid_count / 2;
            for (uint32_t i = 0; i < pairs; i++)
            {
                printf("%4.3f,%4.3f,%d\r\n", p_voltage_data[2*i], p_voltage_data[2*i+1],Module_LightCounting_GetAndClearCount());
                vTaskDelay(10);
            }
            
            // 如果有奇数个数据，单独发送最后一个
            if (valid_count % 2 != 0)
            {
                printf("%4.3f\r\n", p_voltage_data[valid_count - 1]);
                vTaskDelay(10);
            }
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
