#include "Module_Key.h"
#include "bsp_gpio.h"
#include "stdio.h"

#define COMBO_KEY_NUM  2   /* 组合键数量 */

/* 静态全局变量 */
static KeyGpioConfig_t* key_gpio_list = NULL;                   /**< 指向从BSP层获取的按键GPIO配置数组的指针 */
static uint8_t KEY_COUNT = HARD_KEY_NUM + COMBO_KEY_NUM;        /**< 总按键数量 = 实体按键数量 + 组合键数量 */
static KeyState_t s_key_states[HARD_KEY_NUM + COMBO_KEY_NUM];   /**< 存储所有按键状态的数组 */
static KeyFifo_t s_key_fifo;                                    /**< 按键事件FIFO缓冲区 */

/* 内部静态函数声明 */
static void Module_InitKeyVar(void);
static void Moudle_Key_Detect(uint8_t key_id);
static uint8_t Moudle_Key_IsPinActive(uint8_t key_id);
static uint8_t Moudle_Key_IsPressed(uint8_t key_id);


/**
  * @brief  初始化按键模块的所有内部变量。
  * @note   此函数会清空FIFO，并将所有按键状态重置为默认值。
  * @param  无
  * @retval 无
  */
static void Module_InitKeyVar(void)
{
    uint8_t i;

    /* 对按键FIFO读写指针清零 */
    s_key_fifo.Read = 0;
    s_key_fifo.Write = 0;

    /* 给每个按键结构体成员变量赋一组缺省值 */
    for (i = 0; i < KEY_COUNT; i++)
    {
        s_key_states[i].LongTime = KEY_LONG_TIME;
        s_key_states[i].Count = KEY_FILTER_TIME / 2;
        s_key_states[i].State = 0;
        s_key_states[i].RepeatSpeed = 0;
        s_key_states[i].RepeatCount = 0;
    }

    // /* 单独设置需要连发功能的按键参数 */
    // Moudle_Key_SetParam(KID_JOY_U, 100, 6); /* 长按1秒后，每60ms连发一次 */
    // Moudle_Key_SetParam(KID_JOY_D, 100, 6);
    // Moudle_Key_SetParam(KID_JOY_L, 100, 6);
    // Moudle_Key_SetParam(KID_JOY_R, 100, 6);
}

/**
  * @brief  配置并初始化整个按键模块。
  * @note   这是按键模块的入口函数，应在系统初始化时调用。
  * @param  无
  * @retval 无
  */
void Module_KEY_Config(void)
{
    Module_InitKeyVar(); /* 1. 初始化按键状态变量和FIFO */
    BSP_GPIO_KEY_Init(); /* 2. 初始化按键的GPIO硬件 */
    key_gpio_list = BSP_GPIO_KEY_GetHandle(); /* 3. 获取GPIO配置句柄 */
}

/**
  * @brief  检查指定按键的GPIO引脚是否处于激活电平。
  * @param  key_id: 要检查的按键ID。
  * @retval 1: 引脚处于激活电平 (按下)。
  * @retval 0: 引脚处于非激活电平 (弹起)。
  */
static uint8_t Moudle_Key_IsPinActive(uint8_t key_id)
{
    uint8_t pin_level;
    
    if ((key_gpio_list[key_id].GPIO_PORT->IDR & key_gpio_list[key_id].GPIO_Pin) == 0)
    {
        pin_level = 0;
    }
    else
    {
        pin_level = 1;
    }

    if (pin_level == key_gpio_list[key_id].ActiveLevel)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
  * @brief  判断一个按键是否被按下（区分单键和组合键）。
  * @note   此函数用于确保只有单个按键按下时才被识别为单键事件，避免组合键的干扰。
  * @param  key_id: 要判断的按键ID。
  * @retval 1: 按键被有效按下。
  * @retval 0: 按键未按下或有多个键同时按下。
  */
static uint8_t Moudle_Key_IsPressed(uint8_t key_id)
{
    /* 实体单键 */
    if (key_id < HARD_KEY_NUM)
    {
        uint8_t i;
        uint8_t pressed_count = 0;
        uint8_t saved_key_id = 255;
        
        /* 遍历所有硬件按键，统计被按下的数量 */
        for (i = 0; i < HARD_KEY_NUM; i++)
        {
            if (Moudle_Key_IsPinActive(i)) 
            {
                pressed_count++;
                saved_key_id = i;
            }
        }
        
        /* 只有当且仅当一个按键被按下，且该按键就是我们关心的那个按键时，才返回1 */
        if (pressed_count == 1 && saved_key_id == key_id)
        {
            return 1;
        }		

        return 0;
    }
    
    // /* 组合键 K1K2 (示例) */
    // if (key_id == HARD_KEY_NUM + 0)
    // {
    //     if (Moudle_Key_IsPinActive(KID_K1) && Moudle_Key_IsPinActive(KID_K2))
    //     {
    //         return 1;
    //     }
    //     else
    //     {
    //         return 0;
    //     }
    // }

    return 0;
}

/**
  * @brief  将一个键值放入按键FIFO缓冲区。
  * @note   可用于在程序中模拟一个按键事件。
  * @param  key_code: 要放入的按键事件代码。
  * @retval 无
  */
void Moudle_Key_PutFifoBuffer(uint8_t key_code)
{
    s_key_fifo.Buf[s_key_fifo.Write] = key_code;

    if (++s_key_fifo.Write  >= KEY_FIFO_SIZE)
    {
        s_key_fifo.Write = 0;
    }
}

/**
  * @brief  从按键FIFO缓冲区读取一个键值。
  * @param  无
  * @retval 返回队首的按键事件代码，如果FIFO为空则返回 KEY_NONE。
  */
uint8_t Moudle_Key_GetFifoBuffer(void)
{
    uint8_t key_code;

    if (s_key_fifo.Read == s_key_fifo.Write)
    {
        return KEY_NONE;
    }
    else
    {
        key_code = s_key_fifo.Buf[s_key_fifo.Read];

        if (++s_key_fifo.Read >= KEY_FIFO_SIZE)
        {
            s_key_fifo.Read = 0;
        }
        return key_code;
    }
}

/**
  * @brief  获取指定按键的当前物理状态（按下或弹起）。
  * @param  key_id: 要查询的按键ID。
  * @retval 1: 按键当前处于按下状态。
  * @retval 0: 按键当前处于弹起状态。
  */
uint8_t Moudle_Key_GetState(KeyId_e key_id)
{
    return s_key_states[key_id].State;
}

/**
  * @brief  设置指定按键的长按和连发参数。
  * @param  key_id: 要设置的按键ID。
  * @param  long_time: 长按时间阈值 (单位: 10ms)。设置为0则禁用长按检测。
  * @param  repeat_speed: 连发速度 (单位: 10ms)。设置为0则禁用连发。
  * @retval 无
  */
void Moudle_Key_SetParam(uint8_t key_id, uint16_t long_time, uint8_t repeat_speed)
{
    s_key_states[key_id].LongTime = long_time;
    s_key_states[key_id].RepeatSpeed = repeat_speed;
    s_key_states[key_id].RepeatCount = 0;
}

/**
  * @brief  清空按键FIFO缓冲区中所有未读的事件。
  * @param  无
  * @retval 无
  */
void Moudle_Key_Clear(void)
{
    s_key_fifo.Read = s_key_fifo.Write;
}

/**
  * @brief  检测单个按键的状态机，包含软件滤波。
  * @note   此函数为非阻塞式，必须被周期性地调用以更新按键状态。
  * @param  key_id: 要检测的按键ID。
  * @retval 无
  */
static void Moudle_Key_Detect(uint8_t key_id)
{
    KeyState_t *p_key;

    p_key = &s_key_states[key_id];
    
    /* --- 按键按下的处理流程 --- */
    if (Moudle_Key_IsPressed(key_id))
    {
        /* 软件消抖: 只有当按键状态持续一段时间后才确认 */
        if (p_key->Count < KEY_FILTER_TIME)
        {
            p_key->Count = KEY_FILTER_TIME;
        }
        else if(p_key->Count < 2 * KEY_FILTER_TIME)
        {
            p_key->Count++;
        }
        else
        {
            /* 确认按键有效按下 */
            if (p_key->State == 0)
            {
                p_key->State = 1;
                /* 产生“按下”事件，并放入FIFO */
                Moudle_Key_PutFifoBuffer((uint8_t)(3 * key_id + 1));
            }

            /* 长按和连发检测 */
            if (p_key->LongTime > 0)
            {
                if (p_key->LongCount < p_key->LongTime)
                {
                    /* 累加长按计数器，直到达到长按阈值 */
                    if (++p_key->LongCount == p_key->LongTime)
                    {
                        /* 产生“长按”事件，并放入FIFO */
                        Moudle_Key_PutFifoBuffer((uint8_t)(3 * key_id + 3));
                    }
                }
                else /* 已达到长按时间，开始处理连发 */
                {
                    if (p_key->RepeatSpeed > 0)
                    {
                        if (++p_key->RepeatCount >= p_key->RepeatSpeed)
                        {
                            p_key->RepeatCount = 0;
                            /* 产生“连发”事件 (复用“按下”事件码)，并放入FIFO */
                            Moudle_Key_PutFifoBuffer((uint8_t)(3 * key_id + 1));
                        }
                    }
                }
            }
        }
    }
    /* --- 按键弹起的处理流程 --- */
    else
    {
        /* 软件消抖 */
        if(p_key->Count > KEY_FILTER_TIME)
        {
            p_key->Count = KEY_FILTER_TIME;
        }
        else if(p_key->Count != 0)
        {
            p_key->Count--;
        }
        else
        {
            /* 确认按键有效弹起 */
            if (p_key->State == 1)
            {
                p_key->State = 0;
                /* 产生“弹起”事件，并放入FIFO */
                Moudle_Key_PutFifoBuffer((uint8_t)(3 * key_id + 2));
            }
        }

        /* 只要按键弹起，就复位长按和连发计数器 */
        p_key->LongCount = 0;
        p_key->RepeatCount = 0;
    }
}

/**
  * @brief  快速检测单个按键的状态机，无软件滤波。
  * @note   适用于需要快速响应的场景，但对抗抖动能力较弱。
  * @param  key_id: 要检测的按键ID。
  * @retval 无
  */
static void Moudle_Key_DetectFastIO(uint8_t key_id)
{
    KeyState_t *p_key;

    p_key = &s_key_states[key_id];
    
    if (Moudle_Key_IsPressed(key_id))
    {
        if (p_key->State == 0)
        {
            p_key->State = 1;
            Moudle_Key_PutFifoBuffer((uint8_t)(3 * key_id + 1));
        }

        if (p_key->LongTime > 0)
        {
            if (p_key->LongCount < p_key->LongTime)
            {
                if (++p_key->LongCount == p_key->LongTime)
                {
                    Moudle_Key_PutFifoBuffer((uint8_t)(3 * key_id + 3));
                }
            }
            else
            {
                if (p_key->RepeatSpeed > 0)
                {
                    if (++p_key->RepeatCount >= p_key->RepeatSpeed)
                    {
                        p_key->RepeatCount = 0;
                        Moudle_Key_PutFifoBuffer((uint8_t)(3 * key_id + 1));
                    }
                }
            }
        }
    }
    else
    {
        if (p_key->State == 1)
        {
            p_key->State = 0;
            Moudle_Key_PutFifoBuffer((uint8_t)(3 * key_id + 2));
        }

        p_key->LongCount = 0;
        p_key->RepeatCount = 0;
    }
}

/**
  * @brief  扫描所有按键（带滤波）。
  * @note   此函数应由一个定时器（如SysTick）每10ms调用一次。
  * @param  无
  * @retval 无
  */
void Moudle_Key_Scan10ms(void)
{
    uint8_t i;

    for (i = 0; i < KEY_COUNT; i++)
    {
        Moudle_Key_Detect(i);
    }
}

/**
  * @brief  快速扫描所有按键（无滤波）。
  * @note   此函数应由一个定时器（如SysTick）每1ms调用一次。
  * @param  无
  * @retval 无
  */
void Moudle_Key_Scan1ms(void)
{
    uint8_t i;

    for (i = 0; i < KEY_COUNT; i++)
    {
        Moudle_Key_DetectFastIO(i);
    }
}
