#ifndef __BSP_H__
#define __BSP_H__

#include "main.h"
#include <stdio.h>

#include "bsp_gpio.h"
#include "bsp_timer.h"

typedef enum
{
  BSP_OK       = 0x00,
  BSP_ERROR    = 0x01,
  BSP_BUSY     = 0x02,
  BSP_TIMEOUT  = 0x03
} BSP_Status_t;


void BSP_Init(void);
void BSP_RunPer10ms(void);


#endif 

