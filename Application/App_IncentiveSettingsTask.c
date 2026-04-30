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
        // 检查并处理来自上位机的数据（同时包含 ceiod 和 ceiof 两条命令）
        if (Module_USART1_IsDataReady()) {
            if (Module_USART1_ParseData(&rx_data) == Module_OK) {
                printf("Received both commands - Mode: %lu, Int Count: %lu, Float Count: %lu\n", 
                       rx_data.mode, rx_data.int_count, rx_data.float_count);

                // ========== 处理 ceiof 格式的浮点数据 ==========
                // ceiof:%f1,%f2,%f3,%f4,%f5,%f6\n
                // 6个%f对应 data_converter[0-5]
                if (rx_data.float_count > 0) {
                    // 更新 data_converter[0-...] 为接收到的浮点数
                    for (uint32_t i = 0; i < rx_data.float_count && i < 6; i++) {
                        data_converter[i].double_val = (double)rx_data.float_data[i];
                        printf("  data_converter[%lu] = %f\n", i, data_converter[i].double_val);
                    }
                    printf("Updated data_converter from ceiof\n");
                } else {
                    printf("Warning: ceiof has no float data\n");
                }
                
                // ========== 处理 ceiod 格式的整数数据 ==========
                // ceiod:%d1,%d2,%d3,%d4,%d5\n
                // %d1 是模式（Serial_SendPacket 第一参数），%d2-%d5 对应 data_converter[6].u8_array[0-3]
                if (rx_data.int_count >= 4) {
                    // 更新 data_converter[6].u8_array[0-3]（使用后4个整数）
                    for (uint32_t i = 0; i < 4 && i < rx_data.int_count; i++) {
                        data_converter[6].u8_array[i ] = (uint8_t)rx_data.int_data[i];
                        printf("  data_converter[6].u8_array[%lu] = %lu\n", i , rx_data.int_data[i]);
                    }
                    printf("Updated u8_array configuration from ceiod\n");
                } else if (rx_data.int_count > 0) {
                    printf("Warning: ceiod has insufficient data (got %lu, need at least 5)\n", rx_data.int_count);
                }
                
                // 发送数据包（使用 ceiod 中的第一个整数作为模式）
                Serial_SendPacket((uint8_t)rx_data.mode, (double *)data_converter);
                printf("Sent data packet with mode: %lu\n", rx_data.mode);
            }
        }

        vTaskDelay(100);
    }
}
