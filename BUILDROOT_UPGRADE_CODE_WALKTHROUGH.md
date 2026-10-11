# STM32H743 Buildroot（Bootloader）升级全流程代码总结

> 分析对象：
>
> - `E:\MCU_Project\STM32H743Project\EC-ECL_APP`，Git 分支 `APP`，提交 `82e9062`（`APP_V1.0`）
> - `E:\MCU_Project\STM32H743Project\EC-ECL_BOOT`，Git 分支 `Buildroot`，提交 `1d8da97`（`Buildroot_V1.0`）
>
> 本文中的 “Buildroot” 是仓库分支名，实际实现的是 STM32H743 串口 Bootloader/IAP 升级系统，并不是 Linux Buildroot。

## 1. 最终方案概览

系统把 STM32H743 的 2 MiB 内部 Flash 分成两个主要区域：

| 区域 | 地址范围 | 用途 |
|---|---:|---|
| Boot | `0x08000000 ~ 0x0801FFFF` | Bootloader，链接区大小 128 KiB |
| 未使用间隔 | `0x08020000 ~ 0x080FFFFF` | 当前方案未使用 |
| App 镜像 | `0x08100000 ~ 0x081FFFBF` | Bank2 中的应用程序，最大 `0xFFFC0` 字节 |
| App 信息 | `0x081FFFC0 ~ 0x081FFFDF` | 32 字节 `BootImageInfo_t` |
| 最终提交标记 | `0x081FFFE0 ~ 0x081FFFFF` | 32 字节 `BootImageCommit_t` |

核心设计如下：

1. APP 工程被重新链接到 Bank2 起始地址 `0x08100000`，中断向量表也迁移到该地址。
2. BOOT 工程位于 Bank1 起始地址 `0x08000000`，不会被 Bank2 整片擦除影响。
3. 上电按住 K1（PA0，低电平，连续约 50 ms）进入串口升级服务；未按 K1 时尝试校验并启动 App。
4. 升级通过 USART1（PA9/PA10，115200-8-N-1，轮询模式）完成。
5. 上位机依次发送 `HELLO -> START -> DATA... -> END`。
6. 每一帧有独立 CRC32，完整固件也有一次整体 CRC32。
7. 首个 32 字节 Flash Word（包含初始 MSP 和 Reset_Handler）在 DATA 阶段只暂存在 RAM，完整固件 CRC 正确后才写入。
8. 固件信息、向量表和最终提交标记按顺序提交。升级中途断电时，App 不会被误判为有效。
9. 每次启动 App 前重新检查元数据、提交标记、向量表以及完整固件 CRC32。

最终启动路径可以概括为：

```text
复位
  |
  v
BSP_Init()：最小化 Boot 硬件初始化
  |
  v
连续采样 K1 约 50 ms
  |----------------------------|
  | 按下                       | 未按下
  v                            v
Boot_Service_Run()       Boot_App_IsValid()
  |                            |
串口升级永久循环               | 有效
                               v
                         Boot_App_Jump()
                               |
                               v
                         App Reset_Handler

未按 K1 且 App 无效：停留在 Boot，LED 每 1 秒翻转一次
```

## 2. Git 演进过程：从原工程到 Buildroot_V1.0

### 2.1 起点：`2c2cb13`（`Bootloader前增加`）

这一提交是 APP 和 Buildroot 两个分支的共同基线。原工程仍是普通单镜像应用：

- 程序链接在 `0x08000000`。
- Flash 配置覆盖完整 2 MiB。
- `BSP_Init()` 会初始化 MPU、Cache、串口、USB 等完整应用资源。
- 没有 Boot/App 分区，没有升级协议，没有固件有效性元数据。

从该提交分出两条配合使用的分支：

- `APP`：生成可被 Bootloader 写入 Bank2 的应用固件。
- `Buildroot`：生成位于 Bank1 的 Bootloader。

### 2.2 APP 分支：`82e9062`（`APP_V1.0`）

APP 分支主要完成三项工作。

#### 2.2.1 修改 APP 链接地址

文件：`EC-ECL_APP/MDK-ARM/Project.sct`

```text
原来：LR_IROM1 0x08000000 0x00200000
现在：LR_IROM1 0x08100000 0x000FFFC0

原来：ER_IROM1 0x08000000 0x00200000
现在：ER_IROM1 0x08100000 0x000FFFC0
```

意义：

- App 的加载地址和执行地址都变为 `0x08100000`。
- 最大链接长度限制为 `0x000FFFC0`，主动避开 Bank2 最后 64 字节的固件信息和提交标记。
- 向量表作为 `RESET, +First` 仍位于镜像最前面，因此首两个 32 位字分别是 App 初始 MSP 和 Reset_Handler。

#### 2.2.2 修改 APP 向量表地址

文件：`EC-ECL_APP/Core/Src/system_stm32h7xx.c`

代码启用：

```c
#define USER_VECT_TAB_ADDRESS
```

并把 Cortex-M7 的 Flash 向量表基址从：

```c
FLASH_BANK1_BASE
```

改为：

```c
FLASH_BANK2_BASE
```

`VECT_TAB_OFFSET` 保持 `0`，所以 `SystemInit()` 最终设置：

```c
SCB->VTOR = 0x08100000;
```

这与 APP 链接地址和 Boot 跳转前设置的 VTOR 完全一致。

#### 2.2.3 编译后生成升级用裸二进制

文件：`EC-ECL_APP/MDK-ARM/Project.uvprojx`

Keil After Build 命令被启用：

```text
D:\Keil\Keil_v5\ARM\ARMCC\Bin\fromelf.exe
    --bin
    --output ".\Project\output.bin"
    ".\Project\output.axf"
```

升级时应发送：

```text
EC-ECL_APP\MDK-ARM\Project\output.bin
```

不能直接把带地址记录的 HEX 文本当作 DATA 数据发送。仓库中现有 `output.bin` 大小为 47,408 字节，小于 `BOOT_APP_MAX_SIZE = 1,048,512` 字节。

### 2.3 Buildroot 第一步：`b82b77d`（`初版_可跳转`）

这是第一版可运行 Bootloader，完成了基础链路。

#### 新增升级代码

- `Bootloader/boot_crc32.c/.h`：CRC32/IEEE。
- `Bootloader/boot_protocol.c/.h`：帧收发、命令和错误码。
- `Bootloader/boot_upgrade.c/.h`：擦除 Bank2、顺序写入数据、结束时整包 CRC。
- `BSP/USART/bsp_boot_usart.c/.h`：Boot 专用 USART1 轮询驱动。
- `BSP/GPIO/bsp_gpio.c/.h`：K1 升级按键检测。

#### 修改 Boot 初始化

文件：`BSP/bsp.c`

`BSP_Init()` 从完整 APP 初始化缩减为 Boot 所需的最小集合：

```c
HAL_Init();
SystemClock_Config();
BSP_DWT_DelayInit();
BSP_GPIO_System_Clock_Init();
BSP_GPIO_LED_Init();
```

这样 Boot 不再启动 USB、应用串口、MPU、Cache 和业务模块，减少了跳转前残留状态。

#### 限制 Boot 自身链接区

文件：`EC-ECL_BOOT/MDK-ARM/Project.sct`

Boot 的加载区和执行区从完整 2 MiB 改为：

```text
0x08000000，长度 0x00020000（128 KiB）
```

#### 第一版的能力和不足

第一版已经能够：

- K1 选择升级模式。
- 接收 HELLO、START、DATA、END。
- 擦除整个 Bank2。
- 按 32 字节 Flash Word 编程，并逐 Word 回读。
- END 时计算完整固件 CRC。
- 简单检查 MSP、Reset_Handler 后跳转 App。

但第一版在 DATA 阶段会立即写入首个 Flash Word。若升级中断，向量表可能已经存在；启动校验也只检查向量表范围，没有“完整升级已提交”的持久化证据。这是下一提交重点解决的问题。

### 2.4 Buildroot 第二步：`2fb8013`（`第一版_主要代码都在main里面`）

该提交增加了断电安全和持久化有效性验证。

#### 预留 Bank2 末尾 64 字节

`boot_protocol.h` 增加：

```c
#define BOOT_IMAGE_INFO_ADDRESS   0x081FFFC0UL
#define BOOT_IMAGE_COMMIT_ADDRESS 0x081FFFE0UL
#define BOOT_APP_MAX_SIZE \
    (BOOT_IMAGE_INFO_ADDRESS - BOOT_APP_BASE_ADDRESS)
```

#### 新增固件元数据

新增 `boot_image_info.c/.h`：

- `BootImageInfo_t`：格式版本、固件版本、镜像大小、镜像 CRC、App 基址和信息自身 CRC。
- `BootImageCommit_t`：魔数及其反码、信息 CRC 及其反码、镜像大小及其反码、镜像 CRC 及其反码。
- 两个结构体都有编译期大小检查，必须恰好是一个 32 字节 Flash Word。
- 每次写入后立刻回读比较。

#### 首个 Flash Word 延迟提交

`BootUpgradeContext_t` 增加：

```c
uint32_t first_flash_word[8];
uint8_t first_flash_word_received;
```

DATA 收到 offset 0 时：

- 前 32 字节只复制到 RAM。
- 从 offset 32 开始的数据才写入 Flash。
- 在 END 整包 CRC 通过之前，`0x08100000` 仍保持擦除态 `0xFFFFFFFF`。

#### 增加丢 ACK 后的单包重传容错

`Boot_Upgrade_IsRepeatedDataValid()` 允许“刚刚上一包”被重发：

- 重发包结束位置必须刚好等于当前 `received_size`。
- 首 32 字节与 RAM 暂存区比较。
- 其余数据与 Flash 已写内容比较。
- 内容一致则不重复写 Flash，直接返回当前 `next_offset`，让协议层重新发送 ACK。

#### 强化启动前校验和跳转清理

此时相关代码仍集中在 `main.c` 中，但已经增加：

- 元数据与提交标记校验。
- MSP、Reset_Handler 合法性校验。
- 完整 App CRC32 校验。
- 跳转前 RCC、HAL、SysTick、DWT、Cache、MPU、NVIC 清理。
- ARMCC 汇编跳转，确保切换 MSP 后不再访问 Boot 栈。

### 2.5 Buildroot 第三步：`1d8da97`（`Buildroot_V1.0`）

功能基本不变，主要完成模块化和代码收尾。

#### 从 `main.c` 拆出两个职责模块

- `boot_app.c/.h`：App 有效性校验、跳转前清理、最终汇编跳转。
- `boot_service.c/.h`：升级服务循环、帧错误处理、命令分发、ACK/NACK。

最终 `main.c` 只有 29 行，清楚表达启动策略：

```c
BSP_Init();
BSP_GPIO_BOOT_KEY_Init();

if (BSP_GPIO_BOOT_KEY_IsPressed())
{
    Boot_Service_Run();
}

if (Boot_App_IsValid())
{
    Boot_App_Jump();
}

while (1)
{
    LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_13);
    BSP_DWT_Delay_ms(1000U);
}
```

#### Keil 工程加入新模块

`MDK-ARM/Project.uvprojx` 的 Bootloader 组加入：

- `boot_app.c`
- `boot_service.c`

#### 其他非 Boot 功能性改动

此提交还做了少量编译告警清理：

- 浮点常量增加 `f` 后缀。
- 若干 C 文件补充末尾换行。

这些改动不是升级协议本身的一部分。

## 3. 最终代码逐文件说明

### 3.1 `Core/Src/main.c`：总入口

执行顺序：

1. `BSP_Init()` 初始化 HAL、系统时钟、DWT、基础 GPIO 和 LED。
2. `BSP_GPIO_BOOT_KEY_Init()` 把 PA0 配置为上拉输入。
3. `BSP_GPIO_BOOT_KEY_IsPressed()` 连续检查 5 次，每次间隔 10 ms：
   - 任一次读到高电平，判定未按下。
   - 连续约 50 ms 均为低电平，判定按下。
4. 按下时调用 `Boot_Service_Run()`；该函数正常不会返回。
5. 未按下时调用 `Boot_App_IsValid()`。
6. App 有效时调用 `Boot_App_Jump()`；正常不会返回。
7. App 无效时每 1000 ms 翻转 PC13 LED。

注意：App 无效时不会自动进入串口服务，仍需复位并按住 K1 才能升级。

### 3.2 `BSP/USART/bsp_boot_usart.c`：Boot 串口驱动

`BSP_BOOT_USART1_Init()`：

- USART1 时钟源：PCLK2。
- TX：PA9，RX：PA10，复用 AF7。
- 115200 baud、8 数据位、无校验、1 停止位。
- 不启用 FIFO，不配置 NVIC，不使用中断和 DMA。
- 清除 ORE、FE、NE 错误标志。

`BSP_BOOT_USART1_SendBuffer()`：逐字节等待 TXE，全部写入后只等待一次 TC。

`BSP_BOOT_USART1_ReceiveByte()`：轮询 RXNE，每次调用按 `HAL_GetTick()` 执行超时检查，并在等待期间清除串口错误标志。

### 3.3 `Bootloader/boot_crc32.c`：统一 CRC 算法

实现 CRC32/IEEE：

```text
反射多项式：0xEDB88320
初始值：    0xFFFFFFFF
最终异或：  0xFFFFFFFF
```

`Boot_CRC32_Update()` 支持分段累计；`Boot_CRC32_Calculate()` 完成初始化和最终异或。结果与 Python `zlib.crc32(data) & 0xFFFFFFFF` 一致。

该算法同时用于：

- 协议帧 CRC。
- 完整固件 CRC。
- `BootImageInfo_t` 前 28 字节的元数据 CRC。

### 3.4 `Bootloader/boot_protocol.c/.h`：帧协议

#### 帧格式

所有多字节整数均为小端序。

| 相对字节 | 长度 | 字段 |
|---:|---:|---|
| 0 | 1 | 固定 `0x55` |
| 1 | 1 | 固定 `0xAA` |
| 2 | 1 | 协议版本，当前 `0x01` |
| 3 | 1 | 命令 |
| 4 | 2 | sequence |
| 6 | 2 | payload 长度 |
| 8 | N | payload，最大 1024 字节 |
| 8+N | 4 | CRC32，小端 |

帧 CRC 的计算范围是“协议版本”到 payload 末尾，即不包含 `0x55 0xAA`，也不包含 CRC 字段自身。

#### 收帧代码顺序

`Boot_Protocol_ReceiveFrame()`：

1. 清零输出帧的命令、序号和长度。
2. `Boot_FindFrameMagic()` 在字节流中搜索连续的 `55 AA`，也正确处理连续多个 `55`。
3. `Boot_ReceiveExact()` 接收 6 字节内部帧头。
4. 解析 command、sequence、payload length。
5. 检查协议版本和 payload 最大长度。
6. 接收 payload。
7. 接收 4 字节 CRC。
8. 对 6 字节内部帧头和 payload 分段计算 CRC32。
9. 返回 `OK`、`TIMEOUT`、`VERSION_ERROR`、`LENGTH_ERROR` 或 `CRC_ERROR`。

服务循环传入的超时时间为 1000 ms；这是逐字节等待超时，不是整个帧的总超时。

#### 发帧代码顺序

`Boot_Protocol_SendFrame()`：

1. 检查 payload 参数。
2. 构造 magic 和 6 字节内部帧头。
3. 计算内部帧头及 payload 的 CRC32。
4. 依次发送 magic、内部帧头、payload 和 CRC。

### 3.5 `Bootloader/boot_service.c`：服务循环与命令分发

`Boot_Service_Run()`：

1. 使用 `static BootFrame_t received_frame` 保存约 1 KiB 的接收帧，避免占用函数栈。
2. 初始化 USART1。
3. 调用 `Boot_Upgrade_Init()` 把升级状态置为 IDLE。
4. 永久循环收帧。
5. 超时直接继续等待。
6. 版本、长度或帧 CRC 错误分别回复对应 NACK。
7. 合法帧交给 `Boot_Service_DispatchFrame()`。

`Boot_Service_DispatchFrame()`：

- HELLO：payload 必须为空，回复设备能力。
- START：调用 `Boot_Upgrade_HandleStart()`，成功回复 ACK/START/offset 0，否则回复 NACK。
- DATA：调用 `Boot_Upgrade_HandleData()`，成功回复 ACK/DATA/next_offset，否则回复 NACK。
- END：调用 `Boot_Upgrade_HandleEnd()`，成功回复 ACK/END/image_size，否则回复 NACK。
- 其他命令：回复 `BOOT_ERROR_COMMAND`。

### 3.6 `Bootloader/boot_upgrade.c`：升级状态机和 Flash 写入

状态机：

```text
Boot_Upgrade_Init()
        |
        v
      IDLE
        |
  START成功并擦除Bank2
        |
        v
   RECEIVING
        |
 DATA按offset顺序接收
        |
 END长度、CRC、元数据、向量表、提交标记全部成功
        |
        v
    COMPLETE
```

任何 Flash 编程失败或整包 CRC 错误都会重新调用 `Boot_Upgrade_Init()`，回到 IDLE；上位机必须重新发送 START，而 START 会再次整片擦除 Bank2。

#### `Boot_Upgrade_Init()`

清空：

- 状态。
- 目标镜像大小和期望 CRC。
- 已接收字节数。
- 写地址。
- 首个 32 字节暂存区，并填为 `0xFF`。
- 首 Word 已接收标志。

#### `Boot_Upgrade_HandleStart()`

START payload 必须是 8 字节：

| 偏移 | 长度 | 含义 |
|---:|---:|---|
| 0 | 4 | `image_size` |
| 4 | 4 | `image_crc32` |

代码执行顺序：

1. 检查 frame 指针。
2. 要求 payload 长度恰好为 8。
3. 小端读取镜像长度和整体 CRC。
4. 镜像长度必须在 32 到 `BOOT_APP_MAX_SIZE` 之间。
5. 重置旧升级会话。
6. 解锁 Flash。
7. 清除 Bank2 错误标志。
8. 使用 `FLASH_TYPEERASE_MASSERASE + FLASH_BANK_2` 擦除整个 Bank2。
9. 重新锁定 Flash。
10. 擦除失败则回到 IDLE 并返回 `FLASH_ERASE`。
11. 成功则进入 RECEIVING，保存 image_size、expected_crc32，接收偏移归零。

重要结果：START 一旦成功执行擦除，旧 App 和旧提交标记都会消失；系统没有保留旧固件，也没有 A/B 回滚。

#### `Boot_Upgrade_HandleData()`

DATA payload：

| 偏移 | 长度 | 含义 |
|---:|---:|---|
| 0 | 4 | 固件 offset |
| 4 | 1~992 | 固件数据 |

约束：

- payload 总长必须大于 4 且不超过 996。
- offset 必须按 32 字节对齐。
- offset 和数据长度不能超出 START 声明的 image_size。
- 非最后一包的数据长度必须是 32 的整数倍。
- 最后一包可不足 32 字节，写 Flash 时剩余字节填 `0xFF`。
- 第一包必须从 offset 0 开始且至少包含 32 字节。

处理顺序：

1. 输出参数 `next_offset` 先设为当前 `received_size`。
2. 要求状态是 RECEIVING。
3. 检查 payload、offset、镜像边界和 Flash Word 对齐。
4. 若 offset 等于 `received_size`，这是正常新包。
5. 若 offset 小于 `received_size`，只接受“上一包恰好重发”且内容完全一致的情况；不重新编程，直接再次 ACK。
6. 其他 offset 返回 `BOOT_ERROR_SEQUENCE`。
7. 解锁 Flash并清除 Bank2 错误标志。
8. offset 为 0 时，把前 32 字节复制到 `first_flash_word`，不写入 Flash。
9. 后续数据按 32 字节切分到本地 `flash_word[8]`。
10. 每个 Flash Word 先填 `0xFF`，再拷贝实际数据，处理最后不足 32 字节的情况。
11. 调用 `HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, ...)`。
12. 立即用 `memcmp()` 回读整个 32 字节 Flash Word。
13. 全部写完后锁定 Flash。
14. 任一写入或回读失败：会话回到 IDLE，返回 `FLASH_PROGRAM`。
15. 成功则增加 `received_size`，返回新的 `next_offset`。

#### `Boot_Upgrade_HandleEnd()`

END 不允许携带 payload。代码执行顺序：

1. 检查 frame 和输出参数。
2. 要求状态为 RECEIVING。
3. 要求 `received_size == image_size`。
4. 要求首个 32 字节已收到并暂存在 RAM。
5. 计算完整镜像 CRC：
   - 前 32 字节取自 RAM 的 `first_flash_word`。
   - 剩余字节从 `0x08100020` 开始的 Bank2 Flash 读取。
6. 与 START 中的 expected_crc32 比较。
7. CRC 不同：不写向量表，回到 IDLE，返回 `IMAGE_CRC`。
8. CRC 正确：先调用 `Boot_ImageInfo_WriteInfo()` 写 `0x081FFFC0`。
9. 再把 RAM 中首个 32 字节写到 `0x08100000`，并回读验证。
10. 最后调用 `Boot_ImageInfo_WriteCommit()` 写 `0x081FFFE0`。
11. 全部成功后进入 COMPLETE，返回固件 CRC 和大小。

这个提交顺序保证：

```text
DATA 中途断电：向量表仍是擦除态，提交标记不存在 -> 无效
写 info 后断电：提交标记不存在 -> 无效
写向量表后断电：提交标记不存在 -> 无效
写 commit 成功：元数据、镜像和向量表均已落盘 -> 可进入启动校验
```

### 3.7 `Bootloader/boot_image_info.c/.h`：持久化提交记录

#### `BootImageInfo_t`，地址 `0x081FFFC0`

| 字段 | 含义 |
|---|---|
| `format_version` | 当前为 1 |
| `firmware_version` | 当前协议未传版本，写 0 |
| `image_size` | 实际 bin 长度 |
| `image_crc32` | 完整 bin 的 CRC32 |
| `app_base_address` | 必须为 `0x08100000` |
| `reserved[2]` | 保持擦除态 `0xFFFFFFFF` |
| `info_crc32` | 前 28 字节的 CRC32 |

#### `BootImageCommit_t`，地址 `0x081FFFE0`

| 字段 | 校验方式 |
|---|---|
| `magic` | `0x544F4F42`，内存字节为 `B O O T` |
| `magic_inverse` | `~magic` |
| `info_crc32` | 必须等于 info 中的 CRC |
| `info_crc32_inverse` | 必须为反码 |
| `image_size` | 必须等于 info 中的大小 |
| `image_size_inverse` | 必须为反码 |
| `image_crc32` | 必须等于 info 中的镜像 CRC |
| `image_crc32_inverse` | 必须为反码 |

`Boot_ImageInfo_ProgramFlashWord()` 统一执行地址对齐检查、Flash 解锁、错误标志清除、32 字节编程、锁定和整 Word 回读。

`Boot_ImageInfo_ReadValid()` 先验证 info 自身，再逐项验证 commit 与 info 的对应关系。该函数只验证元数据一致性；完整 App CRC 由 `Boot_App_IsValid()` 再计算一次。

### 3.8 `Bootloader/boot_app.c`：启动校验和跳转

#### `Boot_App_IsValid()`

按以下顺序检查：

1. `Boot_ImageInfo_ReadValid()`：info 和 commit 必须存在且一致。
2. 从 `0x08100000` 读取初始 MSP，从 `0x08100004` 读取 Reset_Handler。
3. 两者都不能是擦除态 `0xFFFFFFFF`。
4. MSP 必须位于 DTCM `0x20000000 ~ 0x20020000`，允许等于末地址。
5. MSP 必须 8 字节对齐。
6. Reset_Handler 必须设置 Thumb 位，即 bit0 为 1。
7. 去掉 Thumb 位后的地址必须位于 `0x08100000 ~ 0x081FFFBF`。
8. 按元数据中的 image_size 重新计算完整 App CRC32。
9. 计算结果必须与元数据中的 image_crc32 一致。

只有全部通过才返回 1。

#### `Boot_App_PrepareForJump()`

跳转前依次：

1. 在 HAL Tick 仍可用时执行 `HAL_RCC_DeInit()`。
2. 执行 `HAL_DeInit()`，复位总线外设。
3. 全局关中断。
4. 关闭 Boot 使用过的 USART1、TIM6、GPIOA/C/H 时钟。
5. 清零 SysTick CTRL、LOAD、VAL。
6. 停止并清零 DWT CYCCNT。
7. 若 I-Cache/D-Cache 已开启则关闭。
8. 禁用 MPU。
9. 禁用并清除全部 8 组 NVIC 外部中断。
10. 清除 PendSV 和 SysTick 挂起状态。
11. 恢复 NVIC 优先级分组为复位值。
12. 执行 DSB、ISB。

#### `Boot_App_Start()` 与 `Boot_App_Jump()`

`Boot_App_Jump()` 在清理环境之前读出 App MSP 和 Reset_Handler，然后：

1. 调用 `Boot_App_PrepareForJump()`。
2. 设置 `SCB->VTOR = 0x08100000`。
3. 调用 ARMCC 汇编函数 `Boot_App_Start()`。

汇编函数执行：

```text
CONTROL   = 0    -> 特权线程模式，使用 MSP
BASEPRI   = 0
FAULTMASK = 0
PSP       = 0
MSP       = App 初始 MSP
CPSIE I          -> 恢复全局中断
BX Reset_Handler -> 跳入 App，正常不返回
```

把最终切 MSP 和分支放在汇编中非常重要：一旦 MSP 改成 App 栈，普通 C 函数不能再安全访问 Boot 的局部变量或执行标准函数尾声。

## 4. 升级协议命令详解

### 4.1 命令和响应

| 值 | 名称 | Payload | 作用 |
|---:|---|---|---|
| `0x01` | HELLO | 空 | 查询 Boot 能力 |
| `0x02` | START | size(4) + crc32(4) | 开始新会话并擦除 Bank2 |
| `0x03` | DATA | offset(4) + data(1~992) | 顺序发送固件块 |
| `0x04` | END | 空 | 校验并提交固件 |
| `0x05` | ABORT | 当前未实现 | 代码只定义枚举，发送后会得到 COMMAND NACK |
| `0x81` | HELLO_RSP | 11 字节 | 返回版本、App 基址、最大镜像、最大 payload |
| `0x82` | ACK | command(1) + next_offset(4) | 命令成功 |
| `0x83` | NACK | error(1) | 命令失败 |

HELLO_RSP payload：

| 偏移 | 长度 | 当前值 |
|---:|---:|---:|
| 0 | 1 | 协议版本 `1` |
| 1 | 4 | App 地址 `0x08100000` |
| 5 | 4 | 最大镜像 `0x000FFFC0` |
| 9 | 2 | 最大协议 payload `1024` |

### 4.2 错误码

| 值 | 名称 | 触发条件 |
|---:|---|---|
| `0x00` | NONE | 无错误 |
| `0x01` | FRAME_TIMEOUT | 定义存在；服务对普通接收超时选择静默继续 |
| `0x02` | FRAME_HEADER | 当前实现未实际返回 |
| `0x03` | FRAME_VERSION | 协议版本错误 |
| `0x04` | FRAME_LENGTH | payload 或 DATA 长度非法 |
| `0x05` | FRAME_CRC | 帧 CRC 错误 |
| `0x06` | COMMAND | 命令/状态/参数指针非法或未实现命令 |
| `0x07` | SEQUENCE | DATA offset 错序或未通过重传验证 |
| `0x08` | IMAGE_SIZE | 镜像大小或数据范围非法 |
| `0x09` | IMAGE_CRC | 完整固件或元数据 CRC 错误 |
| `0x0A` | FLASH_ERASE | Bank2 解锁/擦除失败 |
| `0x0B` | FLASH_PROGRAM | Flash 编程或回读失败 |

`sequence` 字段只在响应中原样回显，用于上位机匹配请求和响应；设备端没有用 sequence 判断 DATA 顺序，真正的顺序控制依据是 DATA payload 中的 32 位 offset。

## 5. 从生成固件到启动新 App 的完整操作顺序

### 步骤 1：编译 APP

用 Keil 编译 `EC-ECL_APP/MDK-ARM/Project.uvprojx`。链接器把镜像放在 `0x08100000`，After Build 生成：

```text
EC-ECL_APP\MDK-ARM\Project\output.bin
```

### 步骤 2：上位机准备镜像参数

读取整个 `output.bin`：

```text
image_size  = 文件实际字节数
image_crc32 = CRC32/IEEE(output.bin)
```

Python 算法对应关系：

```python
image_crc32 = zlib.crc32(firmware_bytes) & 0xFFFFFFFF
```

当前仓库没有提供实际串口升级上位机程序；以上位机实现时必须按本文帧格式自行封包。

### 步骤 3：让设备进入 Boot 升级模式

1. 按住 K1，使 PA0 保持低电平。
2. 复位或重新上电。
3. Boot 连续采样约 50 ms 后进入 `Boot_Service_Run()`。
4. 可以松开 K1；升级服务已进入永久循环。

### 步骤 4：发送 HELLO

发送空 payload 的 HELLO，确认收到 HELLO_RSP，并检查：

- 协议版本为 1。
- App 基址为 `0x08100000`。
- `image_size <= max_image_size`。

### 步骤 5：发送 START

payload 为：

```text
little_endian_u32(image_size)
+ little_endian_u32(image_crc32)
```

设备擦除整个 Bank2。收到 ACK 后，ACK 中 `next_offset` 应为 0。

### 步骤 6：循环发送 DATA

推荐每包发送 992 字节，因为 992 是 32 的整数倍，并符合当前代码的最大 DATA 数据长度。

```text
offset = 0
while offset < image_size:
    chunk = firmware[offset : offset + 992]
    payload = little_endian_u32(offset) + chunk
    发送 DATA
    等待 ACK
    offset = ACK.next_offset
```

除最后一包外，chunk 必须是 32 字节整数倍。最后一包可以是任意剩余长度。

若 DATA 的 ACK 丢失，可以原样重发刚才那一包。设备会验证 RAM/Flash 中的内容，内容一致时重新 ACK，但不会再次写 Flash。

收到 NACK 后：

- `SEQUENCE`：以设备最近一次 ACK 的 next_offset 为准，只允许重发刚才一包或继续正确 offset。
- `FLASH_PROGRAM`：设备已回到 IDLE，必须从 START 重新开始。
- 其他数据/CRC错误：修正帧后按协议状态决定是否重发；无法确定时应重新 START。

### 步骤 7：发送 END

所有 DATA 被 ACK 且 `next_offset == image_size` 后，发送空 payload 的 END。

设备会完成：

```text
完整镜像CRC
 -> 写固件信息
 -> 写首个32字节/向量表
 -> 写最终提交标记
 -> ACK END，next_offset字段携带完成的image_size
```

只有收到 END ACK 才能认为固件已经提交。

### 步骤 8：启动新 App

当前代码在 END 成功后不会自动复位，也不会自动跳转，仍停留在升级服务循环中。因此需要：

1. 不按 K1。
2. 手动复位或重新上电。
3. Boot 读取 info/commit，验证向量表并重新计算完整 App CRC。
4. 校验通过后清理 Boot 环境并跳转到 App Reset_Handler。

## 6. 编译产物和现有验证证据

仓库中的最近构建日志显示：

### APP

```text
Program Size: Code=45492 RO-data=1448 RW-data=29284 ZI-data=96420
0 Error(s), 11 Warning(s)
output.bin: 47,408 bytes
```

APP 的 11 个告警包括源文件末尾换行、单精度转双精度以及空 Scatter 区域等。Buildroot 最终提交修复了其中部分源码告警，但 APP 分支自身的现有构建日志仍保留这些告警。

### BOOT

```text
Program Size: Code=19236 RO-data=880 RW-data=20608 ZI-data=24232
0 Error(s), 10 Warning(s)
```

BOOT 的告警主要来自空 Scatter 区域或被链接器移除的未使用段，没有编译错误。

以上只能证明对应 Keil 构建曾成功完成；仓库中未发现自动化单元测试、串口上位机测试脚本、断电注入测试记录或硬件在环测试报告。

## 7. 当前实现的安全性特点

- Boot 位于 Bank1，升级只擦除 Bank2。
- 帧级 CRC 防止串口传输错误进入命令处理。
- 镜像级 CRC 验证完整 bin。
- Flash 每个 32 字节 Word 写完立即回读。
- 首向量 Word 延迟到 END 整包校验成功后再写。
- 最终 commit 最后写入，支持识别未完成升级。
- 元数据有自身 CRC，提交字段同时存原值和反码。
- 每次启动都重新计算 App 完整 CRC，不只信任持久化标记。
- 跳转前清理时钟、外设、中断、Cache、MPU 和 CPU 屏蔽状态。
- 支持上一 DATA 包因 ACK 丢失而安全重传，避免重复编程 Flash。

## 8. 当前限制和后续改进点

1. **没有 A/B 固件或回滚。** START 会立即擦除整个 Bank2，旧 App 不再可用。
2. **没有签名认证。** CRC 只能检查传输/存储错误，不能防止恶意固件。产品化应增加数字签名和公钥验证。
3. **没有防降级和版本策略。** `firmware_version` 固定写 0，协议没有传版本。
4. **ABORT 未实现。** `BOOT_CMD_ABORT` 已定义，但服务层没有分支处理。
5. **END 后不自动复位。** 必须由人工或上位机触发硬件复位/重新上电。
6. **App 无效时不自动开放升级。** 未按 K1 且 App 无效时只闪 LED。
7. **没有配套升级工具。** 仓库没有 Python/C#/Qt 等上位机发送端。
8. **sequence 不参与设备端时序判断。** 只回显；DATA 顺序完全依赖 offset。
9. **只容忍刚才上一包重发。** 不支持任意历史块重发或断电续传。
10. **`write_address` 当前只被维护但未参与实际写地址计算。** 实际地址由 `BOOT_APP_BASE_ADDRESS + offset + processed` 得到，可考虑删除冗余字段。
11. **部分协议枚举未实际使用。** `BOOT_ERROR_FRAME_HEADER` 当前没有产生路径，`FRAME_TIMEOUT` 对普通空闲超时也不会回复。
12. **工程存在绝对 Keil 路径。** APP After Build 固定使用 `D:\Keil\Keil_v5\...\fromelf.exe`，换开发机可能需要调整。
13. **硬件测试证据不足。** 建议至少验证正常升级、每个 Flash Word 边界、最大镜像、最后不足 32 字节、DATA ACK 丢失、CRC 错误以及 END 三个提交阶段分别断电。

## 9. 建议的验收测试清单

| 测试 | 预期结果 |
|---|---|
| 正常 HELLO/START/DATA/END | END ACK，复位后进入新 App |
| HELLO 版本错误 | NACK `FRAME_VERSION` |
| 单帧 CRC 错误 | NACK `FRAME_CRC`，不处理 payload |
| START size 小于 32 或超过上限 | NACK `IMAGE_SIZE` |
| DATA offset 非 32 字节对齐 | NACK `SEQUENCE` |
| 非最后 DATA 长度非 32 倍数 | NACK `FRAME_LENGTH` |
| 最后一包不足 32 字节 | 成功写入，尾部以 `0xFF` 补齐 |
| 丢弃一次 DATA ACK 并原样重发 | 不重复编程，返回相同 next_offset |
| 重发包内容被篡改 | NACK `SEQUENCE` |
| 整包 CRC 错误 | END NACK `IMAGE_CRC`，向量表保持擦除态 |
| DATA 中途断电 | 不按 K1 复位后不得启动 App |
| 写 info 后、写 commit 前断电 | 提交标记无效，不得启动 App |
| 固件区任意 1 bit 损坏 | 启动时完整 CRC 失败，不得跳转 |
| K1 按住复位 | 无论 App 是否有效都进入升级服务 |
| K1 松开且 App 有效 | 清理 Boot 环境后正常运行 App |
| K1 松开且 App 无效 | LED 每秒翻转，不跳转 |

## 10. 一句话总结

这套 Buildroot_V1.0 已形成完整的“Bank2 APP 产物生成 -> K1 进入 Boot -> 串口分包传输 -> Flash Word 编程与回读 -> 整包 CRC -> 元数据/向量表/提交标记三阶段提交 -> 下次复位完整校验 -> 安全跳转”闭环；其重点不是复杂的双分区回滚，而是用首向量 Word 延迟写入和最终提交标记，保证单 Bank2 升级中断时不会把半成品 App 当作有效固件启动。
