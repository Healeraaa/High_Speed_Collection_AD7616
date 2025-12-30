#include "main.h"
#include "bsp.h"
#include "Module.h"
#include "App.h"
#include "string.h"

int main(void)
{
  BSP_Init();
  Module_Config();
  App_Init();
  while (1)
  {
  }

}




