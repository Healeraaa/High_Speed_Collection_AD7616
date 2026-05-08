#ifndef __APP_LIGHTCOLLECTIONTASK_H__
#define __APP_LIGHTCOLLECTIONTASK_H__

#include "FreeRTOS.h"
#include "queue.h"

/**
 * @brief  光数据收集和处理任务
 * @param  argument: Not used
 * @retval None
 */
void App_LightCollectionTask(void *argument);
void App_LightCollection_Start(void);
void App_LightCollection_Stop(void);

/**
 * 光数据队列 - 处理后的光数据包
 * 结构体：{float *pLightBuffer, uint32_t validCount}
 */
extern QueueHandle_t xLightDataQueue;

#endif

