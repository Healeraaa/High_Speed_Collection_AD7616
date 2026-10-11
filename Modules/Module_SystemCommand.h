#ifndef __MODULE_SYSTEM_COMMAND_H__
#define __MODULE_SYSTEM_COMMAND_H__

#include "main.h"

/**
 * @brief 初始化USART1系统命令解析状态
 * @note  USART1硬件已经由BSP_Init()初始化，本函数只清空命令缓存。
 */
void Module_SystemCommand_Config(void);

/**
 * @brief 从USART1环形缓冲区取出字节并解析完整命令
 * @note  由Module_RunPer10ms()调用，运行在FreeRTOS任务环境，不在中断中。
 */
void Module_SystemCommand_RunPer10ms(void);

#endif /* __MODULE_SYSTEM_COMMAND_H__ */
