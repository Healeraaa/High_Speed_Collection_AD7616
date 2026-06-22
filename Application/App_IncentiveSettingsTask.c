#include "App_IncentiveSettingsTask.h"
#include "App_TasksInit.h"
#include "App_CurveFit.h"
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
    data_converter[6].u8_array[1] = IV_GAIN_33;
    data_converter[6].u8_array[2] = VOLTAGE_GAIN_STAGE1_1X;
    data_converter[6].u8_array[3] = VOLTAGE_GAIN_STAGE2_1X;
    data_converter[6].u8_array[4] = FEEDBACK_GND;
    data_converter[6].u8_array[5] = FEEDBACK_GND;
    data_converter[6].u8_array[6] = FEEDBACK_GND;
    data_converter[6].u8_array[7] = FEEDBACK_GND;
    data_converter[7].double_val = DAC_CH_ALL;
    data_converter[8].double_val = 0.0;
    data_converter[9].double_val = 0.0;

    /* 根据初始增益配置注册对应的拟合曲线 */
    App_CurveFit_RegisterCallback(CurveFit_Range1_33_1_1);

    USART1_RxData_t rx_data = {0};

    while (1)
    {
        // 检查并处理来自上位机的数据（同时包含 ceiod 和 ceiof 两条命令）
        if (Module_USART1_IsDataReady())
        {
            if (Module_USART1_ParseData(&rx_data) == Module_OK)
            {
                // ========== 处理 ceiof 格式的浮点数据 ===========
                // ceiof:%f1,%f2,%f3,%f4,%f5,%f6\n
                // 6个%f对应 data_converter[0-5]
                if (rx_data.float_count > 0)
                {
                    // 更新 data_converter[0-...] 为接收到的浮点数
                    for (uint32_t i = 0; i < rx_data.float_count && i < 6; i++)
                    {
                        data_converter[i].double_val = (double)rx_data.float_data[i];
                    }
                }
                else
                {
                }

                // ========== 处理 ceiod 格式的整数数据 ==========
                // ceiod:%d1,%d2,%d3,%d4,%d5\n
                // %d1 是模式（Serial_SendPacket 第一参数），%d2-%d5 对应 data_converter[6].u8_array[0-3]
                if (rx_data.int_count >= 4)
                {
                    // 更新 data_converter[6].u8_array[0-3]（使用后4个整数）
                    for (uint32_t i = 0; i < 4 && i < rx_data.int_count; i++)
                    {
                        data_converter[6].u8_array[i] = (uint8_t)rx_data.int_data[i];
                    }

                    // ===== 根据配置挡位选择相应的拟合曲线 =====
                    IV_Gain_TypeDef iv_gain = (IV_Gain_TypeDef)data_converter[6].u8_array[1];
                    Voltage_Gain_Stage1_TypeDef stage1 = (Voltage_Gain_Stage1_TypeDef)data_converter[6].u8_array[2];
                    Voltage_Gain_Stage2_TypeDef stage2 = (Voltage_Gain_Stage2_TypeDef)data_converter[6].u8_array[3];

                    CurveFit_Callback_t callback = NULL;

                    // 根据配置组合选择拟合曲线
                    if (iv_gain == IV_GAIN_33 && stage1 == VOLTAGE_GAIN_STAGE1_1X && stage2 == VOLTAGE_GAIN_STAGE2_1X)//100mA
                    {
                        callback = CurveFit_Range1_33_1_1;
                    }
                    else if (iv_gain == IV_GAIN_33 && stage1 == VOLTAGE_GAIN_STAGE1_1X && stage2 == VOLTAGE_GAIN_STAGE2_3_3X)//30.3mA
                    {
                        callback = CurveFit_Range2_33_1_3_3;
                    }
                    else if (iv_gain == IV_GAIN_33 && stage1 == VOLTAGE_GAIN_STAGE1_1X && stage2 == VOLTAGE_GAIN_STAGE2_10X)//10mA
                    {
                        callback = CurveFit_Range3_33_1_10;
                    }
                    else if (iv_gain == IV_GAIN_1K && stage1 == VOLTAGE_GAIN_STAGE1_1X && stage2 == VOLTAGE_GAIN_STAGE2_1X)//3.3mA
                    {
                        callback = CurveFit_Range4_1K_1_1;
                    }
                    else if (iv_gain == IV_GAIN_1K && stage1 == VOLTAGE_GAIN_STAGE1_1X && stage2 == VOLTAGE_GAIN_STAGE2_3_3X)//1mA
                    {
                        callback = CurveFit_Range5_1K_1_3_3;
                    }
                    else if (iv_gain == IV_GAIN_10K && stage1 == VOLTAGE_GAIN_STAGE1_1X && stage2 == VOLTAGE_GAIN_STAGE2_1X)//330uA
                    {
                        callback = CurveFit_Range6_10K_1_1;
                    }
                    else if (iv_gain == IV_GAIN_10K && stage1 == VOLTAGE_GAIN_STAGE1_10X && stage2 == VOLTAGE_GAIN_STAGE2_3_3X)//100uA
                    {
                        callback = CurveFit_Range7_10K_10_3_3;
                    }
                    else if (iv_gain == IV_GAIN_100K && stage1 == VOLTAGE_GAIN_STAGE1_1X && stage2 == VOLTAGE_GAIN_STAGE2_1X)//33uA
                    {
                        callback = CurveFit_Range8_100K_1_1;
                    }
                    else if (iv_gain == IV_GAIN_100K && stage1 == VOLTAGE_GAIN_STAGE1_1X && stage2 == VOLTAGE_GAIN_STAGE2_3_3X)//10uA
                    {
                        callback = CurveFit_Range9_100K_1_3_3;
                    }
                    else if (iv_gain == IV_GAIN_100K && stage1 == VOLTAGE_GAIN_STAGE1_1X && stage2 == VOLTAGE_GAIN_STAGE2_10X)//3.3uA
                    {
                        callback = CurveFit_Range10_100K_1_10;
                    }
                    else if (iv_gain == IV_GAIN_100K && stage1 == VOLTAGE_GAIN_STAGE1_10X && stage2 == VOLTAGE_GAIN_STAGE2_3_3X)//1uA
                    {
                        callback = CurveFit_Range11_100K_10_3_3;
                    }
                    else if (iv_gain == IV_GAIN_100K && stage1 == VOLTAGE_GAIN_STAGE1_10X && stage2 == VOLTAGE_GAIN_STAGE2_10X)//330nA
                    {
                        callback = CurveFit_Range12_100K_10_10;
                    }
                    else if (iv_gain == IV_GAIN_100K && stage1 == VOLTAGE_GAIN_STAGE1_10X && stage2 == VOLTAGE_GAIN_STAGE2_33X)//100nA
                    {
                        callback = CurveFit_Range13_100K_10_33;
                    }
                    

                    // 注册选中的拟合曲线回调函数
                    App_CurveFit_RegisterCallback(callback);

                    // 发送数据包（使用 ceiod 中的第一个整数作为模式）
                    Serial_SendPacket((uint8_t)rx_data.mode, (double *)data_converter);
                }
            }

            
        }
        vTaskDelay(100);
    }
}
