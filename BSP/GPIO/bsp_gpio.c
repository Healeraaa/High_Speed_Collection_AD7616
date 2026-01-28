#include "bsp_gpio.h"

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

  LL_EXTI_InitTypeDef EXTI_InitStruct = {0};  // EXTI 配置结构体

  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOE);  // 使能 GPIOE 时钟

  LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTE, LL_SYSCFG_EXTI_LINE0);  // 设置 PE0 为 EXTI0 中断源

  EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_0;       // 配置 EXTI Line 0
  EXTI_InitStruct.Line_32_63 = LL_EXTI_LINE_NONE;   // 不使用 Line 32-63
  EXTI_InitStruct.Line_64_95 = LL_EXTI_LINE_NONE;   // 不使用 Line 64-95
  EXTI_InitStruct.LineCommand = ENABLE;              // 使能 EXTI Line
  // EXTI_InitStruct.Mode = LL_EXTI_MODE_EVENT;         // 配置为事件模式(不触发中断,仅用于唤醒或 DMA)
  EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;  // 从 LL_EXTI_MODE_EVENT 改为 IT 必须设置为中断才能触发dma
  EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_FALLING;  // 下降沿触发(BUSY 信号由高变低表示转换完成)
  LL_EXTI_Init(&EXTI_InitStruct);                    // 初始化 EXTI

  LL_GPIO_SetPinPull(GPIOE, LL_GPIO_PIN_0, LL_GPIO_PULL_NO);     // PE0 无上下拉(AD7616 BUSY 为推挽输出)
  LL_GPIO_SetPinMode(GPIOE, LL_GPIO_PIN_0, LL_GPIO_MODE_INPUT);  // PE0 配置为输入模式

  NVIC_SetPriority(EXTI0_IRQn, 6);
  NVIC_EnableIRQ(EXTI0_IRQn);
}

__attribute__((section(".itcm")))
void EXTI0_IRQHandler(void)
{
    if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_0))
    {
        LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);
    }
}

