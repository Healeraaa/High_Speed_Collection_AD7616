#include "App_IncentiveSettingsTask.h"
#include "App_TasksInit.h"
#include "Module.h"
#include "main.h"
#include "bsp.h"
#include "stdio.h"


void App_IncentiveSettingsTask(void *argument)
{
    Serial411_DoubleConverter_t data_converter[SERIAL411_DATA_LENGTH];
    data_converter[0].double_val = 100.0;
    data_converter[1].double_val = -800.0;    
    data_converter[2].double_val = 50.0;
    data_converter[3].double_val = 75.0;
    data_converter[4].double_val = 250.0;
    data_converter[5].double_val = 500.0;   
    data_converter[6].u8_array[0] = WE_CHANNEL_1;
    data_converter[6].u8_array[1] = IV_GAIN_100;
    data_converter[6].u8_array[2] = VOLTAGE_GAIN_10;
    data_converter[6].u8_array[3] = FB_CHANNEL_3; 
    data_converter[7].double_val = DAC_CH_ALL; 
    data_converter[8].double_val = 0.0;
    data_converter[9].double_val = 0.0;


    while(1)
    {
        Serial_SendPacket(0x01, data_converter);
        vTaskDelay(20000);
    }
}
