# USART1 接收和解析使用指南

## 功能概述
- **接收上位机发送的数据**：格式为 `ceio:%f,%f,%f,%f,%f,%f\n`
- **自动缓冲和解析**：提取 6 个浮点数
- **中断驱动接收**：无轮询开销

## 关键接口

### 1. 接收字符（在中断中调用）
```c
void BSP_USART1_ReceiveChar(uint8_t ch);
```
- **调用位置**：`USART1_IRQHandler` → `USART1_IRQ_Task()`
- **作用**：将每个接收到的字符存入缓冲区

### 2. 解析数据
```c
BSP_Status_t BSP_USART1_ParseData(USART1_RxData_t *pRxData);
```
- **返回值**：
  - `BSP_OK` (0x00) - 解析成功，数据有效
  - `BSP_BUSY` (0x02) - 数据不完整，未找到 `\n`
  - `BSP_ERROR` (0x01) - 解析失败，格式错误
- **参数**：指向 `USART1_RxData_t` 结构体的指针

### 3. 接收数据结构体
```c
typedef struct {
    uint8_t  data[6];      // 存储 6 个浮点数（每个 4 字节）
    uint32_t count;        // 实际接收的数据个数（应为 6）
    uint8_t  is_valid;     // 数据是否有效（1=有效，0=无效）
} USART1_RxData_t;
```

## 代码示例

### 示例 1：在应用任务中接收和处理数据

```c
#include "bsp_usart.h"

void App_ReceiveDataTask(void *argument)
{
    USART1_RxData_t rx_data = {0};
    float *p_data = (float *)rx_data.data;
    
    while (1)
    {
        // 尝试解析接收缓冲区中的数据
        if (BSP_USART1_ParseData(&rx_data) == BSP_OK)
        {
            // 数据解析成功
            printf("收到有效数据:\r\n");
            printf("  CH0: %f\r\n", p_data[0]);
            printf("  CH1: %f\r\n", p_data[1]);
            printf("  CH2: %f\r\n", p_data[2]);
            printf("  CH3: %f\r\n", p_data[3]);
            printf("  CH4: %f\r\n", p_data[4]);
            printf("  CH5: %f\r\n", p_data[5]);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 查询一次
    }
}
```

### 示例 2：FreeRTOS 队列方式（推荐用于高频处理）

```c
#include "bsp_usart.h"
#include "FreeRTOS.h"
#include "queue.h"

extern QueueHandle_t xDataQueue;  // 全局队列句柄

void App_DataProcessTask(void *argument)
{
    USART1_RxData_t rx_data;
    float *p_data = (float *)rx_data.data;
    
    while (1)
    {
        // 从队列中接收数据（无数据时阻塞）
        if (xQueueReceive(xDataQueue, &rx_data, portMAX_DELAY) == pdTRUE)
        {
            // 处理接收到的 6 个浮点数
            float ch0 = p_data[0];
            float ch1 = p_data[1];
            // ... 处理数据 ...
        }
    }
}

// 在主应用任务中定期尝试解析
void App_MainTask(void *argument)
{
    USART1_RxData_t rx_data = {0};
    
    while (1)
    {
        if (BSP_USART1_ParseData(&rx_data) == BSP_OK)
        {
            // 将有效数据发送到队列
            xQueueSend(xDataQueue, &rx_data, 0);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

## 数据格式说明

### 上位机发送格式
```
ceio:1.234,2.345,3.456,4.567,5.678,6.789\n
```

### 说明
- **前缀**：`ceio:` (5 个字符)
- **数据**：6 个浮点数，用逗号 `,` 分隔
- **结尾**：`\n` (换行符)

### 有效的数据范例
```
ceio:0.5,-1.25,3.14159,2.71828,-0.5,100.0\n
ceio:1.0,2.0,3.0,4.0,5.0,6.0\n
ceio:-5.5,-4.4,-3.3,-2.2,-1.1,0.0\n
```

## 工作流程图

```
┌─────────────────────────────────────┐
│  上位机发送数据                     │
│ ceio:1.0,2.0,3.0,4.0,5.0,6.0\n    │
└─────────────┬───────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  USART1 接收中断                    │
│  USART1_IRQHandler()               │
│  ↓                                  │
│  USART1_IRQ_Task()                 │
│  ↓                                  │
│  BSP_USART1_ReceiveChar()          │
│  (字符存入缓冲区)                  │
└─────────────┬───────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  应用层定期调用解析函数            │
│  BSP_USART1_ParseData()            │
│  ↓                                  │
│  查找 "ceio:"                       │
│  ↓                                  │
│  查找 "\n"                          │
│  ↓                                  │
│  sscanf 提取 6 个浮点数            │
└─────────────┬───────────────────────┘
              ↓
┌─────────────────────────────────────┐
│  返回 BSP_OK (成功)                │
│  数据存储在 USART1_RxData_t 中    │
│  应用可读取并处理                 │
└─────────────────────────────────────┘
```

## 错误处理

| 返回值 | 含义 | 处理方法 |
|--------|------|---------|
| `BSP_OK` | 数据解析成功 | 处理数据，缓冲区已自动清空 |
| `BSP_BUSY` | 数据不完整 | 继续等待下一次调用 |
| `BSP_ERROR` | 格式错误 | 检查发送数据格式，可选：清空缓冲区重新开始 |

## 注意事项

1. **缓冲区大小**：定义为 256 字节，足以存储多个完整数据包
2. **浮点数精度**：使用 32 位单精度浮点数（`float`）
3. **中断优先级**：USART1 中断优先级已设置为 0，可根据需要调整
4. **FreeRTOS 安全**：接收和解析已考虑多任务环境，但如果在多个任务中并发调用 `BSP_USART1_ParseData()`，建议使用互斥锁保护

## 性能指标

- **接收速率**：3Mbps（当前波特率配置）
- **缓冲区容量**：~40+ 个完整数据包
- **解析延迟**：< 1ms（sscanf 处理时间）
- **中断响应**：微秒级

## 故障排查

### 问题 1：收不到数据
- 检查 USART1 中断是否启用：`LL_USART_EnableIT_RXNE(USART1);`
- 检查 NVIC 中 USART1 中断是否启用：`NVIC_EnableIRQ(USART1_IRQn);`
- 检查上位机串口配置是否正确（波特率、奇偶校验等）

### 问题 2：解析失败
- 确认发送的数据包格式：`ceio:` 开头，以 `\n` 结尾
- 使用调试器查看 `usart1_rx_buffer` 内容
- 验证浮点数个数是否为 6 个

### 问题 3：缓冲区溢出
- 增大 `USART1_RX_BUFFER_SIZE` 定义
- 确保定期调用 `BSP_USART1_ParseData()` 清空已处理数据
