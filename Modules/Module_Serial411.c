#include "Module.h"
#include "bsp.h"

/**
 * @brief 全局增益配置变量，用于记录当前设置的所有放大倍数
 * @note  在 Serial_SendPacket 中自动更新
 */
Serial411_GainConfig_t g_Serial411_GainConfig = {
    .iv_gain = IV_GAIN_33,
    .voltage_gain_stage1 = VOLTAGE_GAIN_STAGE1_1X,
    .voltage_gain_stage2 = VOLTAGE_GAIN_STAGE2_1X,
    .feedback_select = FEEDBACK_GND,
    .we_channel = WE_CHANNEL_1};

/**
 * @brief  发送串口数据包
 * @param  huart: UART 句柄指针
 * @param  command: 命令字
 * @param  data: double 数组指针
 * @param  length: 数组长度（最大 SERIAL_DATA_LENGTH）
 * @retval BSP_OK: 成功, BSP_ERROR: 失败
 */
Module_Status_t Serial_SendPacket(uint8_t command, double *data)
{
    if (data == NULL)
    {
        return Module_ERROR;
    }

    // 将 data[6] (double) 转换为字节数组来访问各个字节
    Serial411_DoubleConverter_t *data_converter = (Serial411_DoubleConverter_t *)data;

    g_Serial411_GainConfig.we_channel = (WE_Channel_TypeDef)data_converter[6].u8_array[0];
    g_Serial411_GainConfig.iv_gain = (IV_Gain_TypeDef)data_converter[6].u8_array[1];
    g_Serial411_GainConfig.voltage_gain_stage1 = (Voltage_Gain_Stage1_TypeDef)data_converter[6].u8_array[2];
    g_Serial411_GainConfig.voltage_gain_stage2 = (Voltage_Gain_Stage2_TypeDef)data_converter[6].u8_array[3];
    g_Serial411_GainConfig.feedback_select = (Feedback_Select_TypeDef)data_converter[6].u8_array[4];

    Serial411_Packet_t packet;
    packet.hrader = SERIAL411_PACKET_HEADER;
    packet.tail = SERIAL411_PACKET_TAIL;
    packet.command = command;
    for (uint8_t i = 0; i < SERIAL411_DATA_LENGTH; i++)
    {
        packet.write_buffer[i].double_val = data[i];
    }

    BSP_USART3_SendByte(packet.hrader);
    BSP_USART3_SendByte(packet.command);
    for (uint8_t i = 0; i < SERIAL411_DATA_LENGTH; i++)
    {
        for (uint8_t j = 0; j < sizeof(double); j++)
        {
            BSP_USART3_SendByte(packet.write_buffer[i].u8_array[j]);
        }
    }
    BSP_USART3_SendByte(packet.tail);
    return Module_OK;
}
