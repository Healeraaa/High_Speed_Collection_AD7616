#ifndef __APP_H__
#define __APP_H__

#define Enable_EventRecorder  0  //是否使能事件记录器，1使能，0不使能

#if Enable_EventRecorder == 1
#include "EventRecorder.h"  
#endif

void App_Init(void);


#endif 

