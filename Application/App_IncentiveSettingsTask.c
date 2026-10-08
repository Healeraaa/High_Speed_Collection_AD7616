#include "App_IncentiveSettingsTask.h"
#include "App_TasksInit.h"
#include "App_CurveFit.h"
#include "Module.h"
#include "Module_ReceiveUpper.h"
#include "Module_IncentiveConfig.h"
#include "main.h"
#include "bsp.h"
#include "stdio.h"

void App_IncentiveSettingsTask(void *argument)
{
    USART1_RxData_t rx_data = {0};

    /* 当前任务不使用传入参数，防止产生未使用参数警告。 */
    (void)argument;

    /* 注册上电后的默认电流拟合曲线，只需要执行一次。 */
    Module_IncentiveConfig_Init();

    /* 本工程的GPCI由若干次CA事件组合实现。 */
    Module_IncentiveConfig_SetGPCIImplementation(
        GPCI_IMPLEMENTATION_BY_CA);

    while (1)
    {
        /*
         * 上位机连续发送完整的 ceiod 和 ceiof 后，
         * Module_USART1_IsDataReady() 才会返回非零。
         */
        if (Module_USART1_IsDataReady())
        {
            /*
             * 将两行文本解析到 rx_data：
             *
             * rx_data.mode        = 模式
             * rx_data.int_data[]  = 通道和三级增益
             * rx_data.float_data[]= 六个模式参数
             */
            if (Module_USART1_ParseData(&rx_data) == Module_OK)
            {
                /*
                 * 新模块会完成：
                 * 1. 检查参数数量和编号；
                 * 2. 根据模式整理六个参数；
                 * 3. 选择对应拟合曲线；
                 * 4. 生成并发送411数据包。
                 *
                 * 非法配置会返回 Module_ERROR，并且不会发送。
                 */
                (void)Module_IncentiveConfig_Handle(&rx_data);
            }
        }

        /*
         * 最多等待10ms检查一次上位机数据；如果PE1下降沿到来，
         * 任务会立即被唤醒，并在任务环境中发送下一次CA。
         */
        (void)Module_IncentiveConfig_ProcessGPCI(pdMS_TO_TICKS(10));
    }
}
