#include "Module.h"
#include "bsp.h"


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
    if (data == NULL ) {
        return Module_ERROR;
    }
    Serial411_Packet_t packet;
    packet.hrader = SERIAL411_PACKET_HEADER;
    packet.tail = SERIAL411_PACKET_TAIL;
    packet.command = command;
    for (uint8_t i = 0; i < SERIAL411_DATA_LENGTH; i++) {
        packet.write_buffer[i].double_val = data[i];
    }

    BSP_USART3_SendByte(packet.hrader);
    BSP_USART3_SendByte(packet.command);    
    for (uint8_t i = 0; i < SERIAL411_DATA_LENGTH; i++) {
        for (uint8_t j = 0; j < sizeof(double); j++) {
            BSP_USART3_SendByte(packet.write_buffer[i].u8_array[j]);
        }
    }
    BSP_USART3_SendByte(packet.tail);
    return Module_OK;

}