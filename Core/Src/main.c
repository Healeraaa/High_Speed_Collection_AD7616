#include "main.h"
#include "bsp.h"
#include "Module.h"
#include "App.h"
#include "string.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

int main(void)
{
  BSP_Init();

  MX_USB_DEVICE_Init();
  while (1)
  {
    CDC_Transmit_HS((uint8_t *)"Module Config OK!\r\n", strlen("Module Config OK!\r\n"));
    BSP_DWT_Delay_ms(500);
  }

  Module_Config();


  
  


  App_Init();
  while (1)
  {
  }

}