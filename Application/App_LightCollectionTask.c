#include "App_LightCollectionTask.h"
#include "App_TasksInit.h"
#include "main.h"
#include "Module_LightCounting.h"
#include "Module_TransmitUpper.h"
#include "bsp_timer.h"

#define LIGHT_BUFFER_SIZE 1024

__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) float Light_DataBuffer_A[LIGHT_BUFFER_SIZE] = {0.0f};
__attribute__((section("RAM_D3"))) __attribute__((aligned(32))) float Light_DataBuffer_B[LIGHT_BUFFER_SIZE] = {0.0f};

/* 原始光数据信息（ISR → 处理任务）*/
typedef struct
{
  uint8_t bufferIndex;
  uint32_t validCount;
  uint8_t isLastPacket; /* 是否为本轮最后一包 */
} RawLightDataInfo_t;

/* 处理后的光数据包（处理任务 → 上传任务）*/
typedef struct
{
  float *pLightBuffer;
  uint32_t validCount;
} LightData_t;

QueueHandle_t xRawLightDataQueue = NULL;
QueueHandle_t xLightDataQueue = NULL;

static volatile uint8_t u8_light_buffer = 0;
static volatile uint32_t u32_light_count = 0;

/**
 * @brief  光数据收集和处理任务
 * @param  argument: Not used
 * @retval None
 */
void App_LightCollectionTask(void *argument)
{
  RawLightDataInfo_t rxInfo;
  float *p_light_buffer;

  /* 创建数据队列 */
  xRawLightDataQueue = xQueueCreate(4, sizeof(RawLightDataInfo_t));
  xLightDataQueue = xQueueCreate(2, sizeof(LightData_t));

  xQueueReset(xRawLightDataQueue);
  xQueueReset(xLightDataQueue);

  if (xRawLightDataQueue == NULL || xLightDataQueue == NULL)
  {
    while (1)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  /* 初始化光计数模块 */
  Module_LightCounting_Init();


  while (1)
  {
    if (xQueueReceive(xRawLightDataQueue, &rxInfo, portMAX_DELAY) == pdTRUE)
    {
      p_light_buffer = (rxInfo.bufferIndex == 0) ? Light_DataBuffer_A : Light_DataBuffer_B;

      /* ========== 数据处理 ========== */
      /* 这里可以对光数据进行处理、校准等操作 */
      /* 当前直接透传数据 */

      /* 将处理后的数据发送到上传队列 */
      LightData_t txData = {
          .pLightBuffer = p_light_buffer,
          .validCount = rxInfo.validCount};
      xQueueSend(xLightDataQueue, &txData, 0);

      /* 最后一包处理完后清空队列 */
      if (rxInfo.isLastPacket)
      {
        xQueueReset(xRawLightDataQueue);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

/**
 * @brief  TIM4 中断服务程序 - 光计数收集
 * @note   周期性收集光计数值，填充到缓冲区
 * @retval None
 */
__attribute__((section(".itcm"))) void TIM4_IRQHandler(void)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (LL_TIM_IsActiveFlag_UPDATE(TIM4))
  {
    LL_TIM_ClearFlag_UPDATE(TIM4);

    /* 读取光计数值并转换为浮点数 */
    uint32_t light_count = Module_LightCounting_GetAndClearCount();
    float *p_current_buffer = (u8_light_buffer == 0) ? Light_DataBuffer_A : Light_DataBuffer_B;

    if (u32_light_count < LIGHT_BUFFER_SIZE)
    {
      p_current_buffer[u32_light_count] = (float)light_count;
      u32_light_count++;
    }

    /* 缓冲区满，发送数据到处理队列 */
    if (u32_light_count >= LIGHT_BUFFER_SIZE)
    {
      if (xRawLightDataQueue != NULL)
      {
        RawLightDataInfo_t info = {
            .bufferIndex = u8_light_buffer,
            .validCount = LIGHT_BUFFER_SIZE,
            .isLastPacket = 0};
        xQueueSendFromISR(xRawLightDataQueue, &info, &xHigherPriorityTaskWoken);

        /* 切换缓冲区 */
        u8_light_buffer = 1 - u8_light_buffer;
        u32_light_count = 0;

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
      }
      else
      {
        /* 队列未创建，重置计数 */
        u32_light_count = 0;
      }
    }
  }
}

void App_LightCollection_Start(void)
{
  u8_light_buffer = 0;
  u32_light_count = 0;

  Module_LightCounting_Start();
  Module_LightCounting_ClearCount(); /* 清除计数器值 */

  BSP_TIM4_ClearCount(); /* 清除定时器计数值 */
  BSP_TIM4_COUNT_Start();

}

void App_LightCollection_Stop(void)
{
  if(u32_light_count > 0)
  {
    /* 发送最后一包数据 */
    if (xRawLightDataQueue != NULL)
    {
      RawLightDataInfo_t info = {
          .bufferIndex = u8_light_buffer,
          .validCount = u32_light_count,
          .isLastPacket = 1};
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xQueueSendFromISR(xRawLightDataQueue, &info, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
  BSP_TIM4_COUNT_Stop(); /* 停止定时器中断 */
  BSP_TIM4_ClearCount(); /* 清除定时器计数值 */
  /* 停止计数器 */
  Module_LightCounting_Stop();
  Module_LightCounting_ClearCount(); /* 清除计数器值 */
  /* 重置缓冲区索引和计数 */
  u8_light_buffer = 0;
  u32_light_count = 0;
}

