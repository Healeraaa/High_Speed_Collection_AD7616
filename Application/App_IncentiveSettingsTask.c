#include "App_IncentiveSettingsTask.h"
#include "App_TasksInit.h"
#include "Module.h"
#include "main.h"
#include "bsp.h"
#include "stdio.h"


void App_IncentiveSettingsTask(void *argument)
{
    while(1)
    {
        Serial_SendPacket(0x01, (double[]){100.0,-800.0,50.0,75.0,250.0,500.0,0,0.0,0.0,0.0});
        vTaskDelay(20000);
    }
}
