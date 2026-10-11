/* Private includes -----------------------------------------------------------*/
#include "App_TasksInit.h"
#include "bsp.h"
#include "Module.h"
#include "Module_AD7616.h"
#include "Module_TransmitUpper.h"
#include "bsp_fmc.h"
#include "App_WaveCollectionTask.h"
#include "App_VOFA_DataUpload.h"
#include "App_IncentiveSettingsTask.h"
#include "App_LightCollectionTask.h"



/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Timers --------------------------------------------------------------------*/

/* Tasks ---------------------------------------------------------------------*/
TaskHandle_t App_LEDToggle_Task_Handle;
TaskHandle_t App_Run10ms_Task_Handle;
TaskHandle_t App_Key_Task_Handle;
TaskHandle_t App_AD7616_Task_Handle;
TaskHandle_t App_IV_DataUpload_Task_Handle;
TaskHandle_t App_Light_DataUpload_Task_Handle;
TaskHandle_t App_IncentiveSettingsTask_Handle;
TaskHandle_t App_LightCollectionTask_Handle;

/* Queues --------------------------------------------------------------------*/

/* Events --------------------------------------------------------------------*/


/**
 * @brief  LED间隔0.5s闪烁一次
 * @param  argument: Not used
 * @retval None
 */
void App_LEDToggle_Task(void *argument)
{
  while (1)
  {
    LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_13);
    vTaskDelay(500);
  }
}

/**
 * @brief  10ms 周期任务
 * @param  argument: Not used
 * @retval None
 * @note   用于周期性执行的任务，如按键扫描、LED扫描等
 */
void App_Run10ms_Task(void *argument)
{
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 10ms 周期
  
  // 初始化 xLastWakeTime 为当前时间
  xLastWakeTime = xTaskGetTickCount();
  while (1)
  {
    Module_RunPer10ms();  // 调用模块的10ms运行函数
    
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

/**
 * @brief  按键测试处理任务
 * @param  argument: Not used
 * @retval None
 * @note   用于测试按键功能
 */

void App_KeyTestTask(void *argument)
{
  float f_data[8] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};
  while (1)
  {
    uint8_t ucKeyCode = Moudle_Key_GetFifoBuffer();	/* 读取键值, 无键按下时返回 KEY_NONE = 0 */
		if (ucKeyCode != KEY_NONE)
		{
			switch (ucKeyCode)
			{
				case KEY_1_DOWN:			/* K1键按下 */
					printf("K1_Dowm\r\n");
          // CDC_Transmit_HS("K1_Dowm\r\n", sizeof("K1_Dowm\r\n") - 1);
          Module_USB_TransmitUpper_SendFrame(DEVICE_TYPE_IV, 100, f_data, 8);
					break;

				case KEY_1_UP:				/* K1键弹起 */
					printf("K1_Up\r\n");
          // CDC_Transmit_HS("K1_Up\r\n", sizeof("K1_Up\r\n") - 1);
					break;
				case KEY_1_LONG:				/* K1键弹起 */
					printf("K1_Long\r\n");
          // CDC_Transmit_HS("K1_Long\r\n", sizeof("K1_Long\r\n") - 1);
					break;
				default:
					/* 其它的键值不处理 */
					break;
			}
  		}
      vTaskDelay(50);
  }
}

/**
 * @brief  AD7616数据读取任务
 * @param  argument: Not used
 * @retval None
 * @note   
 */

// void App_AD7616_Task(void *argument)
// {
//   uint16_t i;
//   __IO int16_t read_data;
//   __IO float f_read_data;
//   while (1)
//   {                           

//     // Module_AD7616_WriteReg(AD7616_REG_RANGE_A1, 0xA5);
//     // for (volatile int i = 0; i < 10; i++);
//     read_data = BSP_FMC_PSRAM_ReadHalfWord(0);
//     f_read_data = read_data/32768.0f*5.0f;
//     // read_data = Module_AD7616_ReadReg(AD7616_REG_RANGE_A1);
//     vTaskDelay(1);
//     // Module_AD7616_ReadReg(AD7616_REG_RANGE_A1);
//     // AD7616_DataBuffer[0] = Module_AD7616_ReadChannel(0);
//     // *(__IO uint16_t *)(0x60000000U) = 0xA55A; 
//     // *(__IO uint16_t *)(0x60000000U) = 0xA5A5; 
//   }
// }




/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void App_Tasks_Init(void)
{
  /* 创建USB上传互斥锁和发送完成信号量。 */
  configASSERT(Module_TransmitUpper_Init() == 0);
  xTaskCreate(App_LEDToggle_Task, "App_LEDToggle_Task", 128, NULL, 1, &App_LEDToggle_Task_Handle);
  xTaskCreate(App_Run10ms_Task, "App_Run10ms_Task", 256, NULL, 2, &App_Run10ms_Task_Handle);    
  xTaskCreate(App_KeyTestTask, "App_KeyTestTask", 256, NULL, 2, &App_Key_Task_Handle);        
  xTaskCreate(App_WaveCollectionTask, "App_WaveCollectionTask", 256, NULL, 3, &App_AD7616_Task_Handle);     
  xTaskCreate(App_IVDataUploadTask, "App_IVDataUploadTask", 128*4, NULL, 1, &App_IV_DataUpload_Task_Handle);   
  xTaskCreate(App_LightDataUploadTask, "App_LightDataUploadTask", 128*4, NULL, 1, &App_Light_DataUpload_Task_Handle);   
  xTaskCreate(App_IncentiveSettingsTask, "App_IncentiveSettingsTask", 256, NULL,3, &App_IncentiveSettingsTask_Handle);
  xTaskCreate(App_LightCollectionTask, "App_LightCollectionTask", 256, NULL, 3, &App_LightCollectionTask_Handle);
}



