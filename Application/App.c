#include "main.h"
#include "cmsis_os.h"
#include "memorymap.h"
#include "App.h"
#include "App_TasksInit.h"




void App_Init(void)
{
  osKernelInitialize();
  App_Tasks_Init();
  osKernelStart();
}


