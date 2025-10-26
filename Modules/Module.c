#include "main.h"
#include "Module.h"




void Module_Config(void)
{
  #if Enable_EventRecorder == 1 
    EventRecorderInitialize(EventRecordAll, 1U); //初始化事件记录器  
    EventRecorderStart(); //启动事件记录器
  #endif
  
  Module_KEY_Config();



}
