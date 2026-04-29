/**
 * @file   App_ReceiveUpper_Example.c
 * @brief  优化后的 USART1 接收和解析数据示例代码
 * @note   新协议格式: "ceio:%d,%f,%f,%f,...\n"
 *         - %d: 模式标识符 (1~16 字节)
 *         - %f: 浮点数数据（支持 1~16 个，灵活扩展）
 * 
 * @date   2026-04-29
 */

#include "Module_ReceiveUpper.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* ========== 新协议格式说明 ========== */
/*
 * 协议格式: "ceio:%d,%f,%f,%f,...\n"
 * 
 * 示例 1 (6个浮点数 + 模式 0):
 *   ceio:0,0.5,1.5,2.5,3.5,4.5,5.5\n
 * 
 * 示例 2 (3个浮点数 + 模式 1):
 *   ceio:1,10.0,20.0,30.0\n
 * 
 * 示例 3 (12个浮点数 + 模式 2，可扩展):
 *   ceio:2,1.0,2.0,3.0,...,12.0\n
 * 
 * 特点：
 * - 第一个 %d 是模式标识符（对应 Serial_SendPacket 的第一个参数）
 * - 后续 %f 个数不固定，自动检测
 * - 支持 1~16 个浮点数（USART1_MAX_FLOAT_DATA）
 * - 未来可轻松增加浮点数个数，无需修改解析函数
 */

/* ========== 方案 A：简单轮询方式 ========== */

/**
 * @brief  简单轮询接收和解析数据（新协议）
 */
void App_ReceiveUpper_Simple_Task(void *argument)
{
    USART1_RxData_t rx_data = {0};
    
    printf("[ReceiveUpper] 任务启动，等待数据...\r\n");
    printf("[ReceiveUpper] 期望格式: ceio:MODE,f1,f2,f3,...\r\n");
    
    while (1)
    {
        // 尝试解析接收缓冲区中的数据
        BSP_Status_t status = Module_USART1_ParseData(&rx_data);
        
        if (status == BSP_OK)
        {
            // ========== 数据解析成功 ==========
            printf("[ReceiveUpper] 收到完整数据包:\r\n");
            printf("  Mode: %lu\r\n", rx_data.mode);
            printf("  Float Count: %lu\r\n", rx_data.float_count);
            printf("  Float Data:\r\n");
            
            // 遍历所有接收到的浮点数
            for (uint32_t i = 0; i < rx_data.float_count; i++)
            {
                printf("    [%lu] = %.6f\r\n", i, rx_data.float_data[i]);
            }
            printf("---\r\n");
            
            // 在这里可以根据不同的模式处理数据
            switch (rx_data.mode)
            {
                case 0:
                    printf("[Mode 0] 标准模式，处理 6 个浮点数\r\n");
                    // data_converter[0~5] = float_data[0~5]
                    break;
                
                case 1:
                    printf("[Mode 1] 快速模式，处理 3 个浮点数\r\n");
                    // 处理 3 个关键参数
                    break;
                
                case 2:
                    printf("[Mode 2] 扩展模式，处理 %lu 个浮点数\r\n", rx_data.float_count);
                    // 处理可变数量的数据
                    break;
                
                default:
                    printf("[Mode Unknown] 未知模式\r\n");
                    break;
            }
        }
        else if (status == BSP_BUSY)
        {
            // 数据还不完整，继续等待
            // 不打印消息，避免频繁输出
        }
        else if (status == BSP_ERROR)
        {
            // 数据格式错误
            printf("[ReceiveUpper] 数据格式错误，等待下一个数据包\r\n");
        }
        
        // 每 10ms 查询一次
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ========== 方案 B：使用 FreeRTOS 队列（推荐）========== */

// 全局队列句柄
QueueHandle_t xReceiveUpperQueue = NULL;

/**
 * @brief  创建并初始化接收数据队列
 */
void App_ReceiveUpper_Queue_Create(void)
{
    xReceiveUpperQueue = xQueueCreate(10, sizeof(USART1_RxData_t));
    if (xReceiveUpperQueue == NULL)
    {
        printf("[ERROR] 接收数据队列创建失败\r\n");
    }
    else
    {
        printf("[OK] 接收数据队列创建成功\r\n");
    }
}

/**
 * @brief  接收数据检测任务（周期性调用解析函数）
 */
void App_ReceiveUpper_Detect_Task(void *argument)
{
    USART1_RxData_t rx_data = {0};
    
    printf("[ReceiveDetect] 任务启动\r\n");
    
    while (1)
    {
        // 尝试解析接收缓冲区中的数据
        if (Module_USART1_ParseData(&rx_data) == BSP_OK)
        {
            // 将完整的数据包发送到队列
            if (xQueueSend(xReceiveUpperQueue, &rx_data, 0) != pdTRUE)
            {
                printf("[WARNING] 接收数据队列满\r\n");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/**
 * @brief  数据处理任务（阻塞等待接收数据）
 */
void App_ReceiveUpper_Process_Task(void *argument)
{
    USART1_RxData_t rx_data = {0};
    uint32_t process_count = 0;
    
    if (xReceiveUpperQueue == NULL)
    {
        printf("[ERROR] 接收数据队列未初始化\r\n");
        vTaskDelete(NULL);
    }
    
    printf("[ReceiveProcess] 任务启动，等待数据...\r\n");
    
    while (1)
    {
        // 阻塞等待接收数据
        if (xQueueReceive(xReceiveUpperQueue, &rx_data, portMAX_DELAY) == pdTRUE)
        {
            process_count++;
            
            printf("[ReceiveProcess] 处理数据包 #%lu:\r\n", process_count);
            printf("  Mode: %lu\r\n", rx_data.mode);
            printf("  Float Count: %lu\r\n", rx_data.float_count);
            printf("  Float Data:\r\n");
            
            for (uint32_t i = 0; i < rx_data.float_count; i++)
            {
                printf("    [%lu] = %.6f\r\n", i, rx_data.float_data[i]);
            }
            printf("---\r\n");
            
            // 根据模式和数据进行处理
            // 示例：更新参数结构体
            if (rx_data.mode == 0 && rx_data.float_count >= 6)
            {
                // 对应 Serial_SendPacket 的参数更新
                // data_converter[0] = rx_data.float_data[0]
                // data_converter[1] = rx_data.float_data[1]
                // ...
                printf("[Processing] 更新 Mode 0 的 6 个参数\r\n");
            }
        }
    }
}

/* ========== 应用初始化函数 ========== */

/**
 * @brief  初始化所有接收上位机数据相关资源
 */
void App_ReceiveUpper_Init(void)
{
    // 方案 B：队列方式
    App_ReceiveUpper_Queue_Create();
    
    printf("[OK] 接收上位机数据模块初始化完成\r\n");
}

/**
 * @brief  创建接收上位机数据任务
 * @param  scheme: 0=轮询, 1=队列
 */
void App_ReceiveUpper_TasksCreate(uint8_t scheme)
{
    if (scheme == 0)
    {
        // 方案 A：简单轮询
        xTaskCreate(App_ReceiveUpper_Simple_Task,
                    "ReceiveUpper_Simple",
                    512,  // 栈大小较大，因为 printf 需要空间
                    NULL,
                    3,
                    NULL);
    }
    else if (scheme == 1)
    {
        // 方案 B：FreeRTOS 队列
        xTaskCreate(App_ReceiveUpper_Detect_Task,
                    "ReceiveUpper_Detect",
                    256,
                    NULL,
                    3,
                    NULL);
        
        xTaskCreate(App_ReceiveUpper_Process_Task,
                    "ReceiveUpper_Process",
                    512,
                    NULL,
                    2,
                    NULL);
    }
}

/* ========== 测试和调试函数 ========== */

/**
 * @brief  测试：模拟上位机发送 Mode 0 数据（6个浮点数）
 */
void App_Test_SendMode0(void)
{
    printf("ceio:0,1.5,2.5,3.5,4.5,5.5,6.5\r\n");
}

/**
 * @brief  测试：模拟上位机发送 Mode 1 数据（3个浮点数）
 */
void App_Test_SendMode1(void)
{
    printf("ceio:1,10.0,20.0,30.0\r\n");
}

/**
 * @brief  测试：模拟上位机发送 Mode 2 数据（8个浮点数，展示扩展性）
 */
void App_Test_SendMode2(void)
{
    printf("ceio:2,1.1,2.2,3.3,4.4,5.5,6.6,7.7,8.8\r\n");
}

/**
 * @brief  本地测试解析函数
 */
void App_Test_ParseData(void)
{
    // 预填充缓冲区（模拟接收数据）
    extern uint8_t usart1_rx_buffer[];
    
    printf("========== 本地测试解析函数 ==========\r\n");
    
    // 测试 1：Mode 0 + 6 个浮点数
    strcpy((char *)usart1_rx_buffer, "ceio:0,1.5,2.5,3.5,4.5,5.5,6.5\n");
    USART1_RxData_t rx_data = {0};
    
    if (Module_USART1_ParseData(&rx_data) == BSP_OK)
    {
        printf("[TEST 1] Mode 0 + 6 floats - 解析成功:\r\n");
        printf("  Mode: %lu, Count: %lu\r\n", rx_data.mode, rx_data.float_count);
        for (uint32_t i = 0; i < rx_data.float_count; i++)
        {
            printf("    [%lu] = %f\r\n", i, rx_data.float_data[i]);
        }
    }
    else
    {
        printf("[TEST 1] 解析失败\r\n");
    }
    
    // 测试 2：Mode 1 + 3 个浮点数
    strcpy((char *)usart1_rx_buffer, "ceio:1,10.0,20.0,30.0\n");
    memset(&rx_data, 0, sizeof(rx_data));
    
    if (Module_USART1_ParseData(&rx_data) == BSP_OK)
    {
        printf("[TEST 2] Mode 1 + 3 floats - 解析成功:\r\n");
        printf("  Mode: %lu, Count: %lu\r\n", rx_data.mode, rx_data.float_count);
        for (uint32_t i = 0; i < rx_data.float_count; i++)
        {
            printf("    [%lu] = %f\r\n", i, rx_data.float_data[i]);
        }
    }
    else
    {
        printf("[TEST 2] 解析失败\r\n");
    }
    
    // 测试 3：Mode 2 + 8 个浮点数（展示可扩展性）
    strcpy((char *)usart1_rx_buffer, "ceio:2,1.1,2.2,3.3,4.4,5.5,6.6,7.7,8.8\n");
    memset(&rx_data, 0, sizeof(rx_data));
    
    if (Module_USART1_ParseData(&rx_data) == BSP_OK)
    {
        printf("[TEST 3] Mode 2 + 8 floats - 解析成功:\r\n");
        printf("  Mode: %lu, Count: %lu\r\n", rx_data.mode, rx_data.float_count);
        for (uint32_t i = 0; i < rx_data.float_count; i++)
        {
            printf("    [%lu] = %f\r\n", i, rx_data.float_data[i]);
        }
    }
    else
    {
        printf("[TEST 3] 解析失败\r\n");
    }
    
    printf("========== 本地测试完成 ==========\r\n");
}

/* ========== 在 App_TasksInit.c 中的使用示例 ========== */

/*
// 在 App_Init() 中
void App_Init(void)
{
    // ... 其他初始化代码 ...
    
    // 初始化接收上位机数据模块
    App_ReceiveUpper_Init();
    
    // ... 其他初始化代码 ...
}

// 在 App_TasksInit() 中
void App_Tasks_Init(void)
{
    // 创建接收上位机数据任务（方案 B：队列方式）
    App_ReceiveUpper_TasksCreate(1);
    
    // ... 创建其他任务 ...
}
*/
