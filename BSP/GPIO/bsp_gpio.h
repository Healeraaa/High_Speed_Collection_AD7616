#ifndef __BSP_GPIO_H__
#define __BSP_GPIO_H__


#include "main.h"

// ==================== System ====================
void BSP_GPIO_System_Clock_Init(void);
// ==================== LED ====================
void BSP_GPIO_LED_Init(void);
// ==================== KEY ====================
#define HARD_KEY_NUM  1 //按键数量
typedef struct
{
	GPIO_TypeDef * GPIO_PORT;
	uint32_t GPIO_Pin;
    uint8_t ActiveLevel;//激活电平 0-低电平激活 1-高电平激活
}KeyGpioConfig_t;
void BSP_GPIO_KEY_Init(void);
uint8_t BSP_GPIO_KEY_GetHardNum(void);
KeyGpioConfig_t* BSP_GPIO_KEY_GetHandle(void);



#endif 

