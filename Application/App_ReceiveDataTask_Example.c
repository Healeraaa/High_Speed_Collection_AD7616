/**
 * @file   App_ReceiveDataTask_Example.c
 * @brief  USART1 接收和解析数据示例代码
 * @note   展示如何接收上位机发送的数据包并解析出 6 个浮点数
 * 
 * 数据格式：ceio:%f,%f,%f,%f,%f,%f\n
 */

#include "Module_ReceiveUpper.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* ========== 方案 A：简单轮询方式（最简单） ========== */

/**
 * @brief  简单轮询接收和解析数据
 * @note   定期调用解析函数，处理接收到的数据
 */
void App_ReceiveData_Simple_Task(void *argument)
{
    USART1_RxData_t rx_data = {0};
    float *p_data = (float *)rx_data.data;
    
    printf("[ReceiveData] 任务启动，等待数据...\r\n");
    
    while (1)
    {
        // 尝试解析接收缓冲区中的数据
        BSP_Status_t status = Module_USART1_ParseData(&rx_data);
        
        if (status == BSP_OK)
        {
            // ========== 数据解析成功 ==========
            printf("[ReceiveData] 收到完整数据包:\r\n");
            printf("  CH0: %.6f\r\n", p_data[0]);
            printf("  CH1: %.6f\r\n", p_data[1]);
            printf("  CH2: %.6f\r\n", p_data[2]);
            printf("  CH3: %.6f\r\n", p_data[3]);
            printf("  CH4: %.6f\r\n", p_data[4]);
            printf("  CH5: %.6f\r\n", p_data[5]);
            printf("---\r\n");
            
            // 在这里可以对数据进行进一步处理
            // 例如：更新 DAC 输出、控制设备等
        }
        else if (status == BSP_BUSY)
        {
            // 数据还不完整，继续等待
            // 不打印消息，避免频繁输出
        }
        else if (status == BSP_ERROR)
        {
            // 数据格式错误
            printf("[ReceiveData] 数据格式错误，等待下一个数据包\r\n");
        }
        
        // 每 10ms 查询一次
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ========== 方案 B：使用 FreeRTOS 队列（推荐高频处理） ========== */

// 全局队列句柄
QueueHandle_t xReceiveDataQueue = NULL;

/**
 * @brief  创建并初始化接收数据队列
 * @note   在应用初始化时调用（App_Init() 中）
 */
void App_ReceiveDataQueue_Create(void)
{
    xReceiveDataQueue = xQueueCreate(10, sizeof(USART1_RxData_t));
    if (xReceiveDataQueue == NULL)
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
 * @note   优先级应该高于数据处理任务
 */
void App_ReceiveData_Detect_Task(void *argument)
{
    USART1_RxData_t rx_data = {0};
    
    printf("[ReceiveDetect] 任务启动\r\n");
    
    while (1)
    {
        // 尝试解析接收缓冲区中的数据
        if (Module_USART1_ParseData(&rx_data) == BSP_OK)
        {
            // 将完整的数据包发送到队列
            if (xQueueSend(xReceiveDataQueue, &rx_data, 0) != pdTRUE)
            {
                printf("[WARNING] 接收数据队列满\r\n");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));  // 每 5ms 检查一次
    }
}

/**
 * @brief  数据处理任务（阻塞等待接收数据）
 * @note   接收到完整数据包后立即处理，无需轮询
 */
void App_ReceiveData_Process_Task(void *argument)
{
    USART1_RxData_t rx_data = {0};
    float *p_data = (float *)rx_data.data;
    uint32_t process_count = 0;
    
    if (xReceiveDataQueue == NULL)
    {
        printf("[ERROR] 接收数据队列未初始化\r\n");
        vTaskDelete(NULL);
    }
    
    printf("[ReceiveProcess] 任务启动，等待数据...\r\n");
    
    while (1)
    {
        // 阻塞等待接收数据（无数据时不消耗 CPU）
        if (xQueueReceive(xReceiveDataQueue, &rx_data, portMAX_DELAY) == pdTRUE)
        {
            process_count++;
            
            printf("[ReceiveProcess] 处理数据包 #%lu:\r\n", process_count);
            printf("  数据个数: %lu\r\n", rx_data.count);
            printf("  数据有效: %s\r\n", rx_data.is_valid ? "YES" : "NO");
            printf("  数据值:\r\n");
            printf("    [0] = %.6f\r\n", p_data[0]);
            printf("    [1] = %.6f\r\n", p_data[1]);
            printf("    [2] = %.6f\r\n", p_data[2]);
            printf("    [3] = %.6f\r\n", p_data[3]);
            printf("    [4] = %.6f\r\n", p_data[4]);
            printf("    [5] = %.6f\r\n", p_data[5]);
            printf("---\r\n");
            
            // 在这里处理数据
            // 例如：
            // - 控制 DAC 输出
            // - 调节 PID 参数
            // - 更新显示
            // - 发送到其他模块
        }
    }
}

/* ========== 方案 C：使用事件组（用于等待多个事件） ========== */

#include "event_groups.h"

EventGroupHandle_t xDataEventGroup = NULL;
#define DATA_RECEIVED_BIT  (1 << 0)

/**
 * @brief  初始化事件组
 */
void App_ReceiveData_EventGroup_Init(void)
{
    xDataEventGroup = xEventGroupCreate();
    if (xDataEventGroup == NULL)
    {
        printf("[ERROR] 数据事件组创建失败\r\n");
    }
}

/**
 * @brief  数据检测任务（使用事件组）
 */
void App_ReceiveData_Detect_EventGroup_Task(void *argument)
{
    USART1_RxData_t rx_data = {0};
    
    while (1)
    {
        if (Module_USART1_ParseData(&rx_data) == BSP_OK)
        {
            // 保存数据到全局变量（需要互斥锁保护）
            // g_rx_data = rx_data;
            
            // 设置事件标志
            xEventGroupSetBits(xDataEventGroup, DATA_RECEIVED_BIT);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/**
 * @brief  数据处理任务（等待事件组）
 */
void App_ReceiveData_Process_EventGroup_Task(void *argument)
{
    EventBits_t event_bits;
    
    printf("[ReceiveProcess_EventGroup] 任务启动\r\n");
    
    while (1)
    {
        // 等待数据接收事件
        event_bits = xEventGroupWaitBits(
            xDataEventGroup,
            DATA_RECEIVED_BIT,
            pdTRUE,              // 等待后清除标志
            pdFALSE,             // 不等待所有位
            portMAX_DELAY
        );
        
        if (event_bits & DATA_RECEIVED_BIT)
        {
            printf("[ReceiveProcess_EventGroup] 收到数据事件\r\n");
            // 处理数据
            // printf("  数据: %f, %f, %f, %f, %f, %f\r\n", ...)
        }
    }
}

/* ========== 应用初始化函数 ========== */

/**
 * @brief  初始化所有接收数据相关资源
 * @note   在 App_Init() 中调用
 */
void App_ReceiveData_Init(void)
{
    // 方案 B：队列方式
    App_ReceiveDataQueue_Create();
    
    // 方案 C：事件组方式
    App_ReceiveData_EventGroup_Init();
    
    printf("[OK] 接收数据模块初始化完成\r\n");
}

/**
 * @brief  创建接收数据任务
 * @note   在 App_TasksInit() 中调用（使用 xTaskCreate）
 * @param  方案选择：A(轮询), B(队列), 或 C(事件组)
 */
void App_ReceiveData_TasksCreate(uint8_t scheme)
{
    if (scheme == 0)
    {
        // 方案 A：简单轮询
        xTaskCreate(App_ReceiveData_Simple_Task,
                    "ReceiveData_Simple",
                    256,
                    NULL,
                    3,
                    NULL);
    }
    else if (scheme == 1)
    {
        // 方案 B：FreeRTOS 队列
        xTaskCreate(App_ReceiveData_Detect_Task,
                    "ReceiveData_Detect",
                    256,
                    NULL,
                    3,
                    NULL);
        
        xTaskCreate(App_ReceiveData_Process_Task,
                    "ReceiveData_Process",
                    256,
                    NULL,
                    2,
                    NULL);
    }
    else if (scheme == 2)
    {
        // 方案 C：事件组
        xTaskCreate(App_ReceiveData_Detect_EventGroup_Task,
                    "ReceiveData_Detect_EG",
                    256,
                    NULL,
                    3,
                    NULL);
        
        xTaskCreate(App_ReceiveData_Process_EventGroup_Task,
                    "ReceiveData_Process_EG",
                    256,
                    NULL,
                    2,
                    NULL);
    }
}

/* ========== 在 App_TasksInit.c 中的使用示例 ========== */

/**
 * @brief  完整的应用初始化流程
 * @note   在 main() → BSP_Init() → Module_Config() → App_Init() 中按顺序调用
 */

/*
// 在 App_Init() 中
void App_Init(void)
{
    // ... 其他初始化代码 ...
    
    // 初始化接收数据模块
    App_ReceiveData_Init();
    
    // ... 其他初始化代码 ...
}

// 在 App_TasksInit() 中
void App_Tasks_Init(void)
{
    // 创建接收数据任务（选择方案 B：队列方式）
    App_ReceiveData_TasksCreate(1);
    
    // ... 创建其他任务 ...
}
*/

/* ========== 测试函数 ========== */

/**
 * @brief  模拟上位机发送数据（用于本地测试）
 */
void App_Test_SendData(void)
{
    printf("ceio:1.234,2.345,3.456,4.567,5.678,6.789\r\n");
}

/**
 * @brief  测试解析函数
 */
void App_Test_ParseData(void)
{
    // 预填充缓冲区（模拟接收数据）
    extern uint8_t usart1_rx_buffer[];
    strcpy((char *)usart1_rx_buffer, "ceio:1.0,2.0,3.0,4.0,5.0,6.0\n");
    
    USART1_RxData_t rx_data = {0};
    float *p_data = (float *)rx_data.data;
    
    if (Module_USART1_ParseData(&rx_data) == BSP_OK)
    {
        printf("[TEST] 解析成功:\r\n");
        for (int i = 0; i < 6; i++)
        {
            printf("  [%d] = %f\r\n", i, p_data[i]);
        }
    }
    else
    {
        printf("[TEST] 解析失败\r\n");
    }
}
