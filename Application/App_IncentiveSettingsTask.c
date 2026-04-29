#include "App_IncentiveSettingsTask.h"
#include "App_TasksInit.h"
#include "Module.h"
#include "Module_ReceiveUpper.h"
#include "main.h"
#include "bsp.h"
#include "stdio.h"

// void App_IncentiveSettingsTask(void *argument)
// {
//     Serial411_DoubleConverter_t data_converter[SERIAL411_DATA_LENGTH];
//     data_converter[0].double_val = 100.0;
//     data_converter[1].double_val = -800.0;
//     data_converter[2].double_val = 50.0;
//     data_converter[3].double_val = 75.0;
//     data_converter[4].double_val = 250.0;
//     data_converter[5].double_val = 500.0;
//     data_converter[6].u8_array[0] = WE_CHANNEL_1        ;
//     data_converter[6].u8_array[1] = IV_GAIN_1K;
//     data_converter[6].u8_array[2] = VOLTAGE_GAIN_STAGE1_5X;
//     data_converter[6].u8_array[3] = VOLTAGE_GAIN_STAGE2_1X;
//     data_converter[6].u8_array[4] = FEEDBACK_GND;
//     data_converter[6].u8_array[5] = FEEDBACK_GND;
//     data_converter[6].u8_array[6] = FEEDBACK_GND;
//     data_converter[6].u8_array[7] = FEEDBACK_GND;
//     data_converter[7].double_val = DAC_CH_A;
//     data_converter[8].double_val = 0.0;
//     data_converter[9].double_val = 0.0;

//     while(1)
//     {
//         Serial_SendPacket(0x01, (double *)data_converter);
//         vTaskDelay(20000);
//     }
// }

void App_IncentiveSettingsTask(void *argument)
{
    Serial411_DoubleConverter_t data_converter[SERIAL411_DATA_LENGTH];
    data_converter[0].double_val = 0.0;
    data_converter[1].double_val = 0.0;
    data_converter[2].double_val = 500.0;
    data_converter[3].double_val = -500.0;
    data_converter[4].double_val = 30.0;
    data_converter[5].double_val = 3.0;
    data_converter[6].u8_array[0] = WE_CHANNEL_1;
    data_converter[6].u8_array[1] = IV_GAIN_1K;
    data_converter[6].u8_array[2] = VOLTAGE_GAIN_STAGE1_5X;
    data_converter[6].u8_array[3] = VOLTAGE_GAIN_STAGE2_1X;
    data_converter[6].u8_array[4] = FEEDBACK_GND;
    data_converter[6].u8_array[5] = FEEDBACK_GND;
    data_converter[6].u8_array[6] = FEEDBACK_GND;
    data_converter[6].u8_array[7] = FEEDBACK_GND;
    data_converter[7].double_val = DAC_CH_ALL;
    data_converter[8].double_val = 0.0;
    data_converter[9].double_val = 0.0;

    USART1_RxData_t rx_data = {0};

    while (1)
    {
        // 检查并处理来自上位机的数据
        if (Module_USART1_IsDataReady()) {
            if (Module_USART1_ParseData(&rx_data) == BSP_OK) {
                // 根据接收到的模式处理数据
                printf("Received Mode: %lu, Data Count: %lu\n", rx_data.mode, rx_data.float_count);

                // 更新 data_converter 前6个参数（对应浮点数据）
                for (uint32_t i = 0; i < rx_data.float_count && i < 6; i++) {
                    data_converter[i].double_val = (double)rx_data.float_data[i];
                    printf("  data_converter[%lu] = %f\n", i, data_converter[i].double_val);
                }
                Serial_SendPacket((uint8_t)rx_data.mode, (double *)data_converter);
            }

        }

        vTaskDelay(5);

        // Serial_SendPacket(0x00, (double *)data_converter);
        // vTaskDelay(500000);
    }
}
