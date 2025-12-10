#include "main.h"
#include "bsp.h"
#include "Module.h"
#include "App.h"
#include "string.h"
uint8_t t1 = 0;
uint8_t t2 = 1;
uint8_t t3 = 2;
uint8_t t4 = 3;

uint8_t str[] = "Hello World!";

uint32_t target_addr = 0x30000008;


int main(void)
{
  BSP_Init();
  Module_Config();
  //     /* 填充前 64 字节 */
  //   // memset((void *)(target_addr - 4), 0xDD, 4);
    
  //   /* 填充后 64 字节 */
  //   memset((void *)target_addr, 0xDD, 256);
  App_Init();
  while (1)
  {

  }

}






