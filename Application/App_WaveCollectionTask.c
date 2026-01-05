#include "App_WaveCollectionTask.h"
#include "App_TasksInit.h"
#include "main.h"
#include "bsp_fmc.h"
#include "Module_AD7616.h"
#include "bsp_fmc.h"
#include "bsp_dma.h"


 __attribute__((section("RAM_D3"))) __attribute__((aligned(2)))  uint16_t AD7616_DataBuffer[1024];  /**< AD7616 数据缓冲区 */

void App_WaveCollectionTask(void *argument)
{
    uint16_t i;
    __IO int16_t read_data;
    __IO float f_read_data;
    // SCB_InvalidateDCache_by_Addr((uint32_t *)AD7616_DataBuffer, sizeof(AD7616_DataBuffer));
    BSP_TIM3_PWM0_Start();
    BSP_DMA_TIM3_Start(AD7616_DataBuffer, 1024);

    while (1)
    {                           
    // read_data = BSP_FMC_PSRAM_ReadHalfWord(0);
    // f_read_data = read_data/32768.0f*5.0f;
    // vTaskDelay(1);
    }
}

void DMA1_Stream0_IRQHandler(void)
{
    if (LL_DMA_IsActiveFlag_TC0(DMA1)) {
        LL_DMA_ClearFlag_TC0(DMA1);
    }

}