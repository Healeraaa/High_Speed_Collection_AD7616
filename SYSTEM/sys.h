#ifndef __SYS_H__
#define __SYS_H__

#include <stdint.h>

#define SystemCoreClock 480000000U

void SystemClock_Config(void);
void SRAM_ClockEnable(void);
void SRAM_LowPowerConfig(void);
void MPU_Config(void);

/*
 * 在RTC备份寄存器中保存一次性的“下次复位进入Bootloader”请求。
 * RTC备份寄存器不会被NVIC_SystemReset()清零，因此Boot工程可以在
 * MCU复位后读取该请求；它不是普通RAM，也不会产生Flash擦写损耗。
 */
void System_BootRequest_Set(void);
void System_BootRequest_Clear(void);
uint8_t System_BootRequest_Take(void);

#endif
