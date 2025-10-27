#ifndef __MODULE_H__
#define __MODULE_H__

#include "main.h"
#include <stdio.h>

#define Enable_EventRecorder  1  //是否使能事件记录器，1使能，0不使能

#if Enable_EventRecorder == 1
#include "EventRecorder.h"  
#endif

#include "Module_Key.h"




void Module_Config(void);
void Module_RunPer10ms(void);


#endif 

