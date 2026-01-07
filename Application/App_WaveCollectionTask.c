#include "App_WaveCollectionTask.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp_fmc.h"
#include "Module_AD7616.h"
#include "bsp_dma.h"

/* 双缓冲区定义 - 使用 D3 SRAM */
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint16_t AD7616_DataBuffer_A[1024] = {0};
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint16_t AD7616_DataBuffer_B[1024] = {0};

__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) float AD7616_VoltageBuffer_A[1024] = {0.0f};
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) float AD7616_VoltageBuffer_B[1024] = {0.0f};

/* 计数信号量句柄 */
SemaphoreHandle_t xDMA_BufferReadySemaphore = NULL;

/* 数据队列句柄 (传递电压缓冲区指针) */
QueueHandle_t xVoltageDataQueue = NULL;

/* 当前 DMA 写入缓冲区标志 */
static volatile uint8_t u8_dma_buffer = 0;      // 0: Buffer_A, 1: Buffer_B

/* 任务处理缓冲区标志（与 u8_dma_buffer 相反） */
static volatile uint8_t u8_process_buffer = 1;  // 初始处理 Buffer_B (等待首次 DMA 完成)

void App_WaveCollectionTask(void *argument)
{
    uint16_t *p_processing_buffer;
    float *p_voltage_buffer;  // 指向处理完成的电压缓冲区
    
    /* 创建计数信号量 (最大计数 2, 初始计数 0) */
    xDMA_BufferReadySemaphore = xSemaphoreCreateCounting(2, 0);
    
    /* 创建消息队列 (队列长度 2, 存储 float* 指针) */
    xVoltageDataQueue = xQueueCreate(2, sizeof(float*));
     
    if (xDMA_BufferReadySemaphore == NULL || xVoltageDataQueue == NULL)
    {
        /* 信号量/队列创建失败 */
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    
    Module_AD7616_Set_SampleRate(100);  // 设置采样率 100
    
    /* 启动定时器和 DMA (首次使用 Buffer_A) */
    BSP_TIM3_PWM0_Start();
    BSP_DMA_TIM3_Start(AD7616_DataBuffer_A, 1024);
    
    while (1)
    {
        /* 等待 DMA 缓冲区就绪 */
        if (xSemaphoreTake(xDMA_BufferReadySemaphore, portMAX_DELAY) == pdTRUE)
        {
            /* 选择处理缓冲区（处理上一次 DMA 完成的缓冲区） */
            p_processing_buffer = (u8_process_buffer == 0) ? AD7616_DataBuffer_A : AD7616_DataBuffer_B;
            p_voltage_buffer = (u8_process_buffer == 0) ? AD7616_VoltageBuffer_A : AD7616_VoltageBuffer_B;
            
            /* ========== 数据处理 ========== */
            for(uint16_t i = 0; i < 1024; i++)
            {
                int16_t raw_data = (int16_t)p_processing_buffer[i];
                p_voltage_buffer[i] = (raw_data / 32768.0f) * 5.0f;  // ±5V 量程转换
            }
            
            /* 发送电压缓冲区指针到队列 (通知 VOFA 上传任务) */
            xQueueSend(xVoltageDataQueue, &p_voltage_buffer, 0);  // 不阻塞
            
            u8_process_buffer = 1 - u8_process_buffer;  // 切换到下一个处理缓冲区
        }
    }
}

void DMA1_Stream0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
     
    if (LL_DMA_IsActiveFlag_TC0(DMA1)) 
    {
        LL_DMA_ClearFlag_TC0(DMA1);
        
        /* 切换 DMA 缓冲区并重启 */
        u8_dma_buffer = 1 - u8_dma_buffer;  // 0 -> 1 或 1 -> 0
        
        if (u8_dma_buffer == 0)
        {
            BSP_DMA_TIM3_Start(AD7616_DataBuffer_A, 1024);
        }
        else
        {
            BSP_DMA_TIM3_Start(AD7616_DataBuffer_B, 1024);
        }
        
        /* 释放信号量 (计数 +1) */
        if (xDMA_BufferReadySemaphore != NULL)
        {
            xSemaphoreGiveFromISR(xDMA_BufferReadySemaphore, &xHigherPriorityTaskWoken);
        }
        
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}