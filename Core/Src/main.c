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
    // *(__IO uint16_t *)(0x60000000U) = 0xA55A; 
    // BSP_DWT_Delay_ms(10);
  }

}




