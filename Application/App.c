#include "main.h"
#include "cmsis_os.h"
#include "memorymap.h"
#include "App.h"
#include "App_TasksInit.h"

void App_Init(void)
{
#if Enable_EventRecorder == 1
  EventRecorderInitialize(EventRecordAll, 1U); // 初始化事件记录器
  EventRecorderStart();                        // 启动事件记录器
#endif

  osKernelInitialize();
  App_Tasks_Init();
  osKernelStart();
}
