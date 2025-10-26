#ifndef __MODULE_KEY_H__
#define __MODULE_KEY_H__

#include "stdint.h"

/* 按键ID, 用于bsp_GetKeyState()等函数的入口参数 */
typedef enum
{
	
    KID_K1 = 0,
    KID_K2,
    KID_K3,
    KID_JOY_U,
    KID_JOY_D,
    KID_JOY_L,
    KID_JOY_R,
    KID_JOY_OK
} KeyId_e;

/*
    按键滤波时间50ms, 单位10ms。
    只有连续检测到50ms状态不变才认为有效，包括弹起和按下两种事件。
*/
#define KEY_FILTER_TIME   5
#define KEY_LONG_TIME     100			/* 单位10ms， 持续1秒，认为长按事件 */

/*
    每个按键对应1个全局的结构体变量，用于维护按键状态。
*/
typedef struct
{
    uint8_t  Count;			/* 滤波器计数器 */
    uint16_t LongCount;		/* 长按计数器 */
    uint16_t LongTime;		/* 按键按下持续时间, 0表示不检测长按 */
    uint8_t  State;			/* 按键当前状态（0:弹起, 1:按下） */
    uint8_t  RepeatSpeed;	/* 连发速度（单位: 10ms） */
    uint8_t  RepeatCount;	/* 连发计数器 */
} KeyState_t;

/*
    定义键值代码, 必须按如下次序定义每个键的按下、弹起和长按事件。
*/
typedef enum
{
    KEY_NONE = 0,			/* 0 表示无按键事件 */

    KEY_1_DOWN,				/* 1键按下 */
    KEY_1_UP,				/* 1键弹起 */
    KEY_1_LONG,				/* 1键长按 */

    KEY_2_DOWN,				/* 2键按下 */
    KEY_2_UP,				/* 2键弹起 */
    KEY_2_LONG,				/* 2键长按 */

    KEY_3_DOWN,				/* 3键按下 */
    KEY_3_UP,				/* 3键弹起 */
    KEY_3_LONG,				/* 3键长按 */

    KEY_4_DOWN,				/* 4键按下 */
    KEY_4_UP,				/* 4键弹起 */
    KEY_4_LONG,				/* 4键长按 */

    KEY_5_DOWN,				/* 5键按下 */
    KEY_5_UP,				/* 5键弹起 */
    KEY_5_LONG,				/* 5键长按 */

    KEY_6_DOWN,				/* 6键按下 */
    KEY_6_UP,				/* 6键弹起 */
    KEY_6_LONG,				/* 6键长按 */

    KEY_7_DOWN,				/* 7键按下 */
    KEY_7_UP,				/* 7键弹起 */
    KEY_7_LONG,				/* 7键长按 */

    KEY_8_DOWN,				/* 8键按下 */
    KEY_8_UP,				/* 8键弹起 */
    KEY_8_LONG,				/* 8键长按 */

    /* 组合键 */
    KEY_9_DOWN,				/* 9键按下 */
    KEY_9_UP,				/* 9键弹起 */
    KEY_9_LONG,				/* 9键长按 */

    KEY_10_DOWN,			/* 10键按下 */
    KEY_10_UP,				/* 10键弹起 */
    KEY_10_LONG,			/* 10键长按 */
} KeyEvent_e;

/* 按键FIFO用到变量 */
#define KEY_FIFO_SIZE	10
typedef struct
{
	uint8_t Buf[KEY_FIFO_SIZE];		/* 键值缓冲区 */
	uint8_t Read;					/* 缓冲区读指针1 */
	uint8_t Write;					/* 缓冲区写指针 */
} KeyFifo_t;

/* 按键模块接口函数声明 */
void Module_KEY_Config(void);
uint8_t Moudle_Key_GetState(KeyId_e key_id);
void Moudle_Key_PutFifoBuffer(uint8_t key_code);
uint8_t Moudle_Key_GetFifoBuffer(void);
void Moudle_Key_SetParam(uint8_t key_id, uint16_t long_time, uint8_t repeat_speed);
void Moudle_Key_Clear(void);
void Moudle_Key_Scan10ms(void);

#endif 

