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

