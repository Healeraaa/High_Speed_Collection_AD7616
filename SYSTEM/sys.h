#ifndef __SYS_H__
#define __SYS_H__

#include <stdint.h>

#define SystemCoreClock 480000000U

void SystemClock_Config(void);
void SRAM_ClockEnable(void);
void SRAM_LowPowerConfig(void);
void MPU_Config(void);

/*
 * RTC备份寄存器中的一次性Bootloader请求。
 * Boot启动时调用System_BootRequest_Take()读取并立即清除该请求。
 */
void System_BootRequest_Set(void);
void System_BootRequest_Clear(void);
uint8_t System_BootRequest_Take(void);

#endif
