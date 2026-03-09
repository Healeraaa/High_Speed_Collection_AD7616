#include "App_WaveCollectionTask.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp_fmc.h"
#include "Module_AD7616.h"
#include "bsp_dma.h"
#include "bsp_gpio.h"

#define DMA_BUFFER_SIZE 1024 // 每个缓冲区的采样点数量
/* 双缓冲区定义 - 使用 D3 SRAM */
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint16_t AD7616_DataBuffer_A[DMA_BUFFER_SIZE] = {0};
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint16_t AD7616_DataBuffer_B[DMA_BUFFER_SIZE] = {0};

__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) float AD7616_VoltageBuffer_A[DMA_BUFFER_SIZE] = {0.0f};
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) float AD7616_VoltageBuffer_B[DMA_BUFFER_SIZE] = {0.0f};

/* 计数信号量句柄 */
SemaphoreHandle_t xDMA_BufferReadySemaphore = NULL;

/* 数据队列句柄 (传递电压缓冲区指针) */
QueueHandle_t xVoltageDataQueue = NULL;

/* 当前 DMA 写入缓冲区标志 */
static volatile uint8_t u8_dma_buffer = 0; // 0: Buffer_A, 1: Buffer_B

/* 任务处理缓冲区标志（与 u8_dma_buffer 相反） */
static volatile uint8_t u8_process_buffer = 1; // 初始处理 Buffer_B (等待首次 DMA 完成)

void App_WaveCollectionTask(void *argument)
{
    uint16_t *p_processing_buffer;
    float *p_voltage_buffer; // 指向处理完成的电压缓冲区

    xDMA_BufferReadySemaphore = xSemaphoreCreateCounting(2, 0); // 计数信号量，最大计数 2，初始值 0
    xVoltageDataQueue = xQueueCreate(2, sizeof(float *));       // 队列长度 2，元素大小为 float*（指向电压缓冲区的指针）
    if (xDMA_BufferReadySemaphore == NULL || xVoltageDataQueue == NULL)
    {
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        } // 信号量或队列创建失败，进入死循环
    }
    BSP_GPIO_STM32F411_SYN_Init();     // 初始化与STM32F411的同步GPIO
    Module_AD7616_Config();            // 初始化 AD7616 模块
    Module_AD7616_Set_SampleRate(100); // 设置采样率 100

    /* 启动定时器和 DMA (首次使用 Buffer_A) */
    // BSP_TIM3_PWM0_Start();
    // BSP_DMA_AD7616_Start(AD7616_DataBuffer_A, 1024);

    while (1)
    {
        /* 等待 DMA 缓冲区就绪 */
        if (xSemaphoreTake(xDMA_BufferReadySemaphore, portMAX_DELAY) == pdTRUE)
        {
            /* 选择处理缓冲区（处理上一次 DMA 完成的缓冲区） */
            p_processing_buffer = (u8_process_buffer == 0) ? AD7616_DataBuffer_A : AD7616_DataBuffer_B;
            p_voltage_buffer = (u8_process_buffer == 0) ? AD7616_VoltageBuffer_A : AD7616_VoltageBuffer_B;

            /* ========== 数据处理 ========== */
            for (uint16_t i = 0; i < 1024; i++)
            {
                int16_t raw_data = (int16_t)p_processing_buffer[i];
                p_voltage_buffer[i] = (raw_data / 32768.0f) * 5.0f; // ±5V 量程转换
            }

            /* 发送电压缓冲区指针到队列 (通知 VOFA 上传任务) */
            xQueueSend(xVoltageDataQueue, &p_voltage_buffer, 0); // 不阻塞

            u8_process_buffer = 1 - u8_process_buffer; // 切换到下一个处理缓冲区
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
        u8_dma_buffer = 1 - u8_dma_buffer; // 0 -> 1 或 1 -> 0

        if (u8_dma_buffer == 0)
        {
            BSP_DMA_AD7616_Start(AD7616_DataBuffer_A, 1024);
        }
        else
        {
            BSP_DMA_AD7616_Start(AD7616_DataBuffer_B, 1024);
        }

        /* 释放信号量 (计数 +1) */
        if (xDMA_BufferReadySemaphore != NULL)
        {
            xSemaphoreGiveFromISR(xDMA_BufferReadySemaphore, &xHigherPriorityTaskWoken);
        }

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

__attribute__((section(".itcm"))) void EXTI1_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_1))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_1);

        if (LL_GPIO_IsInputPinSet(GPIOE, LL_GPIO_PIN_1)) // 上升沿
        {
            BSP_TIM3_PWM0_Start();
            BSP_DMA_AD7616_Start(AD7616_DataBuffer_A, DMA_BUFFER_SIZE);
        }
        else // 下降沿
        {
            BSP_TIM3_PWM0_Stop();
        }
    }
}