# 协议优化快速参考卡

## 🎯 核心变化

### 协议格式升级

```
旧 ❌  ceio:%f,%f,%f,%f,%f,%f\n
新 ✅  ceio:%d,%f,%f,%f,...,%f\n
```

### 数据结构升级

```c
// 旧结构体（固定6个浮点数）
typedef struct {
    uint8_t  data[6];      // ❌ 固定大小
    uint32_t count;
    uint8_t  is_valid;
} USART1_RxData_t;

// 新结构体（支持可变长 + 模式）
typedef struct {
    uint32_t mode;                            // ✅ 模式标识（对应 Serial_SendPacket 的第一参数）
    float    float_data[USART1_MAX_FLOAT_DATA];  // ✅ 最多16个浮点数
    uint32_t float_count;                     // ✅ 自动返回实际个数
    uint8_t  is_valid;
} USART1_RxData_t;
```

---

## 📝 上位机发送示例

### Mode 0：标准模式（6个参数）
```
ceio:0,0.0,0.0,500.0,-500.0,30.0,3.0\n
```

### Mode 1：快速模式（3个参数）
```
ceio:1,10.0,20.0,30.0\n
```

### Mode 2：扩展模式（12个参数）
```
ceio:2,1.0,2.0,3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0,11.0,12.0\n
```

---

## 💻 下位机接收代码

### 轮询方式（最简单）
```c
USART1_RxData_t rx_data = {0};

while (1) {
    if (Module_USART1_ParseData(&rx_data) == BSP_OK) {
        uint32_t mode = rx_data.mode;
        uint32_t count = rx_data.float_count;
        float *data = rx_data.float_data;
        
        printf("Mode:%lu, Count:%lu\n", mode, count);
        for (uint32_t i = 0; i < count; i++) {
            printf("  [%lu]=%f\n", i, data[i]);
        }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

### 根据模式处理
```c
switch (rx_data.mode) {
    case 0:  // 标准
        for (int i = 0; i < 6; i++)
            data_converter[i].double_val = rx_data.float_data[i];
        Serial_SendPacket(0, (double *)data_converter);
        break;
    
    case 1:  // 快速
        for (int i = 0; i < 3; i++)
            data_converter[i].double_val = rx_data.float_data[i];
        Serial_SendPacket(1, (double *)data_converter);
        break;
    
    case 2:  // 扩展
        for (uint32_t i = 0; i < rx_data.float_count; i++)
            data_converter[i].double_val = rx_data.float_data[i];
        Serial_SendPacket(2, (double *)data_converter);
        break;
}
```

---

## 🔧 关键函数

### 接收字符（在中断中调用）
```c
void Module_USART1_ReceiveChar(uint8_t ch);
```

### 解析数据（在应用任务中定期调用）
```c
BSP_Status_t Module_USART1_ParseData(USART1_RxData_t *pRxData);
// 返回: BSP_OK (成功) / BSP_BUSY (不完整) / BSP_ERROR (错误)
```

---

## 📊 优势对比

| 特性 | 旧协议 | 新协议 |
|------|--------|--------|
| 浮点数个数 | 固定6个 | 可变1~16个 |
| 模式标识 | ❌ 无 | ✅ 有 |
| 数据计数 | 固定 | ✅ 自动 |
| 扩展需要修改代码 | ✅ 需要 | ❌ 不需要 |
| 协议灵活性 | 低 | **高** |

---

## 🚀 扩展示例

### 场景：需要从6个增加到12个浮点数

**旧协议方案：** ❌
1. 修改结构体 `data[6]` → `data[12]`
2. 修改 sscanf 增加 6 个参数
3. 修改所有处理代码
4. 重新编译烧录

**新协议方案：** ✅
1. 上位机直接发送 12 个浮点数
2. **下位机无需修改任何代码！**
3. 自动返回 `float_count = 12`

---

## 📁 相关文件

| 文件 | 说明 |
|------|------|
| `Module_ReceiveUpper.h` | 数据结构体、函数声明 |
| `Module_ReceiveUpper.c` | 接收和解析实现 |
| `App_ReceiveUpper_Example.c` | 使用示例（3种方案） |
| `PROTOCOL_OPTIMIZATION.md` | 详细设计文档 |

---

## ✅ 快速集成步骤

1. **查看新数据结构** → Module_ReceiveUpper.h
2. **参考示例代码** → App_ReceiveUpper_Example.c
3. **更新上位机** → 按新协议格式发送数据
4. **调用解析函数** → Module_USART1_ParseData()
5. **根据模式处理** → switch(rx_data.mode)

---

## 🎓 学习建议

1. **理解新协议格式** → PROTOCOL_OPTIMIZATION.md
2. **运行示例代码** → App_ReceiveUpper_Example.c (App_Test_ParseData)
3. **在项目中应用** → 参考轮询或队列方案
4. **验证扩展性** → 修改上位机数据个数，确认无需修改下位机代码

---

## 💡 提示

- **最大支持** 16 个浮点数，可在 Module_ReceiveUpper.h 中修改 `USART1_MAX_FLOAT_DATA`
- **模式值** 可自定义，建议范围 0~15（4 bit）
- **浮点精度** 为 32-bit 单精度，足以应对大多数应用
- **缓冲区大小** 256 字节，可根据需要调整 `USART1_RX_BUFFER_SIZE`
