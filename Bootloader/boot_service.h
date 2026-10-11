#ifndef __BOOT_SERVICE_H__
#define __BOOT_SERVICE_H__

/**
 * @brief 进入串口升级服务循环
 *
 * 本函数初始化 Boot 专用 USART1，然后持续执行：
 * 收取协议帧 -> 检查帧错误 -> 分发 HELLO/START/DATA/END -> 回复 ACK/NACK。
 *
 * @note 该函数正常情况下不会返回；离开升级模式需要复位或重新上电。
 */
void Boot_Service_Run(void);

#endif
