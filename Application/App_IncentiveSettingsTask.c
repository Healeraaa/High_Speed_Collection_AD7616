#include "App_IncentiveSettingsTask.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp.h"
#include "stdio.h"


void App_IncentiveSettingsTask(void *argument)
{
    while(1)
    {
        Serial_SendPacket(0x01, (double[]){1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.10});
        vTaskDelay(1000);
    }
}