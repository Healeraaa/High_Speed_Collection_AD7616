#ifndef __MODULE_H__
#define __MODULE_H__

#include "main.h"
#include <stdio.h>

#include "Module_Key.h"
#include "Module_Serial411.h"

#ifndef MODULE_STATUS_T_DEFINED
#define MODULE_STATUS_T_DEFINED
typedef enum
{
  Module_OK       = 0x00,
  Module_ERROR    = 0x01,
  Module_BUSY     = 0x02,
  Module_TIMEOUT  = 0x03
} Module_Status_t;
#endif


void Module_Config(void);
void Module_RunPer10ms(void);


#endif 

