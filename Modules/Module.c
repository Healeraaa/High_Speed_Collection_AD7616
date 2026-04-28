#include "main.h"
#include "Module.h"
#include "Module_Key.h"
#include "Module_AD7616.h"
#include "Module_LightCounting.h"
#include "bsp_timer.h"





void Module_Config(void)
{
  
  Module_KEY_Config();
  Module_AD7616_Config();
  Module_LightCounting_Init();
  Module_LightCounting_Start();
  
  
  
  
}

void Module_RunPer10ms(void)
{
  Moudle_Key_Scan10ms();
}
