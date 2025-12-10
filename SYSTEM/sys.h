#ifndef __SYS_H__
#define __SYS_H__

#define SystemCoreClock 480000000U

void SystemClock_Config(void);
void SRAM_ClockEnable(void);
void SRAM_LowPowerConfig(void);
void MPU_Config(void);

#endif
