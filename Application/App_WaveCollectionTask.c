#include "App_WaveCollectionTask.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp_fmc.h"
#include "Module_AD7616.h"
#include "bsp_dma.h"
#include "bsp_gpio.h"

#define DMA_BUFFER_SIZE 1024

__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint16_t AD7616_DataBuffer_A[DMA_BUFFER_SIZE] = {0};
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) uint16_t AD7616_DataBuffer_B[DMA_BUFFER_SIZE] = {0};

__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) float AD7616_VoltageBuffer_A[DMA_BUFFER_SIZE] = {0.0f};
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) float AD7616_VoltageBuffer_B[DMA_BUFFER_SIZE] = {0.0f};

/* 原始数据信息（ISR → 处理任务）*/
typedef struct
{
    uint8_t  bufferIndex;
    uint32_t validCount;
    uint8_t  isLastPacket;  /* 是否为本轮最后一包 */
} RawDataInfo_t;

/* 电压数据包（处理任务 → 上传任务）*/
typedef struct
{
    float   *pBuffer;
    uint32_t validCount;
} VoltageData_t;

QueueHandle_t xRawDataQueue = NULL;
QueueHandle_t xVoltageDataQueue = NULL;

static volatile uint8_t u8_dma_buffer = 0;

void App_WaveCollectionTask(void *argument)
{
    RawDataInfo_t rxInfo;
    uint16_t *p_processing_buffer;
    float *p_voltage_buffer;

    xRawDataQueue = xQueueCreate(4, sizeof(RawDataInfo_t));
    xVoltageDataQueue = xQueueCreate(2, sizeof(VoltageData_t));

    if (xRawDataQueue == NULL || xVoltageDataQueue == NULL)
    {
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    BSP_GPIO_STM32F411_SYN_Init();
    // Module_AD7616_Config();  
    Module_AD7616_Set_SampleRate(100);

    while (1)
    {
        if (xQueueReceive(xRawDataQueue, &rxInfo, portMAX_DELAY) == pdTRUE)
        {
            p_processing_buffer = (rxInfo.bufferIndex == 0) ? AD7616_DataBuffer_A : AD7616_DataBuffer_B;
            p_voltage_buffer = (rxInfo.bufferIndex == 0) ? AD7616_VoltageBuffer_A : AD7616_VoltageBuffer_B;

            for (uint32_t i = 0; i < rxInfo.validCount; i++)
            {
                int16_t raw_data = (int16_t)p_processing_buffer[i];
                // p_voltage_buffer[i] = (float)(raw_data );
                p_voltage_buffer[i] = (raw_data / 32768.0f) * 5.0f;
            }

            VoltageData_t txData = {
                .pBuffer    = p_voltage_buffer,
                .validCount = rxInfo.validCount
            };
            xQueueSend(xVoltageDataQueue, &txData, 0);

            /* 最后一包处理完后清空队列 */
            if (rxInfo.isLastPacket)
            {
                xQueueReset(xRawDataQueue);
            }
        }
    }
}

void DMA1_Stream0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (LL_DMA_IsActiveFlag_TC0(DMA1))
    {
        LL_DMA_ClearFlag_TC0(DMA1);

        RawDataInfo_t info = {
            .bufferIndex   = u8_dma_buffer,
            .validCount    = DMA_BUFFER_SIZE,
            .isLastPacket  = 0  /* 非最后一包 */
        };
        xQueueSendFromISR(xRawDataQueue, &info, &xHigherPriorityTaskWoken);

        u8_dma_buffer = 1 - u8_dma_buffer;

        if (u8_dma_buffer == 0)
            BSP_DMA_AD7616_Start(AD7616_DataBuffer_A, DMA_BUFFER_SIZE);
        else
            BSP_DMA_AD7616_Start(AD7616_DataBuffer_B, DMA_BUFFER_SIZE);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

__attribute__((section(".itcm"))) void EXTI1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_1))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_1);

        if (LL_GPIO_IsInputPinSet(GPIOE, LL_GPIO_PIN_1)) // 上升沿
        {
            u8_dma_buffer = 0;
            BSP_TIM3_PWM0_Start();
            BSP_DMA_AD7616_Start(AD7616_DataBuffer_A, DMA_BUFFER_SIZE);
        }
        else // 下降沿
        {
            BSP_TIM3_PWM0_Stop();

            uint32_t remaining = LL_DMA_GetDataLength(DMA1, LL_DMA_STREAM_0);
            uint32_t transferred = DMA_BUFFER_SIZE - remaining;

            LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_0);

            if (transferred > 0)
            {
                RawDataInfo_t info = {
                    .bufferIndex   = u8_dma_buffer,
                    .validCount    = transferred,
                    .isLastPacket  = 1  /* 最后一包 */
                };
                xQueueSendFromISR(xRawDataQueue, &info, &xHigherPriorityTaskWoken);
            }

            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}