# STM32H743 高速数据采集项目 (AD7616)

## 项目架构

这是一个基于 **STM32H743VITx** 微控制器的高速数据采集系统，使用 AD7616 芯片通过 FMC 总线采集 16 通道模拟信号。

### 分层架构
```
main() → BSP_Init() → Module_Config() → App_Init()
  ↓         ↓              ↓              ↓
Core    BSP Layer    Module Layer   Application Layer
```

**初始化顺序至关重要**：
1. **BSP_Init()**: 硬件抽象层初始化（HAL、时钟、GPIO、FMC、定时器）
2. **Module_Config()**: 功能模块配置（AD7616、按键等）
3. **App_Init()**: 应用层初始化（FreeRTOS 内核、任务创建）

### 目录结构
- **`../BSP/`**: 板级支持包（GPIO、FMC、TIMER 等）
- **`../Modules/`**: 功能模块（Module_AD7616、Module_Key）
- **`../Application/`**: 应用层（App.c、App_TasksInit.c）
- **`../Core/`**: STM32 CubeMX 生成的核心代码
- **`MDK-ARM/`**: Keil uVision 项目文件

## 关键技术点

### FMC 外部存储器接口
- **PSRAM 基地址**: `0x60000000` (FMC Bank1 Sector1)
- **数据宽度**: 16-bit
- **时序模式**: 异步模式 (Mode A)
- AD7616 通过 FMC 总线映射到 PSRAM 地址空间：
  ```c
  #define AD7616_REG_ADDR   (0x60000000)      // A0=0: 寄存器地址
  #define AD7616_DATA_ADDR  (0x60000000 + 2)  // A0=1: 数据地址
  ```

### AD7616 模块
- **16 通道同步采样** ADC (16-bit 分辨率)
- **量程配置**: ±2.5V / ±5V / ±10V (通过寄存器配置)
- **寄存器访问**: 通过 FMC 总线直接读写（无需 SPI/并行接口驱动）
- **批量采集**: 支持 DMA 传输到 PSRAM

### FreeRTOS 配置
- **内核**: CMSIS-RTOS v2 API
- **Heap**: heap_4 (65536 bytes)
- **Tick Rate**: 1000 Hz
- **任务优先级**: 最大 56 级
- **关键任务**:
  - `App_LEDToggle_Task`: LED 指示 (优先级 1)
  - `App_Run10ms_Task`: 10ms 周期任务 (优先级 2)
  - `App_KeyTestTask`: 按键处理 (优先级 2)

## 开发工作流

### 构建与烧录
使用 VS Code 任务系统（基于 Embedded IDE 插件）：
- **构建**: `Ctrl+Shift+P` → `Tasks: Run Task` → `build`
- **烧录**: `Tasks: Run Task` → `flash`
- **构建并烧录**: `Tasks: Run Task` → `build and flash`
- **清理**: `Tasks: Run Task` → `clean`

### 编译环境
- **工具链**: ARM Compiler 5 (ARMCC v5.06)
- **优化级别**: `-O3` (高性能优化)
- **目标芯片**: STM32H743VITx (Cortex-M7, 480MHz, FPU)
- **关键编译选项**: `--c99 --gnu --split_sections --apcs=interwork`

### 调试配置
- **调试器**: ST-Link (配置文件在 `DebugConfig/`)
- **事件记录器**: 可通过 `Enable_EventRecorder` 宏启用 ARM EventRecorder

## 编码约定

### 命名规范
- **BSP 函数**: `BSP_<Peripheral>_<Action>()` (例: `BSP_FMC_PSRAM_Init()`)
- **模块函数**: `Module_<Name>_<Action>()` (例: `Module_AD7616_ReadChannel()`)
- **全局句柄**: 小写下划线命名 (例: `bsp_fmc_psram_handle`)
- **宏定义**: 全大写下划线分隔 (例: `PSRAM_BASE_ADDR`)

### 返回值类型
所有 BSP 和 Module 函数使用统一的状态返回类型：
```c
typedef enum {
  BSP_OK       = 0x00,
  BSP_ERROR    = 0x01,
  BSP_BUSY     = 0x02,
  BSP_TIMEOUT  = 0x03
} BSP_Status_t;

typedef enum {
  Module_OK    = 0x00,
  Module_ERROR = 0x01,
  Module_BUSY  = 0x02
} Module_Status_t;
```

### 头文件包含顺序
1. `"main.h"` (包含 HAL 库和芯片定义)
2. BSP 头文件 (`"bsp.h"`, `"bsp_gpio.h"` 等)
3. Module 头文件 (`"Module.h"`, `"Module_AD7616.h"` 等)
4. FreeRTOS 头文件 (`"FreeRTOS.h"`, `"cmsis_os.h"` 等)
5. 标准库 (`<stdio.h>`, `<string.h>` 等)

## 常见操作

### 添加新的功能模块
1. 在 `../Modules/` 创建 `Module_<Name>.h` 和 `Module_<Name>.c`
2. 在 `Module.c` 的 `Module_Config()` 中调用初始化函数
3. 在 Keil 项目中添加源文件到 `Modules` 组

### 修改 FreeRTOS 任务
- 任务创建在 `App_TasksInit.c` 的 `App_Tasks_Init()` 中
- 使用 `xTaskCreate()` 创建任务（非 CMSIS-RTOS v2 API）
- 任务实现在同一文件中，函数名格式: `App_<TaskName>_Task()`

### 配置 FMC 时序
如需调整 PSRAM/AD7616 访问速度，修改 `BSP_FMC_PSRAM_Init()` 中的时序参数：
- `AddressSetupTime`: 地址建立时间
- `DataSetupTime`: 数据建立时间
- `BusTurnAroundDuration`: 总线转向时间

注意：FMC 时钟源为 HCLK (200MHz)，时序单位为 HCLK 周期。

## 重要注意事项

- **不要直接修改 `Core/Src/` 和 `Core/Inc/` 中的 CubeMX 生成代码**，应在 USER CODE 区域添加自定义逻辑
- **FMC 初始化必须在 AD7616 模块初始化之前完成**
- **FreeRTOS 任务中使用 HAL 库函数时，注意线程安全**（使用互斥锁保护共享资源）
- **PSRAM 地址范围**: 0x60000000 ~ 0x60200000 (2MB)，访问越界会导致 HardFault
- **编译输出**: `.axf` 和 `.hex` 文件在 `Project/` 目录
