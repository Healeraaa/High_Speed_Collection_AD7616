#ifndef __MODULE_LIGHTCOUNTING_H__
#define __MODULE_LIGHTCOUNTING_H__

#include "stdint.h"

void Module_LightCounting_Init(void);
void Module_LightCounting_Start(void);
void Module_LightCounting_Stop(void);
uint32_t Module_LightCounting_GetCount(void);
void Module_LightCounting_ClearCount(void);
uint32_t Module_LightCounting_GetAndClearCount(void);


#endif 

