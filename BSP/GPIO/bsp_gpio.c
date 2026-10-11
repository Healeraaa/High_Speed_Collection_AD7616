#include "bsp.h"

// ==================== System ====================
void BSP_GPIO_System_Clock_Init(void)
{
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOH);
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);
}

// ==================== LED ====================
void BSP_GPIO_LED_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOC);
  LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_13);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_13;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

// ==================== KEY ====================
/**
 * @brief 初始化 Boot 选择按键
 * @note PA0，上拉输入，低电平表示按下
 *
 * 内部上拉保证按键松开时PA0有确定的高电平；按键按下后由硬件把PA0
 * 拉到GND。这里只配置普通输入，不使用EXTI，因为Boot只在上电时读取一次。
 */
void BSP_GPIO_BOOT_KEY_Init(void)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);

    GPIO_InitStruct.Pin   = LL_GPIO_PIN_0;
    GPIO_InitStruct.Mode  = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = LL_GPIO_PULL_UP;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;

    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
 * @brief 检测上电时 K1 是否持续按下
 * @retval 1：按下；0：未按下
 *
 * 机械按键闭合时可能在高低电平之间快速抖动。这里每隔10ms采样一次，
 * 只有连续5次都是低电平才进入Boot，避免上电噪声导致误进入升级模式。
 */
uint8_t BSP_GPIO_BOOT_KEY_IsPressed(void)
{
    uint8_t sample;

    for (sample = 0; sample < 5U; sample++)
    {
        /* 任意一次读到高电平，就认为没有持续按住K1。 */
        if (LL_GPIO_IsInputPinSet(GPIOA, LL_GPIO_PIN_0))
        {
            return 0U;
        }

        /* DWT忙等待，不依赖RTOS任务或软件定时器。 */
        BSP_DWT_Delay_ms(10U);
    }

    /* 连续约50ms都是低电平，确认用户确实按住K1。 */
    return 1U;
}

#define ALL_KEY_GPIO_CLK_ENABLE() {	\
		LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA);	\
	};
// 按键列表
static const KeyGpioConfig_t s_key_gpio_list[HARD_KEY_NUM] = {
	{GPIOA, LL_GPIO_PIN_0, 0 },		/* KEY1 */
};	

void BSP_GPIO_KEY_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
  // Enable GPIO Ports Clock
  ALL_KEY_GPIO_CLK_ENABLE();

	GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  for(uint8_t i=0;i<HARD_KEY_NUM;i++)
  {
    GPIO_InitStruct.Pin = s_key_gpio_list[i].GPIO_Pin;
    LL_GPIO_Init(s_key_gpio_list->GPIO_PORT, &GPIO_InitStruct);
  }
}
uint8_t BSP_GPIO_KEY_GetHardNum(void)
{
  return HARD_KEY_NUM;
}

KeyGpioConfig_t* BSP_GPIO_KEY_GetHandle(void)
{
  return (KeyGpioConfig_t*)s_key_gpio_list;
}


// ==================== AD7616_BUSY ====================
void BSP_GPIO_AD7616_BUSY_Init(void)
{
  LL_EXTI_InitTypeDef EXTI_InitStruct = {0};

  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOE);
  //配置 GPIO 模式：先定好电平基准
  LL_GPIO_SetPinPull(GPIOE, LL_GPIO_PIN_0, LL_GPIO_PULL_NO);
  LL_GPIO_SetPinMode(GPIOE, LL_GPIO_PIN_0, LL_GPIO_MODE_INPUT);
  // 配置 EXTI 映射
  LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE0);
  // 配置 EXTI 线与下降沿触发
  EXTI_InitStruct.Line_0_31   = LL_EXTI_LINE_0;
  EXTI_InitStruct.Line_32_63  = LL_EXTI_LINE_NONE;
  EXTI_InitStruct.Line_64_95  = LL_EXTI_LINE_NONE;
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode        = LL_EXTI_MODE_IT;       // 必须为 IT 模式才能触发后续的 DMA 或中断服务
  EXTI_InitStruct.Trigger     = LL_EXTI_TRIGGER_FALLING; // 下降沿触发
  LL_EXTI_Init(&EXTI_InitStruct);
  // 清除 EXTI 标志位
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_0)) 
  {
      LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);
  }
  // 使能 NVIC 中断
  NVIC_SetPriority(EXTI0_IRQn, 6);
  NVIC_EnableIRQ(EXTI0_IRQn);
}

// __attribute__((section(".itcm")))
// void EXTI0_IRQHandler(void)
// {
//     if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_0))
//     {
//         LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);
//     }
// }


// ==================== AD7616_RST ====================
void BSP_GPIO_AD7616_RST_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOE);
  LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_2);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}
void BSP_GPIO_AD7616_FULL_RST(void)
{
  LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_2);
  BSP_DWT_Delay_ms(1); // 保持至少 10us 的低电平
  LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_2);
  BSP_DWT_Delay_ms(1); // 等待 AD7616 上电稳定

}

// ==================== STM32F411 Synchronization ====================

void BSP_GPIO_STM32F411_SYN_Init(void)
{
  LL_EXTI_InitTypeDef EXTI_InitStruct = {0};

  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOE);
  LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_SYSCFG); // 确保 SYSCFG 时钟已开启

  LL_GPIO_SetPinPull(GPIOE, LL_GPIO_PIN_1, LL_GPIO_PULL_DOWN);
  LL_GPIO_SetPinMode(GPIOE, LL_GPIO_PIN_1, LL_GPIO_MODE_INPUT);
  //配置 EXTI 线与边沿触发
  LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE1);
  EXTI_InitStruct.Line_0_31   = LL_EXTI_LINE_1;
  EXTI_InitStruct.Line_32_63  = LL_EXTI_LINE_NONE;
  EXTI_InitStruct.Line_64_95  = LL_EXTI_LINE_NONE;
  EXTI_InitStruct.LineCommand = ENABLE;
  EXTI_InitStruct.Mode        = LL_EXTI_MODE_IT;
  EXTI_InitStruct.Trigger     = LL_EXTI_TRIGGER_RISING_FALLING; // 双边沿触发
  LL_EXTI_Init(&EXTI_InitStruct);

  //清除 EXTI 标志位
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_1)) 
  {
      LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_1);
  }

  // 使能 NVIC 中断
  NVIC_SetPriority(EXTI1_IRQn, 5);
  NVIC_EnableIRQ(EXTI1_IRQn);
}



