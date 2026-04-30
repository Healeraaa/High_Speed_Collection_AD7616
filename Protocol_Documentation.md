# STM32H743 高速数据采集 - 二进制传输协议

## 协议概述

本文档描述了用于 STM32H743 单片机与上位机之间进行高速大批量数据传输的二进制协议。该协议解决了文本协议在大包传输中的可靠性问题。

## 协议帧结构

### 帧格式 (总长度: 19 + 4*N 字节)

```
┌─────────┬──────────┬────────────┬──────────────┬──────────────┬──────────┬────────────┬──────────┬────────┐
│ 帧头    │ 帧长度   │ 设备ID/类型 │ 采样频率 (Fs)│ 起始时间戳   │ 数据量(N)│ 数据负载   │ 校验位  │ 帧尾   │
├─────────┼──────────┼────────────┼──────────────┼──────────────┼──────────┼────────────┼──────────┼────────┤
│ 2 Bytes │ 2 Bytes  │ 1 Byte     │ 4 Bytes      │ 4 Bytes      │ 2 Bytes  │ 4*N Bytes  │ 2 Bytes  │ 2 Bytes│
│ 0x55AA  │ 总长度   │ 0x01/0x02  │ uint32_t     │ uint32_t ms  │ uint16_t │ float[]    │ CRC16    │ 0x0D0A│
└─────────┴──────────┴────────────┴──────────────┴──────────────┴──────────┴────────────┴──────────┴────────┘
```

### 字段详解

| 字段 | 长度 | 数据类型 | 说明 |
|------|------|---------|------|
| 帧头 | 2 | - | 固定值 0x55 0xAA，用于帧同步 |
| 帧长度 | 2 | uint16_t | 整个数据包的字节数（小端序），包括帧头和帧尾 |
| 设备ID/类型 | 1 | uint8_t | `0x01`: 电压数据 / `0x02`: 电流数据 / 支持扩展 |
| 采样频率 | 4 | uint32_t | 数据采样频率 (Hz)，小端序。例如: 10000 = 10kHz |
| 起始时间戳 | 4 | uint32_t | 数据采集时刻的系统时间戳 (ms)，小端序 |
| 数据量 | 2 | uint16_t | 本帧包含的浮点数个数 (N)，小端序，最大 2048 |
| 数据负载 | 4*N | float[] | N 个浮点数，IEEE 754 格式，小端序排列 |
| 校验位 | 2 | uint16_t | CRC16-CCITT 校验码，计算范围: 帧头到数据负载，小端序 |
| 帧尾 | 2 | - | 固定值 0x0D 0x0A，用于帧结束标志 |

### 数据排列

- **多字节整数**: 小端序排列 (LSB first)
- **浮点数**: IEEE 754 标准格式，单精度 (32-bit)
- **数组**: 连续存储在数据负载区域

## CRC16 计算

### 算法：CRC16-CCITT

- **初始值 (Poly)**：0x1021
- **初始CRC值**：0xFFFF
- **计算范围**：从帧头第一字节到数据负载最后一字节
- **不包括**：帧长度字段、校验位、帧尾

### C 实现参考

```c
uint16_t CRC16_CCITT(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    
    return crc;
}
```

## 设备类型定义

```c
#define DEVICE_TYPE_VOLTAGE     0x01  // 电压数据
#define DEVICE_TYPE_CURRENT     0x02  // 电流数据
// 保留 0x03-0xFF 用于未来扩展
```

## 使用示例

### 发送电压数据

```c
#include "Module_TransmitUpper.h"

/* 准备数据数组 */
float voltage_data[100] = { /* ... */ };
uint32_t sample_rate = 10000;  // 10 kHz

/* 调用发送函数 */
int32_t result = Module_TransmitUpper_SendVoltageData(
    voltage_data,           // 数据指针
    100,                    // 数据个数
    sample_rate             // 采样频率
);

if (result == 0) {
    printf("发送成功\r\n");
} else {
    printf("发送失败\r\n");
}
```

### 发送电流数据

```c
float current_data[100] = { /* ... */ };

int32_t result = Module_TransmitUpper_SendCurrentData(
    current_data,           // 数据指针
    100,                    // 数据个数
    10000                   // 采样频率
);
```

### 通用接口

```c
int32_t result = Module_TransmitUpper_SendFrame(
    DEVICE_TYPE_VOLTAGE,    // 设备ID
    10000,                  // 采样频率
    voltage_data,           // 数据指针
    100                     // 数据个数
);
```

## 协议优势

### 1. **大包传输支持**
- 帧长度字段明确指定了数据包大小
- 接收端可以提前分配缓冲区
- 避免缓冲区溢出

### 2. **数据完整性保证**
- CRC16-CCITT 校验更可靠，对比特翻转检测率 99.998%+
- 比 SUM 校验更能发现传输错误

### 3. **时间同步**
- 时间戳字段记录采集时刻
- 支持多帧数据的对齐和拼接
- 便于上位机重建精确时间序列

### 4. **灵活的采样频率**
- 显式记录采样频率，不需要上位机预先配置
- 支持动态采样率调整

### 5. **协议可扩展性**
- 设备类型字段预留空间
- 易于添加新传感器数据类型

## 上位机接收实现步骤

### 1. **帧同步**
```python
while True:
    if uart.read(1) == 0x55:
        if uart.read(1) == 0xAA:
            # 帧头检测到，继续
            break
```

### 2. **读取帧长度**
```python
frame_len_bytes = uart.read(2)
frame_len = int.from_bytes(frame_len_bytes, 'little')
```

### 3. **读取帧数据**
```python
remaining_len = frame_len - 2  # 减去已读的帧头
frame_data = uart.read(remaining_len)
```

### 4. **验证CRC16**
```python
# 提取数据和校验码
payload = frame_data[:-4]  # 除去CRC和帧尾
crc_received = int.from_bytes(frame_data[-4:-2], 'little')
crc_calculated = calculate_crc16(payload)

if crc_received != crc_calculated:
    print("CRC 校验失败")
```

### 5. **解析数据**
```python
device_id = frame_data[0]
sample_freq = int.from_bytes(frame_data[1:5], 'little')
timestamp = int.from_bytes(frame_data[5:9], 'little')
count = int.from_bytes(frame_data[9:11], 'little')

# 读取浮点数数组
float_data = struct.unpack(f'<{count}f', frame_data[11:11+count*4])
```

## 配置指南

### 1. **修改采样频率**

编辑 [Application/App_VOFA_DataUpload.c](Application/App_VOFA_DataUpload.c)：

```c
#define AD7616_SAMPLE_FREQUENCY_HZ    10000  // 改为实际采样频率
```

### 2. **调整最大数据量**

编辑 [Modules/Module_TransmitUpper.c](Modules/Module_TransmitUpper.c)：

```c
#define MAX_DATA_PER_FRAME      2048  // 根据缓冲区大小调整
```

### 3. **修改传输缓冲区大小**

```c
static uint8_t tx_buffer[65536];  // 64KB，根据需要调整
```

## 性能参数

### 传输速率估算

| 条件 | 计算 | 结果 |
|------|------|------|
| 单帧最大大小 | 19 + 4*2048 | ~8.2KB |
| UART 波特率 | 115200 bps | 14.4 KB/s |
| 最大理论吞吐量 | 115200 / 8 | ~14.4 KB/s |
| 单帧传输时间 | 8192 bytes * 8 / 115200 | ~571ms |
| 多帧吞吐量 (@10kHz) | 10000 * 4 bytes/s | 40 KB/s |

**建议**：如需更高速率，考虑升级到 USB 或 千兆以太网连接。

## 故障排查

### 问题 1: CRC 校验失败

- **原因**：UART 传输错误、噪声干扰
- **解决**：
  1. 检查接线和信号完整性
  2. 降低波特率进行测试
  3. 添加缓冲区或 USB 隔离器

### 问题 2: 帧头丢失

- **原因**：数据缓冲区溢出、接收超时
- **解决**：
  1. 增加接收缓冲区大小
  2. 增加接收任务优先级
  3. 添加硬流控（RTS/CTS）

### 问题 3: 时间戳不准确

- **原因**：系统时钟漂移、中断延迟
- **解决**：
  1. 在采样开始时记录时间戳
  2. 使用更高精度的定时器
  3. 关闭不必要的中断

## 参考资源

- **CRC16-CCITT**: 标准多项式 0x1021
- **IEEE 754**: 单精度浮点数标准
- **小端序**: 低字节优先存储顺序
- **STM32H743**: 主控芯片，480MHz Cortex-M7

---

**最后更新**: 2026-04-29  
**协议版本**: 1.0  
**兼容硬件**: STM32H743VITx + AD7616 + FMC 总线
