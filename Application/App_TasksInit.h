#ifndef __APP_TASKSINIT_H__
#define __APP_TASKSINIT_H__



#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "task.h"
#include "cmsis_os2.h"



void App_Tasks_Init(void);

/**
 * 获取 USART1 发送互斥锁句柄
 * 用于Module_TransmitUpper中的同步操作
 *
 * @return 互斥锁句柄
 */
SemaphoreHandle_t App_GetUSART1_TxMutex(void);

// void TaskTickHook(void);

#endif

