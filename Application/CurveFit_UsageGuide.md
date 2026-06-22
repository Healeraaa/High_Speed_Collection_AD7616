# 拟合曲线回调函数使用指南

## 概述

本系统使用回调函数机制来实现不同挡位的拟合曲线转换。当上位机通过 `App_IncentiveSettingsTask` 发送命令时，系统会自动根据配置的 **IV增益**、**电压放大级数1** 和 **电压放大级数2** 的组合，选择相应的拟合曲线。

`App_WaveCollectionTask` 在处理原始ADC数据时，会调用注册的拟合曲线回调函数来计算电流值，替代原有的线性转换计算。

---

## 系统架构

```
┌─────────────────────────────────┐
│  App_IncentiveSettingsTask      │ ← 接收上位机命令，配置参数
│  (激励设置任务)                  │
└──────────────┬──────────────────┘
               │
               │ 根据配置选择回调函数
               ↓
      ┌────────────────────┐
      │  App_CurveFit.c    │ ← 拟合曲线实现库
      │  (10种挡位的函数)   │
      └────────────┬───────┘
                   │
                   │ 注册回调函数
                   ↓
      ┌────────────────────────────────┐
      │  App_WaveCollectionTask        │
      │  (波形采集任务)                 │
      │  使用回调计算电流值             │
      └────────────────────────────────┘
```

---

## 核心接口

### 1. **App_CurveFit.h** - 回调函数定义

```c
// 回调函数指针类型
typedef float (*CurveFit_Callback_t)(float voltage);

// 注册回调函数
void App_CurveFit_RegisterCallback(CurveFit_Callback_t callback);

// 执行拟合曲线转换
float App_CurveFit_Process(float voltage);

// 获取当前注册的回调函数
CurveFit_Callback_t App_CurveFit_GetCallback(void);
```

### 2. **App_WaveCollectionTask.h** - 波形采集接口

```c
// 切换波形采集任务使用的拟合曲线
void App_WaveCollection_SetCurveFitCallback(CurveFit_Callback_t callback);
```

---

## 支持的拟合曲线挡位

系统预定义了10种拟合曲线，对应不同的增益配置组合：

| 挡位 | IV增益 | 电压放大1 | 电压放大2 | 回调函数 |
|------|--------|----------|----------|---------|
| 1 | 33Ω | 1倍 | 1倍 | `CurveFit_Range1_33_1_1` |
| 2 | 1KΩ | 1倍 | 1倍 | `CurveFit_Range2_1K_1_1` |
| 3 | 1KΩ | 10倍 | 1倍 | `CurveFit_Range3_1K_10_1` |
| 4 | 1KΩ | 10倍 | 3.3倍 | `CurveFit_Range4_1K_10_3_3` |
| 5 | 1KΩ | 10倍 | 10倍 | `CurveFit_Range5_1K_10_10` |
| 6 | 10KΩ | 10倍 | 3.3倍 | `CurveFit_Range6_10K_10_3_3` |
| 7 | 10KΩ | 10倍 | 10倍 | `CurveFit_Range7_10K_10_10` |
| 8 | 10KΩ | 10倍 | 33倍 | `CurveFit_Range8_10K_10_33` |
| 9 | 100KΩ | 10倍 | 10倍 | `CurveFit_Range9_100K_10_10` |
| 10 | 100KΩ | 10倍 | 33倍 | `CurveFit_Range10_100K_10_33` |

---

## 工作流程

### 步骤1：上位机发送命令

上位机通过串口发送 `ceiod` 格式命令设置增益配置：

```
ceiod:%d1,%d2,%d3,%d4,%d5\n
```

其中：
- `%d1`: 模式（第一个整数）
- `%d2`: WE通道
- `%d3`: **IV增益** ← 用于选择拟合曲线
- `%d4`: **电压放大级数1** ← 用于选择拟合曲线
- `%d5`: **电压放大级数2** ← 用于选择拟合曲线

### 步骤2：App_IncentiveSettingsTask 处理命令

```c
// 在 App_IncentiveSettingsTask 中
if (Module_USART1_ParseData(&rx_data) == Module_OK)
{
    // 提取配置参数
    IV_Gain_TypeDef iv_gain = (IV_Gain_TypeDef)data_converter[6].u8_array[1];
    Voltage_Gain_Stage1_TypeDef stage1 = (Voltage_Gain_Stage1_TypeDef)data_converter[6].u8_array[2];
    Voltage_Gain_Stage2_TypeDef stage2 = (Voltage_Gain_Stage2_TypeDef)data_converter[6].u8_array[3];

    // 根据配置选择拟合曲线
    CurveFit_Callback_t callback = /* 选择对应的拟合曲线函数 */;

    // 注册到波形采集任务
    if (callback != NULL)
    {
        App_WaveCollection_SetCurveFitCallback(callback);
    }
}
```

### 步骤3：App_WaveCollectionTask 使用回调函数

```c
// 在 App_WaveCollectionTask 的数据处理循环中
for (uint32_t i = 0; i < rxInfo.validCount; i++)
{
    int16_t raw_data = (int16_t)p_processing_buffer[i];
    float voltage = (0.153362f * (float)raw_data) + 0.593916f;

    if (i % 2 == 0)  // 偶数索引 → 电压值
    {
        p_iv_buffer[i] = voltage / 1000.0f;
    }
    else  // 奇数索引 → 电流值（使用拟合曲线）
    {
        CurveFit_Callback_t callback = App_CurveFit_GetCallback();
        if (callback != NULL)
        {
            p_iv_buffer[i] = callback(voltage);  // ← 调用拟合曲线
        }
    }
}
```

---

## 添加新的拟合曲线

如果需要添加新的拟合曲线挡位，按以下步骤操作：

### 1. 在 `App_CurveFit.h` 添加声明

```c
float CurveFit_RangeX_Custom_Name(float voltage);
```

### 2. 在 `App_CurveFit.c` 实现函数

```c
float CurveFit_RangeX_Custom_Name(float voltage)
{
    // 根据实际标定数据填写拟合系数
    float a = 0.001f;  // 一次项系数
    float b = 0.0f;    // 常数项
    float c = 0.0f;    // 二次项系数（如需要）
    
    // 线性拟合
    return a * voltage + b;
    
    // 或二次拟合
    // return a * voltage + b + c * voltage * voltage;
}
```

### 3. 在 `App_IncentiveSettingsTask.c` 中添加匹配逻辑

```c
else if (iv_gain == IV_GAIN_XXX && stage1 == VOLTAGE_GAIN_STAGE1_XXX && stage2 == VOLTAGE_GAIN_STAGE2_XXX)
{
    callback = CurveFit_RangeX_Custom_Name;
}
```

---

## 故障排查

### 问题1：电流值计算不准确

**原因**：拟合曲线系数不正确

**解决方案**：
1. 收集标准参考数据（电压 vs 电流对应关系）
2. 使用数据拟合工具（MATLAB、Python 等）重新计算系数
3. 更新 `App_CurveFit.c` 中对应拟合曲线的系数

### 问题2：未注册回调函数导致电流值为0

**原因**：上位机命令未成功匹配，或配置不在预定义列表中

**解决方案**：
1. 检查上位机发送的配置参数是否正确
2. 如果使用的配置组合不在预定义列表中，需要在 `App_CurveFit.c` 中添加新的拟合曲线函数
3. 在 `App_IncentiveSettingsTask.c` 中添加匹配该配置的条件判断

### 问题3：数据类型不匹配

**原因**：`voltage` 参数单位或数据类型错误

**解决方案**：
- 确保传入拟合曲线的 `voltage` 参数的单位一致
- 根据需要进行单位转换（mV ↔ V）

---

## 示例：使用指定的拟合曲线

```c
// 方法1：直接调用回调函数
float voltage = 1200.0f;  // mV
CurveFit_Callback_t callback = App_CurveFit_GetCallback();
if (callback != NULL)
{
    float current = callback(voltage);
    printf("Current: %.6f A\n", current);
}

// 方法2：使用 App_CurveFit_Process 包装函数
float voltage = 1200.0f;  // mV
float current = App_CurveFit_Process(voltage);
printf("Current: %.6f A\n", current);
```

---

## 备注

- **线程安全**：当前实现假设回调函数的注册和调用不在中断上下文中发生竞争。如果需要多线程安全，可在 `App_CurveFit.c` 中添加互斥锁保护。
- **性能**：回调函数在波形采集数据处理循环中被频繁调用，建议保持实现简洁高效。
- **默认行为**：当未注册回调函数时，`App_WaveCollectionTask` 会回退到原有的线性转换计算。
