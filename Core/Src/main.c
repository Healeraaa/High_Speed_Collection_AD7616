// #include "main.h"
// #include "bsp.h"
// #include "Module.h"
// #include "App.h"
// #include "string.h"

// int main(void)
// {
//   BSP_Init();
//   Module_Config();
//   App_Init();
//   while (1)
//   {
    
//   }

// }

#include "main.h"
#include "sys.h"
#include "Module_AD7616.h"
int main(void)
{
  HAL_Init();

  SystemClock_Config();
  Module_AD7616_Config();

  while (1)
  {

		*(__IO uint16_t *)(0x60000088U) = 0xA55A; 
		for(int i = 0; i < 30000; i++) __NOP();

  }
}




