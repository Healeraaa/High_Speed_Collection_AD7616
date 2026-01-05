#include "App_WaveCollectionTask.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp_fmc.h"
#include "Module_AD7616.h"
#include "bsp_dma.h"

/* 双缓冲区定义 - 使用 D2 SRAM1 */
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint16_t AD7616_DataBuffer_A[1024] = {0};
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint16_t AD7616_DataBuffer_B[1024] = {0};

float AD7616_VoltageBuffer_A[1024] = {0.0f};
float AD7616_VoltageBuffer_B[1024] = {0.0f};

/* 计数信号量句柄 */
SemaphoreHandle_t xDMA_BufferReadySemaphore = NULL;

/* 当前 DMA 写入缓冲区标志 */
static volatile uint8_t u8_dma_buffer = 0;      // 0: Buffer_A, 1: Buffer_B

/* 任务处理缓冲区标志（与 u8_dma_buffer 相反） */
static volatile uint8_t u8_process_buffer = 1;  // 初始处理 Buffer_B (等待首次 DMA 完成)

void App_WaveCollectionTask(void *argument)
{
    uint16_t *p_processing_buffer;
    
    /* 创建计数信号量 (最大计数 2, 初始计数 0) */
    xDMA_BufferReadySemaphore = xSemaphoreCreateCounting(2, 0);
    
    if (xDMA_BufferReadySemaphore == NULL)
    {
        /* 信号量创建失败 */
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }
    
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
            /* ========== 数据处理 ========== */
            if (u8_process_buffer == 0)
            {
                for(uint16_t i = 0; i < 1024; i++)
                {
                    int16_t raw_data = (int16_t)p_processing_buffer[i];
                    AD7616_VoltageBuffer_A[i] = (raw_data / 32768.0f) * 5.0f;  // ±5V 量程
                }
            }
            else
            {
                for(uint16_t i = 0; i < 1024; i++)
                {
                    int16_t raw_data = (int16_t)p_processing_buffer[i];
                    AD7616_VoltageBuffer_B[i] = (raw_data / 32768.0f) * 5.0f;  // ±5V 量程
                }
}
            /* =============================== */
            
            /* 切换到下一个处理缓冲区 */
            u8_process_buffer = 1 - u8_process_buffer;
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