# STM32H7 + FMC + AD7616 高速采集问题总结

---

## 📋 目录

- [问题现象](#问题现象)
- [问题分析](#问题分析)
- [根本原因](#根本原因)
- [完整解决方案](#完整解决方案)
- [STM32H7 总线架构](#stm32h7-总线架构)
- [性能对比](#性能对比)
- [常见陷阱](#常见陷阱)
- [验证清单](#验证清单)
- [最佳实践](#最佳实践)

---

## 问题现象

### 初始问题描述

在 STM32H743 + FMC + AD7616 项目中，发现直接写入 FMC 和使用 HAL 库函数写入的行为不一致：

```c
// 方式 1：直接写入 - 波形正常 ✅
*(__IO uint16_t *)(0x60000000U) = 0xA55A;

// 方式 2：HAL 库函数 - 波形异常 ❌
BSP_FMC_PSRAM_WriteHalfWord(0, 0xA55A);
```

### 逻辑分析仪显示

**期望波形：**
```
D[15:0]: 0xA55A → (延迟) → 0xA55A → (延迟) → 0xA55A
```

**实际波形（使用 HAL 函数）：**
```
D[15:0]: 0xA55A → 0x0000 → 0xE9 → 0xA55A → 0x0000 → 0xE9 → ...
                   ↑ 额外的异常数据
```

### 症状特征

- ✅ 直接写入 FMC 地址：波形完美
- ❌ 使用 `BSP_FMC_PSRAM_WriteHalfWord()`：出现额外波形
- ❌ 额外数据包括：`0x0000`、`0xE9`、`0x0088` 等
- ⚠️ 数据写入功能仍然正常，但总线上有干扰信号

---

## 问题分析

### 关键发现：堆栈位置问题

**测试对比：**

```c
// 配置 1：堆栈在 AXI SRAM (0x24000000)
// 结果：FMC 写入异常 ❌

// 配置 2：堆栈在 DTCM (0x20000000)
// 结果：FMC 写入正常 ✅
```

**问题确认：**
> 💡 将堆栈从 AXI SRAM 移到 DTCM 后，FMC 时序完全正常！

---

## 根本原因

### 核心问题：AXI 总线竞争

#### 堆栈在 AXI SRAM 时的执行流程

```
┌─────────────────────────────────────────────────┐
│  CPU 执行 BSP_FMC_PSRAM_WriteHalfWord(0, 0xA55A)│
├─────────────────────────────────────────────────┤
│  1. 函数调用 → 保存上下文到栈                   │
│     ↓                                           │
│     写 AXI SRAM (0x24000000)                    │
│     ↓                                           │
│     使用 AXI 总线 ⚠️                            │
│                                                 │
│  2. 执行 FMC 写入                               │
│     ↓                                           │
│     写 FMC (0x60000000)                         │
│     ↓                                           │
│     使用 AXI 总线 ⚠️                            │
│     ↓                                           │
│     与步骤 1 的栈操作竞争 ❌                     │
│                                                 │
│  3. 函数返回 → 恢复上下文从栈                   │
│     ↓                                           │
│     读 AXI SRAM (0x24000000)                    │
│     ↓                                           │
│     再次与 FMC 竞争 ❌                          │
├─────────────────────────────────────────────────┤
│  结果：AXI 总线竞争 → FMC 时序被打断            │
│       → 产生额外的 0x0000, 0xE9 波形            │
└─────────────────────────────────────────────────┘
```

#### 堆栈在 DTCM 时的执行流程

```
┌─────────────────────────────────────────────────┐
│  CPU 执行 BSP_FMC_PSRAM_WriteHalfWord(0, 0xA55A)│
├─────────────────────────────────────────────────┤
│  1. 函数调用 → 保存上下文到栈                   │
│     ↓                                           │
│     写 DTCM (0x20000000)                        │
│     ↓                                           │
│     使用 CPU 专用通道 ✅                        │
│     ↓                                           │
│     不占用 AXI 总线！                           │
│                                                 │
│  2. 执行 FMC 写入                               │
│     ↓                                           │
│     写 FMC (0x60000000)                         │
│     ↓                                           │
│     独占 AXI 总线 ✅                            │
│                                                 │
│  3. 函数返回 → 恢复上下文从栈                   │
│     ↓                                           │
│     读 DTCM (0x20000000)                        │
│     ↓                                           │
│     使用 CPU 专用通道 ✅                        │
├─────────────────────────────────────────────────┤
│  结果：无总线竞争 → FMC 时序完美                │
│       → 只有 0xA55A 波形                        │
└─────────────────────────────────────────────────┘
```

### 分散加载文件的问题

#### 问题：多个 `.ANY` 导致数据分散

```sct
❌ 错误配置：

RW_IRAM1 0x20000000 0x00020000  {  // DTCM (128KB)
    .ANY (+RW +ZI)  // ← 第 1 个 .ANY
}

RW_IRAM2 0x24000000 0x00080000  {  // AXI SRAM (512KB)
    .ANY (+RW +ZI)  // ← 第 2 个 .ANY（冲突！）
}

链接器行为：
├─ 链接器会"平均分配"给多个 .ANY
└─ 即使 DTCM 没满，数据也会分散到多个区域
```

---

## 完整解决方案

### 1. 修改分散加载文件（关键！）

```sct
; filepath: MDK-ARM\Project.sct

LR_IROM1 0x08000000 0x00200000  {
  
  ER_IROM1 0x08000000 0x00200000  {
    *.o (RESET, +First)
    *(InRoot$$Sections)
    .ANY (+RO)
  }
  
  ; ========== DTCM (128KB) - 唯一的通用数据区域 ==========
  RW_IRAM1 0x20000000 0x00020000  {
    
    ; ✅ Priority 1：堆栈必须在 DTCM
    startup_stm32h743xx.o (STACK)
    startup_stm32h743xx.o (HEAP)
    
    ; ✅ Priority 2：HAL 核心变量
    stm32h7xx_hal.o (+RW +ZI)
    system_stm32h7xx.o (+RW +ZI)
    stm32h7xx_hal_timebase_tim.o (+RW +ZI)
    
    ; ✅ Priority 3：关键驱动
    bsp_fmc.o (+RW +ZI)
    Module_AD7616.o (+RW +ZI)
    
    ; ✅ Priority 4：其他模块
    .ANY (+RW +ZI)  // 唯一的 .ANY
  }
  
  ; ========== AXI SRAM (512KB) - 只放显式指定的大缓冲区 ==========
  RW_IRAM2 0x24000000 0x00080000  {
    *(.adc_buffer)
    *(RAM_D1)
    ; ❌ 不要加 .ANY！
  }
}
```

### 2. 禁用 FMC WriteFifo

```c
// filepath: BSP\FMC\bsp_fmc.c

bsp_fmc_psram_handle.Init.WriteFifo = FMC_WRITE_FIFO_DISABLE;  // ✅
```

### 3. 使用内联函数

```c
// filepath: BSP\FMC\bsp_fmc.h

static inline void BSP_FMC_PSRAM_WriteHalfWord(uint32_t address, uint16_t data)
{
  *(__IO uint16_t *)(PSRAM_BASE_ADDR + address) = data;
}
```

### 4. 大缓冲区放 AXI SRAM

```c
// filepath: Modules\Module_AD7616.c

__attribute__((section(".adc_buffer"))) __attribute__((aligned(32)))
static uint16_t adc_buffer_a[32768];
```

---

## STM32H7 总线架构

### 总线拓扑图

```
┌───────────────── Cortex-M7 Core @ 480MHz ─────────────────┐
│   ITCM   │   DTCM   │  I-Cache │  D-Cache │  AXI Master  │
│   64KB   │  128KB   │   16KB   │   16KB   │   Interface  │
│  专用通道 │  专用通道 │          │          │              │
└────┬─────┴────┬─────┴──────────┴──────────┴──────┬────────┘
     │          │                                   │
     │          │                                   │
     ↓          ↓                                   ↓
┌─────────┐ ┌─────────┐                   ┌───────────────┐
│  ITCM   │ │  DTCM   │                   │  AXI Matrix   │
│  64KB   │ │ 128KB   │                   │  (总线仲裁器) │
└─────────┘ └─────────┘                   └───────┬───────┘
      ↑                                           │
      │                                           │
 CPU 专用通道                          ┌──────────┼──────────┐
 零等待访问 ✅                         ↓          ↓          ↓
 不占用 AXI 总线                  ┌────────┬────────┬────────┐
                                 │  AXI   │ SRAM1/2│  FMC   │
                                 │  SRAM  │   /3   │ Bank1  │
                                 │ 512KB  │ 288KB  │        │
                                 │ 0x2400 │ 0x3000 │ 0x6000 │
                                 └────────┴────────┴────────┘
                                      ↑               ↑
                                      └─ 共享 AXI 总线 ┘
                                         会产生竞争 ⚠️
```

### 内存特性对比

| 内存区域 | 起始地址 | 大小 | 总线 | CPU 访问延迟 | DMA 支持 | 最佳用途 |
|---------|---------|------|------|-------------|---------|---------|
| **DTCM** | 0x20000000 | 128KB | CPU 专用 | 0 周期 | ❌ | 堆栈、高频数据 |
| **AXI SRAM** | 0x24000000 | 512KB | AXI 共享 | 0~3 周期 | ✅ | DMA 缓冲区 |
| **SRAM1** | 0x30000000 | 128KB | AHB 共享 | 1~2 周期 | ✅ | 通用数据 |
| **FMC Bank1** | 0x60000000 | 256MB | AXI/FMC | 可配置 | ✅ | 外部存储器 |

---

## 性能对比

| 配置 | 平均延迟 (cycles) | 等效频率 (MHz) | 波形质量 | DTCM 使用率 |
|------|------------------|---------------|---------|------------|
| **堆栈在 AXI SRAM** | ~25 | 19.2 | ❌ 有干扰 | ~30% |
| **堆栈在 DTCM** | ~8 | 60 | ✅ 完美 | ~90% |

**性能提升：约 3 倍！**

---

## 常见陷阱

### ⚠️ 陷阱 1：多个 `.ANY` 导致数据分散

```sct
❌ 错误：
RW_IRAM1 { .ANY (+RW +ZI) }
RW_IRAM2 { .ANY (+RW +ZI) }  // 冲突

✅ 正确：
RW_IRAM1 { .ANY (+RW +ZI) }  // 唯一的 .ANY
RW_IRAM2 { *(.adc_buffer) }  // 只放显式指定
```

### ⚠️ 陷阱 2：WriteFifo 干扰时序

```c
❌ 错误：
bsp_fmc_psram_handle.Init.WriteFifo = FMC_WRITE_FIFO_ENABLE;

✅ 正确：
bsp_fmc_psram_handle.Init.WriteFifo = FMC_WRITE_FIFO_DISABLE;
```

### ⚠️ 陷阱 3：DMA 访问 DTCM 失败

```c
❌ 错误：
uint8_t buffer[1024];  // 默认在 DTCM
HAL_UART_Transmit_DMA(&huart1, buffer, 1024);  // 失败

✅ 正确：
__attribute__((section("RAM_D1")))
uint8_t buffer[1024];
HAL_UART_Transmit_DMA(&huart1, buffer, 1024);  // 成功
```

---

## 验证清单

### 编译时验证

- [ ] 查看 Map 文件确认堆栈在 DTCM (0x20000000)
- [ ] 确认 HAL 核心变量在 DTCM
- [ ] 确认关键驱动在 DTCM
- [ ] 确认大缓冲区在 AXI SRAM
- [ ] 只有一个 `.ANY` 在 scatter 文件中

### 运行时验证

- [ ] 使用逻辑分析仪验证 FMC 波形完美
- [ ] 检查内存布局（`Image$$RW_IRAM1$$Base`）
- [ ] 测试 FMC 性能（cycles/write）
- [ ] 验证 DMA 缓冲区位置

---

## 最佳实践

### ✅ 推荐配置

1. **堆栈永远放 DTCM**（STM32H7 的黄金法则）
2. **高频访问数据放 DTCM**（HAL 核心变量、驱动数据）
3. **大缓冲区放 AXI SRAM**（DMA 友好）
4. **关键函数内联化**（减少栈操作）
5. **禁用 FMC WriteFifo**（确保时序可控）
6. **scatter 文件只用一个 `.ANY`**（避免数据分散）

### 📊 内存分配策略

```
DTCM (128KB):
├─ 堆栈 (4KB Stack + 2KB Heap)
├─ HAL 核心变量 (uwTick, SystemCoreClock)
├─ 关键驱动数据 (FMC, AD7616)
└─ 其他小数据

AXI SRAM (512KB):
├─ ADC 采集缓冲区 (32KB × 2)
├─ DMA 缓冲区 (UART, SPI, I2C)
└─ 其他大数组
```

---

## 总结

### 🎯 核心经验

> **DTCM 不仅仅是"快速 RAM"，更是"总线隔离器"**

通过将堆栈和关键数据放入 DTCM，彻底避免了 CPU 栈操作与 FMC 访问的总线竞争，实现了完美的 FMC 时序。

### 🔑 关键要点

1. **STM32H7 的 DTCM 与 AXI 总线完全隔离**
2. **分散加载文件的 `.ANY` 选择器会导致意外的数据分配**
3. **逻辑分析仪是调试总线问题的最佳工具**
4. **性能提升：从 AXI SRAM 到 DTCM 约 3 倍**

---
