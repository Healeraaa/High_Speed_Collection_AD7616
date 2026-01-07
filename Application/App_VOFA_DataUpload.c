#include "App_VOFA_DataUpload.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp.h"
#include "stdio.h"

/* 外部队列句柄 */
extern QueueHandle_t xVoltageDataQueue;

void App_VofaDataUploadTask(void *argument)
{
    float *p_voltage_data = NULL;  // 接收的电压缓冲区指针
    
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
            for (uint16_t i = 0; i < 1024; i++)
            {
                printf("%4.3f\r\n", p_voltage_data[i]);
                vTaskDelay(10);
            }
        }
    }
}