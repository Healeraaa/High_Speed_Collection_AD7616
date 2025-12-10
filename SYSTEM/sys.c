#include "main.h"

/**
  * @brief  使能所有 SRAM 区域的时钟
  * @note   必须在使用 SRAM1/2/3/4 之前调用
  * @retval None
  */
void SRAM_ClockEnable(void)
{
  /* ========================================
   * 使能 D2 域的 SRAM1/2/3 时钟
   * 对应地址:
   * - SRAM1: 0x30000000 - 0x3001FFFF (128KB)
   * - SRAM2: 0x30020000 - 0x3003FFFF (128KB)
   * - SRAM3: 0x30040000 - 0x30047FFF (32KB)
   * ======================================== */
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_D2SRAM1);  // SRAM1
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_D2SRAM2);  // SRAM2
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_D2SRAM3);  // SRAM3 
  
  /* ========================================
   * 使能 D3 域的 SRAM4 时钟
   * 对应地址:
   * - SRAM4: 0x38000000 - 0x3800FFFF (64KB)
   * ======================================== */
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_D3SRAM1);  // SRAM4
  
  /* ========================================
   * 使能 Backup SRAM 时钟（可选）
   * 对应地址:
   * - Backup SRAM: 0x38800000 - 0x38800FFF (4KB)
   * 注意: 需要同时使能备份域访问权限
   * ======================================== */
#ifdef USE_BACKUP_SRAM
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_BKPRAM);   // Backup SRAM
  
  /* 使能备份域访问 */
  LL_PWR_EnableBkUpAccess();
#endif
}

/**
  * @brief  配置低功耗模式下的 SRAM 保持（可选）
  * @note   在 Stop 模式下保持 SRAM 数据
  * @retval None
  */
void SRAM_LowPowerConfig(void)
{
  /* 使能 D2 域 SRAM1/2/3 在 SLEEP/STOP 模式下的时钟 */
  LL_AHB2_GRP1_EnableClockSleep(LL_AHB2_GRP1_PERIPH_D2SRAM1);
  LL_AHB2_GRP1_EnableClockSleep(LL_AHB2_GRP1_PERIPH_D2SRAM2);
  LL_AHB2_GRP1_EnableClockSleep(LL_AHB2_GRP1_PERIPH_D2SRAM3);
  
  /* 使能 D3 域 SRAM4 在 SLEEP/STOP 模式下的时钟 */
  LL_AHB4_GRP1_EnableClockSleep(LL_AHB4_GRP1_PERIPH_D3SRAM1);
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
  while(LL_FLASH_GetLatency()!= LL_FLASH_LATENCY_4)
  {
  }
  LL_PWR_ConfigSupply(LL_PWR_LDO_SUPPLY);
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE0);
  while (LL_PWR_IsActiveFlag_VOS() == 0)
  {
  }
  LL_RCC_HSE_Enable();

   /* Wait till HSE is ready */
  while(LL_RCC_HSE_IsReady() != 1)
  {

  }
  LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSE);
  LL_RCC_PLL1P_Enable();
  LL_RCC_PLL1R_Enable();
  LL_RCC_PLL1_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_4_8);
  LL_RCC_PLL1_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
  LL_RCC_PLL1_SetM(5);
  LL_RCC_PLL1_SetN(192);
  LL_RCC_PLL1_SetP(2);
  LL_RCC_PLL1_SetQ(2);
  LL_RCC_PLL1_SetR(2);
  LL_RCC_PLL1_Enable();

   /* Wait till PLL is ready */
  while(LL_RCC_PLL1_IsReady() != 1)
  {
  }

   /* Intermediate AHB prescaler 2 when target frequency clock is higher than 80 MHz */
   LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);

  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL1);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL1)
  {

  }
  LL_RCC_SetSysPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
  LL_RCC_SetAPB3Prescaler(LL_RCC_APB3_DIV_2);
  LL_RCC_SetAPB4Prescaler(LL_RCC_APB4_DIV_2);
  LL_SetSystemCoreClock(480000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
}

void MPU_Config(void)
{

  /* Disables the MPU */
  LL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  LL_MPU_ConfigRegion(LL_MPU_REGION_NUMBER0, 0x0, 0x24000000, LL_MPU_REGION_SIZE_512KB|LL_MPU_TEX_LEVEL1|LL_MPU_REGION_FULL_ACCESS|LL_MPU_INSTRUCTION_ACCESS_ENABLE|LL_MPU_ACCESS_NOT_SHAREABLE|LL_MPU_ACCESS_CACHEABLE|LL_MPU_ACCESS_BUFFERABLE);
//  LL_MPU_ConfigRegion(
//     LL_MPU_REGION_NUMBER1, 
//     0x0, 
//     0x30000000, 
//     LL_MPU_REGION_SIZE_128KB | 
//     LL_MPU_TEX_LEVEL1 | 
//     LL_MPU_REGION_FULL_ACCESS | 
//     LL_MPU_INSTRUCTION_ACCESS_ENABLE |  //  允许指令访问（即使不执行代码）
//     LL_MPU_ACCESS_NOT_SHAREABLE | 
//     LL_MPU_ACCESS_NOT_CACHEABLE |       // 保持 Non-Cacheable（避免 DMA 问题）
//     LL_MPU_ACCESS_BUFFERABLE            // 启用写缓冲（提高写性能）
//   );
  /* Enables the MPU */
  LL_MPU_Enable(LL_MPU_CTRL_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
		
  }
  /* USER CODE END Error_Handler_Debug */
}


#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
