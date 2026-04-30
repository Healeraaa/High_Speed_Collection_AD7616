## 二进制协议 - 快速参考

### 🚀 快速开始

#### 单片机端 (C代码)

```c
// 1. 包含头文件
#include "Module_TransmitUpper.h"

// 2. 准备数据和参数
float data[100] = { /* ... 采集的数据 ... */ };
uint16_t count = 100;
uint32_t fs = 10000;  // 10kHz

// 3. 发送电压数据
Module_TransmitUpper_SendVoltageData(data, count, fs);

// 或发送电流数据
Module_TransmitUpper_SendCurrentData(data, count, fs);

// 或通用接口
Module_TransmitUpper_SendFrame(0x01, fs, data, count);
```

### 📋 协议帧结构 (一目了然)

```
┌──────┬────────┬─────────┬────────┬────────┬────────┬──────────┬──────┬──────┐
│ 0x55 │ 0xAA   │ 帧长(2) │ ID(1)  │ Fs(4)  │ TS(4)  │ Count(2) │ 浮点 │CRC+尾│
│ AA   │ 55     │ 小端序  │0x01/02 │uint32  │uint32  │uint16    │ 数组 │ 2+2  │
│      │(SYNC)  │ 总长度  │(电压)  │(Hz)    │(ms)    │(个数)    │(4*N) │验证  │
└──────┴────────┴─────────┴─────────┴────────┴────────┴────────┴──────┴──────┘
  2B      2B        2B      1B      4B      4B      2B     4*N    2+2B
```

### ⚙️ 关键参数配置

| 参数 | 位置 | 默认值 | 说明 |
|------|------|--------|------|
| 采样频率 | `App_VOFA_DataUpload.c:19` | 10000 Hz | 改为实际采样频率 |
| 最大数据量 | `Module_TransmitUpper.c:44` | 2048 | 每帧最多浮点数 |
| 缓冲区大小 | `Module_TransmitUpper.c:48` | 65536 | 发送缓冲大小 |

### 📊 协议数据

| 项 | 数值 |
|----|------|
| 帧头 | 0x55AA |
| 帧尾 | 0x0D0A |
| 设备类型 - 电压 | 0x01 |
| 设备类型 - 电流 | 0x02 |
| CRC 多项式 | 0x1021 |
| CRC 初值 | 0xFFFF |
| 最大单帧 | 19 + 4*2048 = 8211 字节 |

### 💻 上位机接收 (Python)

```python
import struct, serial

ser = serial.Serial('/dev/ttyUSB0', 115200)

# 等待 0x55
while ser.read(1)[0] != 0x55: pass
if ser.read(1)[0] != 0xAA: return

# 读帧长
frame_len = struct.unpack('<H', ser.read(2))[0]
data = ser.read(frame_len - 2)

# 解析
device_id = data[0]          # 0x01=电压, 0x02=电流
fs = struct.unpack('<I', data[1:5])[0]    # 采样频率
ts = struct.unpack('<I', data[5:9])[0]    # 时间戳
count = struct.unpack('<H', data[9:11])[0] # 数据个数
values = struct.unpack(f'<{count}f', data[11:11+count*4])

print(f"Type: {'电压' if device_id==1 else '电流'}, Fs={fs}Hz, Count={count}")
print(f"Data: {values[:10]}...")  # 打印前10个
```

### 🔍 快速诊断

```bash
# 1. 查看原始数据
minicom -D /dev/ttyUSB0 -b 115200

# 2. 十六进制查看
xxd /dev/ttyUSB0 | head -20

# 3. 统计帧数
timeout 10 cat /dev/ttyUSB0 | grep -ao "55aa" | wc -l

# 4. Python 验证
python3 << 'EOF'
import serial, struct
s = serial.Serial('/dev/ttyUSB0', 115200)
d = s.read(1024)
print("Header:", hex(d[0]), hex(d[1]))
print("Length:", struct.unpack('<H', d[2:4])[0])
print("Type:", hex(d[4]))
EOF
```

### ⚠️ 常见问题

**Q: 数据丢失？**  
A: 检查 UART 连接，尝试降低波特率到 9600 测试

**Q: CRC 校验失败？**  
A: 检查发送缓冲是否溢出，或 UART 干扰

**Q: 帧头乱码？**  
A: 确认波特率一致，检查 printf 重定向配置

**Q: 编译错误？**  
A: 确保 `Module_TransmitUpper.h` 被正确包含

### 📱 设备类型扩展

```c
#define DEVICE_TYPE_VOLTAGE     0x01  // 已定义
#define DEVICE_TYPE_CURRENT     0x02  // 已定义
#define DEVICE_TYPE_TEMP        0x03  // 可自定义
#define DEVICE_TYPE_PRESSURE    0x04  // 可自定义
```

### 🎯 应用场景

| 场景 | 数据量 | 频率 | 帧大小 |
|------|--------|------|--------|
| 低速测试 | 100 | 1kHz | ~500B |
| 中等数据 | 512 | 10kHz | ~2.1KB |
| 高速采集 | 2048 | 100kHz | ~8.2KB |

### 📞 支持

- 完整文档：`Protocol_Documentation.md`
- 实现总结：`IMPLEMENTATION_SUMMARY.md`
- 源代码：`Module_TransmitUpper.c/h`

---
**最后更新**: 2026-04-29 | **版本**: 1.0
