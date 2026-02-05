#include "App_VOFA_DataUpload.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp.h"
#include "stdio.h"
// #include "usb_device.h"
// #include "usbd_cdc_if.h"

/* 外部队列句柄 */
extern QueueHandle_t xVoltageDataQueue;

void App_VofaDataUploadTask(void *argument)
{
    float *p_voltage_data = NULL;  // 接收的电压缓冲区指针
    // MX_USB_DEVICE_Init();

    
    while (1)
    {
        /* 等待接收电压数据指针 (阻塞等待) */
        if (xQueueReceive(xVoltageDataQueue, &p_voltage_data, portMAX_DELAY) == pdTRUE)
        {
            /* ========== 发送数据到 VOFA+ ========== */
            // 示例: 发送前 3 个通道数据 (可根据实际需求调整)
            // printf("%4.3f,%4.3f,%4.3f\r\n", 
            //        p_voltage_data[0],   // 通道 0
            //        p_voltage_data[1],   // 通道 1
            //        p_voltage_data[2]);  // 通道 2
            
            /* 或发送所有 1024 个采样点 (需 VOFA+ 配置对应通道数) */
            for (uint16_t i = 0; i < 512; i++)                                     
            {
                printf("%4.3f,%4.3f\r\n", p_voltage_data[2*i],p_voltage_data[2*i+1]);
                // CDC_Transmit_HS((uint8_t *)"Module Config OK!\r\n", strlen("Module Config OK!\r\n"));

                // printf("%4.3f\r\n", p_voltage_data[2*i]);
                // printf("%4.3f\r\n", p_voltage_data[2*i+1]);
                // printf("HAHAHHAHAHAHHAHAHAH:%4.3f\r\n", p_voltage_data[2*i]);
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