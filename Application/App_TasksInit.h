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
// void TaskTickHook(void);

#endif

