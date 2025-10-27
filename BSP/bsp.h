#ifndef __BSP_H__
#define __BSP_H__

#include "main.h"
#include <stdio.h>

#include "bsp_gpio.h"
#include "bsp_timer.h"


void BSP_Init(void);
void BSP_RunPer10ms(void);


#endif 

