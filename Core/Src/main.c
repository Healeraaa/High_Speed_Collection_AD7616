#include "main.h"
#include "bsp.h"
#include "Module.h"
#include "App.h"
#include "string.h"

int main(void)
{
  BSP_Init();
  Module_Config();
  BSP_TIM3_PWM0_Start();
  App_Init();
  while (1)
  {
  }

}




