#ifndef __MODULE_INCENTIVE_CONFIG_H__
#define __MODULE_INCENTIVE_CONFIG_H__

#include "Module_ReceiveUpper.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * 上位机发送的测试模式编号。
 * 该编号最终会作为 USART3 数据包的 command 字段发送给下位机。
 */
typedef enum
{
    INCENTIVE_MODE_CV   = 0,
    INCENTIVE_MODE_DPV  = 1,
    INCENTIVE_MODE_CA   = 2,
    INCENTIVE_MODE_GPCI = 3
} Incentive_Mode_TypeDef;

/*
 * GPCI有两种实现方式：
 *
 * NATIVE：保持原来的处理方式，把GPCI命令直接交给底层执行。
 * BY_CA ：把一次GPCI转换成一个CA事件，并由本模块控制循环次数。
 */
typedef enum
{
    GPCI_IMPLEMENTATION_NATIVE = 0,
    GPCI_IMPLEMENTATION_BY_CA  = 1
} GPCI_Implementation_TypeDef;

/**
 * @brief 初始化激励配置模块。
 *
 * 当前只负责注册上电后的默认电流拟合曲线：
 * IV 33 + 一级电压增益 1X + 二级电压增益 1X。
 *
 * 在 App_IncentiveSettingsTask 进入 while 循环之前调用一次即可。
 */
void Module_IncentiveConfig_Init(void);

/**
 * @brief 选择GPCI使用哪一种实现方式。
 *
 * @param implementation GPCI_IMPLEMENTATION_NATIVE 或
 *                       GPCI_IMPLEMENTATION_BY_CA。
 */
void Module_IncentiveConfig_SetGPCIImplementation(
    GPCI_Implementation_TypeDef implementation);

/**
 * @brief 将上位机的GPCI六参数转换为底层CA六参数。
 *
 * @param gpci_parameters 上位机发送的GPCI六参数。
 * @param ca_parameters   转换完成的CA六参数。
 * @param cycle_count     GPCI循环次数，转换成功后通过该指针返回。
 *
 * @retval Module_OK    转换成功。
 * @retval Module_ERROR 指针无效，或循环次数不是大于0的整数。
 */
Module_Status_t Module_IncentiveConfig_ConvertGPCIToCA(
    const float gpci_parameters[6],
    double ca_parameters[6],
    uint32_t *cycle_count);

/**
 * @brief 检查、整理并发送一组上位机配置。
 *
 * @param rx_data Module_USART1_ParseData() 解析成功的数据。
 *
 * @retval Module_OK    配置合法，并且已经通过 USART3 发送。
 * @retval Module_ERROR 配置不完整、模式无效、挡位无效或发送失败。
 *
 * 完整输入必须包含：
 *   ceiod:模式,通道,IV增益,电压增益1,电压增益2
 *   ceiof:参数1,参数2,参数3,参数4,参数5,参数6
 */
Module_Status_t Module_IncentiveConfig_Handle(const USART1_RxData_t *rx_data);

/**
 * @brief 在PE1下降沿中通知配置任务：本次CA已经执行完成。
 *
 * @param higher_priority_task_woken EXTI1_IRQHandler中的
 *                                   xHigherPriorityTaskWoken地址。
 *
 * @note 本函数可以在中断中调用，只发送任务通知，不发送串口。
 */
void Module_IncentiveConfig_NotifyCAFinishedFromISR(
    BaseType_t *higher_priority_task_woken);

/**
 * @brief 在普通任务中等待并处理一次CA完成通知。
 *
 * @param wait_ticks 最长等待时间，单位为FreeRTOS tick。
 *
 * @retval Module_OK    已处理一次完成通知。
 * @retval Module_BUSY  等待期间没有收到完成通知。
 * @retval Module_ERROR 发送下一次CA失败。
 *
 * @note 本函数不能在中断中调用。
 */
Module_Status_t Module_IncentiveConfig_ProcessGPCI(
    TickType_t wait_ticks);

#endif
