# 协议优化设计文档

## 📋 问题分析

### 旧协议的局限性
```c
格式: "ceio:%f,%f,%f,%f,%f,%f\n"
```

**问题：**
1. ❌ 固定 6 个浮点数，难以扩展
2. ❌ 无法区分不同的操作模式
3. ❌ 如果未来需要 8 个或 12 个数据，必须修改解析函数
4. ❌ 对应 `Serial_SendPacket(mode, ...)` 中的 mode 无法传递

---

## ✨ 新协议设计

### 新协议格式
```c
"ceio:%d,%f,%f,%f,...,%f\n"
```

### 协议说明

| 字段 | 类型 | 说明 | 备注 |
|------|------|------|------|
| `ceio:` | 前缀 | 起始标记 | 不变 |
| `%d` | 32-bit 整数 | 模式标识符 | 对应 `Serial_SendPacket` 的第一个参数 |
| `%f` | 浮点数 | 参数数据 | 可变长度（1~16 个） |
| `\n` | 后缀 | 结束标记 | 不变 |

### 协议示例

#### 示例 1：Mode 0（标准模式，6 个浮点数）
```
ceio:0,0.5,1.5,2.5,3.5,4.5,5.5\n
```
- Mode = 0
- Float[0~5] = [0.5, 1.5, 2.5, 3.5, 4.5, 5.5]

#### 示例 2：Mode 1（快速模式，3 个浮点数）
```
ceio:1,10.0,20.0,30.0\n
```
- Mode = 1
- Float[0~2] = [10.0, 20.0, 30.0]

#### 示例 3：Mode 2（扩展模式，12 个浮点数）
```
ceio:2,1.1,2.2,3.3,4.4,5.5,6.6,7.7,8.8,9.9,10.1,11.2,12.3\n
```
- Mode = 2
- Float[0~11] = [1.1, 2.2, ..., 12.3]
- **无需修改解析函数！**

---

## 🔄 架构对比

### 旧架构（固定 6 个浮点数）
```c
// 旧数据结构
typedef struct {
    uint8_t  data[6];       // ❌ 固定 6 个，无法扩展
    uint32_t count;
    uint8_t  is_valid;
} USART1_RxData_t;

// 旧解析函数
sscanf(buffer, "ceio:%f,%f,%f,%f,%f,%f", 
       &data[0], &data[1], &data[2], &data[3], &data[4], &data[5]);
       // ❌ 必须逐个指定，无法适应不同的数据个数
```

### 新架构（支持可变长度 + 模式）
```c
// 新数据结构
typedef struct {
    uint32_t mode;                              // ✅ 模式标识符
    float    float_data[USART1_MAX_FLOAT_DATA]; // ✅ 最多支持 16 个
    uint32_t float_count;                       // ✅ 自动返回实际个数
    uint8_t  is_valid;
} USART1_RxData_t;

// 新解析函数（灵活）
while (strtok_r(NULL, ",", &saveptr)) {
    sscanf(..., "%f", &float_data[count++]);
}
// ✅ 支持任意个数的浮点数，自动计数！
```

---

## 💡 关键优化点

### 1. **支持可变长度数据** ✅
```c
#define USART1_MAX_FLOAT_DATA  16   // 最多 16 个浮点数

// 旧方案：6 个浮点数满了 → 修改代码
// 新方案：需要 12 个 → 直接发送，无需修改！
```

### 2. **自动检测数据个数** ✅
```c
// 接收到数据后，自动返回实际接收的浮点数个数
rx_data.float_count  // 包含实际数量
```

### 3. **模式标识符** ✅
```c
rx_data.mode  // 区分不同的操作模式

switch (rx_data.mode) {
    case 0: // 标准模式（6 个参数）
    case 1: // 快速模式（3 个参数）
    case 2: // 扩展模式（任意个参数）
}
```

### 4. **灵活的字符串分割** ✅
使用 `strtok_r()` 而不是固定的 `sscanf()`
```c
// 旧：必须指定 6 个 %f
sscanf(buffer, "ceio:%f,%f,%f,%f,%f,%f", &f0, &f1, &f2, &f3, &f4, &f5);

// 新：自动适应任意个数
while ((token = strtok_r(NULL, ",", &saveptr))) {
    sscanf(token, "%f", &float_data[count++]);
}
```

---

## 📊 对应关系

### 旧代码
```c
void App_IncentiveSettingsTask(void *argument)
{
    Serial411_DoubleConverter_t data_converter[SERIAL411_DATA_LENGTH];
    data_converter[0].double_val = 0.0;
    data_converter[1].double_val = 0.0;
    data_converter[2].double_val = 500.0;
    data_converter[3].double_val = -500.0;
    data_converter[4].double_val = 30.0;
    data_converter[5].double_val = 3.0;
    
    Serial_SendPacket(0x0, (double *)data_converter);
}
```

### 新代码（上位机发送）
```c
// 上位机发送：
printf("ceio:0,0.0,0.0,500.0,-500.0,30.0,3.0\n");

// 下位机接收并解析：
USART1_RxData_t rx_data;
Module_USART1_ParseData(&rx_data);

// 获取数据：
printf("Mode: %lu\n", rx_data.mode);           // 0
printf("Float Count: %lu\n", rx_data.float_count); // 6
printf("data_converter[0] = %f\n", rx_data.float_data[0]); // 0.0
printf("data_converter[1] = %f\n", rx_data.float_data[1]); // 0.0
// ... 以此类推
```

---

## 🚀 使用示例

### 最简洁的接收代码
```c
#include "Module.h"

void My_Task(void *argument)
{
    USART1_RxData_t rx_data = {0};
    
    while (1)
    {
        if (Module_USART1_ParseData(&rx_data) == BSP_OK)
        {
            // 成功解析数据
            uint32_t mode = rx_data.mode;
            uint32_t count = rx_data.float_count;
            float *data = rx_data.float_data;
            
            printf("Mode: %lu, Count: %lu\n", mode, count);
            for (uint32_t i = 0; i < count; i++)
            {
                printf("  [%lu] = %f\n", i, data[i]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### 根据模式处理数据
```c
if (Module_USART1_ParseData(&rx_data) == BSP_OK)
{
    switch (rx_data.mode)
    {
        case 0:  // 模式 0：标准（6 个参数）
            data_converter[0].double_val = rx_data.float_data[0];
            data_converter[1].double_val = rx_data.float_data[1];
            // ... 处理 6 个参数 ...
            Serial_SendPacket(0, (double *)data_converter);
            break;
        
        case 1:  // 模式 1：快速（3 个参数）
            data_converter[0].double_val = rx_data.float_data[0];
            data_converter[1].double_val = rx_data.float_data[1];
            data_converter[2].double_val = rx_data.float_data[2];
            Serial_SendPacket(1, (double *)data_converter);
            break;
        
        case 2:  // 模式 2：扩展（自适应）
            for (uint32_t i = 0; i < rx_data.float_count; i++)
            {
                data_converter[i].double_val = rx_data.float_data[i];
            }
            Serial_SendPacket(2, (double *)data_converter);
            break;
    }
}
```

---

## 🔧 扩展性对比

### 旧协议：如果需要增加到 8 个浮点数？

❌ 需要修改：
1. 数据结构体（data[6] → data[8]）
2. 解析函数（sscanf 增加 2 个参数）
3. 调用代码（处理新参数）

### 新协议：如果需要增加到 8 个浮点数？

✅ 完全无需修改！
```c
// 只需上位机发送：
ceio:0,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0\n

// 下位机自动处理：
rx_data.float_count = 8  // 自动返回
// 无需修改任何代码！
```

---

## 📈 协议优势总结

| 特性 | 旧协议 | 新协议 |
|------|--------|--------|
| 固定浮点数个数 | 6 个 | 1~16 个（可扩展） |
| 模式标识符 | ❌ 无 | ✅ 有 |
| 自动数据计数 | ❌ 无 | ✅ 有 |
| 代码改动次数 | 频繁 | 0 次 |
| 向后兼容性 | 无 | ✅ 有 |
| 协议灵活性 | 低 | 高 |

---

## 📝 协议设计建议

### 模式定义规范

```c
#define MODE_STANDARD    0    // 标准模式：6 个参数
#define MODE_FAST        1    // 快速模式：3 个参数
#define MODE_EXTENDED    2    // 扩展模式：自定义个数
#define MODE_RESERVED    3~15 // 预留模式
```

### 上位机发送规范

```python
# Python 示例（上位机）

# 模式 0：标准参数
ser.write(b"ceio:0,0.5,1.5,2.5,3.5,4.5,5.5\n")

# 模式 1：快速参数
ser.write(b"ceio:1,10.0,20.0,30.0\n")

# 模式 2：扩展参数（未来可增加）
params = [1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.0]
ser.write(f"ceio:2,{','.join(map(str, params))}\n".encode())
```

---

## 🎯 快速迁移清单

- [ ] 更新 `Module_ReceiveUpper.h` 中的数据结构体
- [ ] 更新 `Module_ReceiveUpper.c` 中的解析函数
- [ ] 上位机更新发送协议
- [ ] 应用层代码根据模式处理数据
- [ ] 测试所有模式的数据接收
- [ ] 验证扩展性（增加数据个数后无需修改代码）

---

## 📚 参考示例

详见 `App_ReceiveUpper_Example.c`：
- 轮询方式接收
- 队列方式接收
- 模式分支处理
- 完整的测试函数
