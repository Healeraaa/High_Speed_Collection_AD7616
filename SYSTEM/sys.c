#include "main.h"

/*
 * “BOOT”四个ASCII字符组成的32位魔数。
 * BKP0R保存魔数，BKP1R保存它的按位取反值。只有两个寄存器同时匹配，
 * 才把它当成有效请求，可以降低随机残留数据被误判的可能性。
 */
#define SYSTEM_BOOT_REQUEST_MAGIC  0x424F4F54UL

/**
  * @brief 允许CPU写RTC备份寄存器
  * @note  不需要启动RTC计时功能，只需打开RTCAPB接口时钟和DBP写权限。
  */
static void System_BootRequest_EnableWriteAccess(void)
{
  /* STM32H743的RTC备份寄存器通过APB4接口访问。 */
  LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_RTCAPB);

  /* DBP=1后，软件才能修改备份域寄存器。 */
  LL_PWR_EnableBkUpAccess();
  while (LL_PWR_IsEnabledBkUpAccess() == 0U)
  {
  }
}

void System_BootRequest_Set(void)
{
  System_BootRequest_EnableWriteAccess();

  /*
   * 先写反码，最后写主魔数。若写入过程意外中断，两个值不会同时有效。
   */
  RTC->BKP1R = ~SYSTEM_BOOT_REQUEST_MAGIC;
  __DMB();
  RTC->BKP0R = SYSTEM_BOOT_REQUEST_MAGIC;

  /* 确保两次寄存器写入在随后触发系统复位前已经真正完成。 */
  __DSB();

  /* 写完重新保护备份域，防止其他代码意外改写。 */
  LL_PWR_DisableBkUpAccess();
}

void System_BootRequest_Clear(void)
{
  System_BootRequest_EnableWriteAccess();

  /* 先破坏主魔数，再清除反码，任何中间状态都不会被判定为有效。 */
  RTC->BKP0R = 0U;
  __DMB();
  RTC->BKP1R = 0U;
  __DSB();

  LL_PWR_DisableBkUpAccess();
}

uint8_t System_BootRequest_Take(void)
{
  uint8_t request_is_valid;
  uint8_t reset_is_software;

  System_BootRequest_EnableWriteAccess();

  /*
   * 软件进入Boot必须同时满足：
   * 1. 两个备份寄存器中的魔数互相匹配；
   * 2. 本次确实是CPU触发的软件复位。
   * 这样即使备份域中出现残留值，普通上电也不会误进Bootloader。
   */
  request_is_valid =
      ((RTC->BKP0R == SYSTEM_BOOT_REQUEST_MAGIC) &&
       (RTC->BKP1R == ~SYSTEM_BOOT_REQUEST_MAGIC)) ? 1U : 0U;

  reset_is_software =
      (LL_RCC_IsActiveFlag_SFTRST() != 0U) ? 1U : 0U;

  /*
   * 请求只允许使用一次。无论本次内容是否有效，都立即清除两个寄存器，
   * 避免后续复位再次进入Bootloader。
   */
  RTC->BKP0R = 0U;
  __DMB();
  RTC->BKP1R = 0U;
  __DSB();

  LL_PWR_DisableBkUpAccess();

  /* 复位原因已经完成采样，清除旧标志，避免影响下一次启动判断。 */
  LL_RCC_ClearResetFlags();

  return ((request_is_valid != 0U) &&
          (reset_is_software != 0U)) ? 1U : 0U;
}

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
  while (!(RCC->AHB2ENR & RCC_AHB2ENR_D2SRAM1EN)){}
  
  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_D2SRAM2);  // SRAM2
  while (!(RCC->AHB2ENR & RCC_AHB2ENR_D2SRAM2EN)){}

  LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_D2SRAM3);  // SRAM3 
  while (!(RCC->AHB2ENR & RCC_AHB2ENR_D2SRAM3EN)){}
  
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

  LL_RCC_HSI48_Enable();
   /* Wait till HSI48 is ready */
  while(LL_RCC_HSI48_IsReady() != 1)
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

/**
  * @brief  配置 MPU (Memory Protection Unit) 保护关键内存区域
  * @note   必须在系统初始化阶段调用,优先级高于 FMC 和其他外设初始化
  * @note   Region 0: AXI SRAM (0x24000000, 512KB) - Non-cacheable, 用于 DMA 缓冲区
  *         Region 1: FMC 扩展 IO (0x60000000, 64KB) - Device 类型, 用于 AD7616
  *         Region 2: D2 SRAM1 (0x30000000, 128KB) - Non-cacheable, 用于 AD7616 DMA 数据缓冲区 
  * @retval None
  */
void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct;

  HAL_MPU_Disable();  // 禁止 MPU

  /* ======== MPU Region 0: AXI SRAM (D1 域) ======== */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress      = 0x24000000;                    // AXI SRAM 基地址 (512KB)
  MPU_InitStruct.Size             = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;        // 读写权限
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;     // 不缓冲 (保证 DMA 一致性)
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;      // 不缓存 (避免 Cache 一致性问题)
  MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;      // 不共享
  MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;                // Normal Memory
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  
  
  /* ======== MPU Region 1: FMC 扩展 IO (AD7616) ======== */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress      = 0x60000000;                    // FMC Bank1 Sector1 基地址 (64KB)
  MPU_InitStruct.Size             = ARM_MPU_REGION_SIZE_64KB;	
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;        // 读写权限
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;         // 允许写缓冲 (提高 FMC 写性能)
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;      // 不缓存 (避免读取过期寄存器值) ⚠️ 不能用 CACHEABLE,会出现 2 次 CS/WE 信号
  MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;      // 不共享
  MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;                // Device 类型 (外设寄存器)
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

   /* ======== MPU Region 2: D2 SRAM1 (用于 AD7616 DMA 数据缓冲区) ======== */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress      = 0x30000000;                    // D2 SRAM1 基地址 (128KB)
  MPU_InitStruct.Size             = MPU_REGION_SIZE_128KB;         // 128KB 大小
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;        // 读写权限
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;     // 不缓冲 (保证 DMA 一致性)
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;      // 不缓存 (避免 DCache 一致性问题)
  MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;          // 共享 (DMA 和 CPU 共享访问)
  MPU_InitStruct.Number           = MPU_REGION_NUMBER2;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL0;                // Normal Memory
  MPU_InitStruct.SubRegionDisable = 0x00;                          // 不禁用子区域
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;// 禁止指令执行 (数据区)
  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);  // 使能 MPU (特权模式下允许默认内存访问)
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
