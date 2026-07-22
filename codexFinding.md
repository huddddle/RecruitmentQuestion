# 串口四电机驱动板代码梳理与封装建议

本报告只分析代码并给出后续整理方案。除本文件外，没有修改工程代码、SysConfig 或生成文件。

## 结论

你贴出的部分不是控制算法本体，而是一个给串口电机驱动板做联调的“加速 - 减速循环演示”。它每 200 ms 向四电机同时发送相同的负速度：`-10, -15, ... -50, -45, ...`，循环往复。

真正的电机通信驱动已经封装在 `Drivers/Motor/drive_board.c`：它用 UART2 + DMA 发送 Modbus RTU 帧，用 UART 中断接收驱动板主动上报的编码器帧。`main.c` 中混入了启动配置、通信维护、编码器读取和演示状态机，因而显得杂乱。

## 现有结构

```text
main.c
  DriveBoard_Init()               启动 UART 中断，并投递闭环/PID 配置帧
  while (1)
    DriveBoard_Process()          到发送间隔时启动下一帧 DMA
    DriveBoard_GetEncoderCounts() 取走已收到的编码器快照
    DriveBoard_SetSpeeds()        演示代码：每 200 ms 投递一帧四电机速度

drive_board.c
  业务 API -> 组 Modbus 帧 -> 8 深度发送队列 -> DMA(UART2 TX)
                                              |
                                      UART2 EOT 中断取下一帧

UART2 RX 中断 -> Modbus 0x03 帧解析 -> 编码器快照
```

硬件由 SysConfig 配置为 `UART2`、115200 bps、`PB17 = TX`、`PA24 = RX`、TX DMA 通道 3。见 `mspm0-modules.syscfg:351`。

## 你贴出的代码在做什么

### 1. 条件编译块

```c
#if DRIVE_BOARD_DEMO_ENABLE
...
#endif
```

`DRIVE_BOARD_DEMO_ENABLE` 在 `main.c` 中为 `1U`，所以这段会参与编译。改成 `0U` 时，整个“加减速演示”会被排除，但底层驱动仍会初始化、收发数据。

### 2. 启动阶段

```c
driveBoardNextUpdateMs = (uint32_t)tick_ms;
```

`tick_ms` 是由 SysTick 每 1 ms 增加的系统毫秒计数。这里把下一次发速度的时间设为“现在”，因此进入主循环后会立即投递第一条速度命令。

上方两条被注释的调用含义如下：

```c
DriveBoard_SetEncoderPolarity(1U, true); // 反转第 1 路编码器方向，索引从 1 开始
DriveBoard_ClearEncoders();              // 清零四路驱动板内的编码器累计值
```

它们并不直接修改本地变量，而是向地址 `0x0A` 的驱动板写 Modbus 寄存器。不要默认全部反转编码器方向；应根据实际正向运动时的计数符号逐路决定。

### 3. `DriveBoard_Process()`

它必须在主循环高频调用，但本身不等待、不延时、不直接发业务命令。它只检查前一帧发送后的等待时间是否结束；如果结束，就从发送队列取出下一帧并启动 DMA。

原因是底层约定：速度帧之间至少间隔 2 ms，闭环/PID 配置帧之间至少间隔 20 ms。发送完成事件由 `MSPMotor_INST_IRQHandler()` 处理，中断记录截止时间；下一帧必须由 `DriveBoard_Process()` 接力启动。

因此，不能把它放进偶尔才执行一次的任务分支，也不能用阻塞延时替代它。

### 4. `DriveBoard_GetEncoderCounts()`

```c
if (DriveBoard_GetEncoderCounts(driveBoardEncoderCounts)) {
    /* 使用四路编码器值 */
}
```

驱动板通过 UART2 主动上报合法的 Modbus `0x03` 帧后，RX 中断会校验 CRC，并更新内部的四个 `gEncoderCounts`。这个函数在短暂关闭中断的情况下复制快照，返回值表示“自上次读取后是否收到过新帧”。

`driveBoardEncoderCounts[0..3]` 目前只是局部数组，读取后没有参与任何里程计、速度环或显示，因此对现有控制逻辑没有影响。

### 5. 加速 - 减速演示

```c
if (tick_ms >= driveBoardNextUpdateMs) {
    command = -driveBoardSpeed;
    DriveBoard_SetSpeeds(command, command, command, command);
    driveBoardNextUpdateMs = tick_ms + 200;
    ...在 10 和 50 之间改变 driveBoardSpeed...
}
```

这是一个非阻塞的锯齿波状态机：

| 时刻 | 发给四路电机的值 |
| --- | --- |
| 0 ms | `-10` |
| 200 ms | `-15` |
| ... | 每次增加 5 |
| 1600 ms 左右 | `-50` |
| 后续 | `-45, -40, ... -10`，然后重复 |

负号代表反方向，但正反方向的物理含义还取决于驱动板和电机接线。四个参数相同，所以它不测试独立四轮控制、差速转向或编码器闭环效果。

`DriveBoard_SetSpeeds()` 的返回值只表示“速度帧是否成功放进本地队列”，不表示 DMA 已发送，也不表示驱动板已经执行。当前代码忽略返回值；如果队列已满，会丢掉该次速度更新，并递增 `gDriveBoardQueueOverflowCount`。

## 当前驱动的公共 API

`drive_board.h` 公开的接口已经可以视为底层驱动层，不应让 `main.c` 接触 CRC、DMA 地址、寄存器号或中断细节。

| API | 含义 | 调用结果 |
| --- | --- | --- |
| `DriveBoard_Init()` | 初始化队列、启用 UART2 中断，并按当前宏投递闭环/PID 配置 | 配置帧已入队 |
| `DriveBoard_Process()` | 推进帧间隔和下一帧 DMA | 无阻塞 |
| `DriveBoard_SetClosedLoop()` | 写闭环使能寄存器 | 命令已入队/队满失败 |
| `DriveBoard_SetPID4()` | 一次写四路 PID，共 12 个寄存器值 | 命令已入队/队满失败 |
| `DriveBoard_SetSpeeds(m1,m2,m3,m4)` | 一次写四路速度 | 命令已入队/队满失败 |
| `DriveBoard_SetEncoderPolarity(1..4, flag)` | 写某一路编码器反向位 | 命令已入队/队满失败 |
| `DriveBoard_ClearEncoders()` | 清零四路累计编码器 | 命令已入队/队满失败 |
| `DriveBoard_GetEncoderCounts(out)` | 复制最新四路编码器快照 | `true` 表示有新数据 |
| `DriveBoard_IsTxIdle()` | 查询本地发送队列和间隔是否为空 | 不等于设备确认 |

下列内容是正确的内部实现细节，业务层不应直接调用：`DriveBoard_CRC16`、`DriveBoard_QueueFrame`、`DriveBoard_SendFrame`、`DriveBoard_WriteSingle`、`DriveBoard_WriteMultiple`、`DriveBoard_ParseByte`、`DriveBoard_StartNextTransfer` 和 `MSPMotor_INST_IRQHandler`。

## 通信过程

### 发送

1. `DriveBoard_SetSpeeds()` 将 4 个有符号速度按 16 位补码写入寄存器 `0x0000` 起始位置。
2. `DriveBoard_WriteMultiple()` 组装 Modbus `0x10` 帧。
3. `DriveBoard_SendFrame()` 追加 Modbus CRC16。
4. `DriveBoard_QueueFrame()` 将帧复制进 8 深度环形队列。
5. 队列空闲时，DMA 将队首数据搬运至 `UART2->TXDATA`。
6. UART 的 EOT 中断取走队首，并设置 2 ms 或 20 ms 间隔。
7. 主循环后续调用 `DriveBoard_Process()`，才会启动间隔后的下一帧。

### 接收

1. UART2 RX 中断读空 FIFO。
2. `DriveBoard_ParseByte()` 等待地址 `0x0A`、功能码 `0x03`、字节数和完整帧。
3. CRC 正确时，将 1 至 4 路编码器计数保存到内部快照。
4. 业务层用 `DriveBoard_GetEncoderCounts()` 取走快照。

该实现不主动发送 Modbus 读取命令，依赖驱动板主动上报 `0x03` 帧。若驱动板实际上不会主动上报，编码器数组就永远不会更新；这不是 `GetEncoderCounts()` 的问题，而是协议行为需要和驱动板文档确认。

## 需要注意的问题

1. `DriveBoard_Init()` 同时承担“底层初始化”和“自动写入闭环/PID”两件事，职责混合。程序启动时它会连续投递两帧配置，但没有等待驱动板应答，因此只能说明帧已发送，不能证明参数已生效。

2. PID、闭环开关和演示开关散落在 `drive_board.c`、`main.c` 两处宏中。实际项目应把它们集中到一份应用配置，而不是让驱动文件内嵌某套默认调参值。

3. 演示代码在 `main.c` 内持有 `driveBoardSpeed`、`driveBoardAccelerating` 和 `driveBoardNextUpdateMs`。这些变量不属于系统主循环，也不属于通信驱动，应该移到独立的测试模块。

4. 业务代码若每次主循环都调用 `DriveBoard_SetSpeeds()`，会比 UART 发得更快，最终填满 8 帧队列。速度命令应仅在目标改变时发送，或由上层固定频率限流；队满时应保留“最新目标”，而不是无差别堆积旧目标。

5. 当前演示忽略 `DriveBoard_SetSpeeds()` 的返回值。联调阶段至少应观察 `gDriveBoardQueueOverflowCount`；它不为零说明调用频率或上层策略有问题。

6. 通讯异常时，驱动层没有暴露“紧急停车”语义。应用层至少应明确在退出测试、失联或故障时投递一次 `0, 0, 0, 0` 速度。驱动板是否带独立通信超时停车，需由其协议确认。

7. `main.c` 仍同时调用旧 TB6612 接口 `Left_Control(1, 0)` / `Right_Control(1, 0)`。它们和串口驱动板当前代码没有软件资源冲突，但会使“最终究竟由哪套硬件驱动电机”不清晰。应在应用层明确选择一种电机后端。

## 建议的分层

保留 `drive_board.c/.h` 作为“协议与硬件驱动层”，不让上层了解 Modbus 寄存器、DMA 或 UART2。未来新增两个文件即可，不需要把业务状态机塞回底层驱动。

```text
Drivers/Motor/drive_board.c/.h       协议、队列、DMA、UART 中断
Services/drive_board_service.c/.h    配置、最新速度缓存、编码器快照、故障策略
Tests/drive_board_demo.c/.h          仅用于锯齿波联调
main.c                               只初始化并周期调用 Service_Update
```

### 建议的服务层接口

以下是建议接口，不是本次对工程的修改：

```c
typedef struct {
    int16_t motor[4];
} DriveBoard_Speeds_t;

typedef struct {
    int16_t count[4];
} DriveBoard_Encoders_t;

typedef struct {
    bool enableClosedLoop;
    DriveBoard_PID_t pid[4];
} DriveBoard_Config_t;

typedef enum {
    DRIVE_BOARD_SERVICE_STARTING,
    DRIVE_BOARD_SERVICE_READY,
    DRIVE_BOARD_SERVICE_FAULT
} DriveBoard_ServiceState_t;

void DriveBoardService_Begin(const DriveBoard_Config_t *config);
void DriveBoardService_Update(void);
DriveBoard_ServiceState_t DriveBoardService_GetState(void);

void DriveBoardService_SetAllSpeed(int16_t speed);
void DriveBoardService_SetSpeeds(const DriveBoard_Speeds_t *speeds);
void DriveBoardService_Stop(void);
bool DriveBoardService_TakeEncoders(DriveBoard_Encoders_t *encoders);
```

服务层应遵守这些规则：

- `SetAllSpeed()` 和 `SetSpeeds()` 只更新“期望速度”，不立刻无条件灌入队列。
- `Update()` 内部始终先调用 `DriveBoard_Process()`，再在底层空闲且速度确实变化时发送一帧四路速度。
- 四路速度作为一个结构体整体保存，因为底层协议本来就是一次写四个连续速度寄存器。
- `Stop()` 将期望速度改为全零，并让 `Update()` 只发送一次全零帧。
- `Begin()` 提交闭环和 PID 配置；只有本地队列清空后才进入 `READY`。该状态仍只是“本机已发完”，若设备需要确认，应在协议层增加应答/超时机制。
- 业务侧统一采用数组下标 `0..3`；若底层仍保持 `SetEncoderPolarity(1..4)`，由服务层转换，避免混用两套索引。

### 建议的主循环调用形态

整理后，`main.c` 不应包含 PID 常量、DMA 时序或锯齿波变量，逻辑会接近下面这样：

```c
DriveBoardService_Begin(&driveBoardConfig);

while (1) {
    DriveBoardService_Update();

    if (DriveBoardService_TakeEncoders(&encoders)) {
        /* 在这里更新里程计、显示或闭环输入 */
    }

    if (DriveBoardService_GetState() == DRIVE_BOARD_SERVICE_READY) {
        DriveBoardService_SetAllSpeed(-30);
    }
}
```

这里重复调用 `SetAllSpeed(-30)` 也不会重复发送，因为服务层只在目标值改变时下发。这是“简便调用”真正需要解决的问题。

## 将演示从主程序移走

加减速逻辑应作为可开关的测试模块，例如：

```c
void DriveBoardDemo_Begin(void);
void DriveBoardDemo_Update(void);
```

`DriveBoardDemo_Begin()` 初始化测试速度、方向和下一次更新时间。`DriveBoardDemo_Update()` 每 200 ms 更新一次期望速度，并调用 `DriveBoardService_SetAllSpeed()`。主循环只需在明确启用联调模式时调用它：

```c
DriveBoardDemo_Begin();

while (1) {
    DriveBoardService_Update();
    DriveBoardDemo_Update();
}
```

量产/比赛逻辑只移除这一行 `DriveBoardDemo_Update()`，不会误删串口通信维护或编码器接收。

## 推荐的后续整理顺序

1. 先确认串口驱动板是否会主动上报 `0x03` 编码器帧，以及“速度为负”对应的实际转向。
2. 选择唯一的电机后端：旧 TB6612 或串口四电机驱动板；避免两个入口同时存在于业务主流程。
3. 将闭环开关和 4 路 PID 从 `drive_board.c` 顶部宏提取为应用配置结构。
4. 新建服务层，集中处理初始化状态、最新速度、停止策略、编码器快照和队满统计。
5. 将锯齿波测试移入独立 demo 模块，并默认关闭。
6. 最后再把编码器快照接入真正需要它的里程计或速度控制；在此之前，读取数组不会改变车辆行为。

## 相关源文件

- `main.c:58-62`：演示参数宏。
- `main.c:73-76`：演示状态和编码器局部缓存。
- `main.c:104-155`：本报告分析的启动、维护、读取和锯齿波演示。
- `Drivers/Motor/drive_board.h:23-41`：底层公共 API 和诊断计数。
- `Drivers/Motor/drive_board.c:86-196`：DMA 队列与 Modbus 发送封装。
- `Drivers/Motor/drive_board.c:209-262`：主动上报编码器帧解析。
- `Drivers/Motor/drive_board.c:264-409`：初始化、公共 API 和 UART2 中断。
- `Drivers/MSPM0/clock.c:23-27`：1 ms SysTick 配置。

## UART3 短帧与错位恢复方案（建议代码，未应用）

本节紧接前文，针对 PA25/PA26 的上位机 UART（`UART_0`，实际外设为 UART3）补充一个可实施的恢复方案。

### 先说明限制

现有 UART3 只启用了 `DMA_DONE_RX`。DMA 的配置长度为 12，因此收到 9 个字节后，DMA 还差 3 个字节，硬件不会产生 `DMA_DONE_RX`，软件完全无法得知发送端已经停下。

所以，仅修改 `Host_Receive_Process()` 不足以识别短帧。必须在 SysConfig 中再开启 UART 的 RX timeout 中断：最后一个字节后长期没有新字节到达时，UART 进入 ISR；ISR 用 DMA 的剩余传输量判断本次已收到多少字节。

需要区分两种情况：

| 情况 | 当前行为 | 建议行为 |
| --- | --- | --- |
| 收到 `1..11` 字节后线路空闲 | 不进 ISR，等待后续任意字节补满 12 | RX timeout 中止 DMA、清空 FIFO、重新装载 DMA |
| 收满 12 字节但不是 `$...#` | 当前整块丢弃并重新装载 DMA，但不显式清 FIFO | 停止 DMA、清空 FIFO、重新装载 DMA，并记录非法帧 |
| 收到合法 12 字节帧 | 解析并重装 DMA | 保持现有行为 |

注意：在“非法帧就清 FIFO”的策略下，若发送端紧接着发送下一帧，FIFO 中已到达的下一帧开头也可能被丢弃。这是按本节需求主动丢弃整段错位数据的代价。若协议将来要求不丢连续数据，应改用环形缓冲区并扫描 `$` / `#` 重同步，而不是固定 12 字节 DMA 分块。

### 1. 修改 SysConfig，而不是 `Debug/ti_msp_dl_config.*`

`Debug/ti_msp_dl_config.c/.h` 是自动生成文件，重新生成后会被覆盖。应在 `mspm0-modules.syscfg` 的 `UART3`（名称 `UART_0`）配置中：

1. 把 RX FIFO 阈值改为 `DL_UART_RX_FIFO_LEVEL_1_2_FULL`。这样末尾少量字节会保留在 FIFO 内，RX timeout 才能可靠识别“数据流停住”。项目里的 WIT UART 已采用相同策略。
2. 将 `RX Timeout Interrupt Counts` 设为 `1` 作为初始联调值。
3. 在 UART Interrupts 中额外勾选 `RX_TIMEOUT_ERROR`，同时保留现有的 `DMA_DONE_RX`、`DMA_DONE_TX` 和 `EOT_DONE`。

对应的 `.syscfg` 代码形态如下，仅供定位配置含义：

```js
UART3.rxFifoThreshold = "DL_UART_RX_FIFO_LEVEL_1_2_FULL";
UART3.rxTimeoutValue = 1;
UART3.enabledInterrupts = [
    "DMA_DONE_RX",
    "DMA_DONE_TX",
    "EOT_DONE",
    "RX_TIMEOUT_ERROR"
];
```

重新生成后，`SYSCFG_DL_UART_0_init()` 应包含下面两项：

```c
DL_UART_Main_enableInterrupt(UART_0_INST,
    DL_UART_MAIN_INTERRUPT_DMA_DONE_RX |
    DL_UART_MAIN_INTERRUPT_DMA_DONE_TX |
    DL_UART_MAIN_INTERRUPT_EOT_DONE |
    DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);

DL_UART_Main_setRXInterruptTimeout(UART_0_INST, 1U);
```

SDK 对 RX timeout 的定义是：收到一个字节后，在设定 timeout 内没有收到额外字节时，即使 RX FIFO 未达到阈值也会触发 RX 中断；`0` 会关闭此能力。timeout 设置过小会把发送端的长字节间隔误判为短帧，所以联调时应确认发送端以连续的 115200、8N1 数据流发送完整帧。

### 2. `hostcom.c` 的建议改动

以下为建议内容，未写入源文件。它保留现有帧格式和 API，只补充诊断计数、DMA/FIFO 清理函数、非法帧处理和 RX timeout 分支。

先在 `gDMADone_RX` 等全局标志之后增加诊断计数：

```c
volatile uint32_t gHostRxShortFrameCount = 0U;
volatile uint32_t gHostRxInvalidFrameCount = 0U;
```

然后在 `Start_UART_DMA_RX()` 后增加下面两个私有函数：

```c
static uint16_t Host_GetReceivedByteCount(void)
{
    return (uint16_t)(HOST_PACKET_LEN -
        DL_DMA_getTransferSize(DMA, DMA_CH0_CHAN_ID));
}

/* 放弃当前半帧或错位帧，并把接收端恢复到“等待新帧第 0 字节”的状态。 */
static void Host_AbortAndRearmRxDma(void)
{
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);

    /* 丢掉 DMA 未搬走的残余字节，避免它们污染下一帧。 */
    while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
        (void)DL_UART_Main_receiveData(UART_0_INST);
    }

    Start_UART_DMA_RX(gRxPacket, HOST_PACKET_LEN);
}
```

将现有 `Host_Receive_Process()` 的“重装 DMA”逻辑改为区分有效帧和非法满帧。主体解析字段的代码可以保持不变：

```c
bool Host_Receive_Process(void)
{
    bool valid = false;

    if (!gDMADone_RX) {
        return false;
    }

    if (gRxPacket[0] == '$' && gRxPacket[11] == '#') {
        g_Host_Var1 = gRxPacket[1];
        g_Host_Var3 = gRxPacket[10];
        gRxPacket[9] = '\0';
        g_Host_Var2 = (int16_t)atoi((char *)&gRxPacket[3]);
        gRxPacket[9] = ',';
        valid = true;
    }

    if (valid) {
        /* 合法帧：尽快重装 DMA，降低连续帧间的接收空窗。 */
        Start_UART_DMA_RX(gRxPacket, HOST_PACKET_LEN);
    } else {
        /* 收满 12 字节却不是完整帧：放弃残余 FIFO 数据并重新同步。 */
        gHostRxInvalidFrameCount++;
        Host_AbortAndRearmRxDma();
    }

    return valid;
}
```

最后，将 `UART_0_INST_IRQHandler()` 改成包含 RX timeout 分支。现有的 TX、DMA 完成分支保持不变：

```c
void UART_0_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST)) {
        case DL_UART_MAIN_IIDX_EOT_DONE:
            gCheckUART = true;
            break;

        case DL_UART_MAIN_IIDX_DMA_DONE_TX:
            gDMADone_TX = true;
            break;

        case DL_UART_MAIN_IIDX_DMA_DONE_RX:
            gDMADone_RX = true;
            if (Host_Receive_Process()) {
                gHostFrameReady = true;
            }
            break;

        case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR:
        {
            const uint16_t received = Host_GetReceivedByteCount();

            /* received == 0：没有半帧，不清理。
             * received == 12：DMA 完成事件已经挂起，交给 DMA_DONE_RX 分支处理。
             * 1..11：确认是短帧/错位残留，放弃并从下一字节重新开始。
             */
            if ((received > 0U) && (received < HOST_PACKET_LEN)) {
                gHostRxShortFrameCount++;
                Host_AbortAndRearmRxDma();
            }
            break;
        }

        default:
            break;
    }
}
```

`DL_UART_Main_getPendingInterrupt()` 会读取并确认当前 UART 中断索引；项目中的 WIT UART RX timeout ISR 也是按“进入中断后重装 DMA”的模式工作。

### 3. 预期结果

| 输入场景 | 预期结果 |
| --- | --- |
| `$2,+12000,6#`，连续发送 | DMA 收满 12，解析成功，`gHostFrameReady = true` |
| 仅发送 9 字节后停止 | RX timeout，`gHostRxShortFrameCount++`，DMA/FIFO 清空，下一字节成为新帧起点 |
| 发送 12 字节垃圾数据 | DMA 完成，`gHostRxInvalidFrameCount++`，DMA/FIFO 清空，下一字节成为新帧起点 |
| 9 字节残帧后再发送合法帧 | timeout 已先清掉残帧，后续合法 12 字节帧可正常对齐 |

### 4. 仍应保留的约束

- 一帧仍必须严格为 12 字节，格式为 `$x,yyyyyy,z#`，其中中间数值字段恰好 6 个字符。
- 不要在有效帧后额外发送 CR/LF；CR/LF 会被视作下一帧的开头，并在超时后作为短帧丢弃。若上位机必须以换行分隔，协议应相应改为可变长度帧解析。
- `gHostFrameReady` 应作为业务层的“有新帧”事件使用；`g_Host_Var1/2/3` 由 ISR 写入，未来整理时应声明为 `volatile` 或通过临界区复制，避免主循环读取与 ISR 写入竞争。
- 该方案是“尽快丢弃并恢复同步”，不提供 CRC、序号或应答。对于噪声环境或连续高频帧，推荐最终升级为 DMA 环形缓冲区加 `$` / `#` 状态机解析。

## 给嵌入式初学者的逐步说明（建议代码解释，未应用）

这一节不假设你已经了解 UART、DMA 或中断。目标是让你能看懂上一节“为什么这样改”、每一段代码到底做了什么，以及调试时应该观察什么。

### 1. 先记住这五个角色

把串口接收想成“快递到仓库”的过程：

| 名称 | 在本项目中的东西 | 做什么 |
| --- | --- | --- |
| UART | UART3，PA25 为 RX | 把 PA25 上的串行电平还原成一个个字节 |
| RX FIFO | UART 内部的小硬件缓冲区 | 临时放刚收到、CPU 尚未取走的字节 |
| DMA | DMA 通道 0 | 自动把 FIFO 字节搬到 `gRxPacket[]`，CPU 不必逐字节搬运 |
| 中断 IRQ | `UART3_IRQHandler()` | 某个事件发生后，CPU 暂停主循环，立即执行的函数 |
| `gRxPacket` | 12 字节数组 | 存放一整帧上位机数据的内存 |

当前的接收路径是：

```text
上位机 TX
  -> PA25
  -> UART3
  -> RX FIFO
  -> DMA_CH0 自动搬运
  -> gRxPacket[0..11]
  -> UART3_IRQHandler()
  -> Host_Receive_Process()
```

PA26 是 MCU 的 TX，只用于 MCU 向上位机发送数据；外部设备的 TX 必须接 PA25，且双方必须共地。

### 2. 现在为什么会卡在 9 个字节

工程把一帧定义为固定 12 字节：

```text
索引:  0 1 2 3 4 5 6 7 8 9 10 11
内容:  $ x , y y y y y y , z  #
示例:  $ 2 , + 1 2 0 0 0 , 6  #
```

`HOST_PACKET_LEN` 的值是 12。DMA 启动后有一个“还需要搬多少字节”的内部计数器，开始时为 12：

```text
收到 0 字节：DMA 剩余 12，未完成
收到 9 字节：DMA 剩余  3，未完成
收到 12字节：DMA 剩余  0，完成，产生 DMA_DONE_RX 中断
```

原代码只关心最后一种“DMA 已完成”。因此只发 9 字节然后停止时，程序不知道发送端已经停止，只会一直等未来任意 3 个字节补齐。这正是错位的根源。

### 3. RX timeout 是什么，为什么需要它

RX timeout 的意思不是“整个程序等了多久”，而是 UART 观察到：已经收过字节，但在一段硬件设置的时间内没有再收到下一个字节。

它适合回答这个问题：

> 当前 DMA 还没收满 12 字节，但发送端是不是已经把这次消息发完了？

答案是“超时就视为本次消息结束”。如果 DMA 还差字节，说明它是一帧短数据、半帧数据，或通信中断后的残留；这时主动丢弃，下一次从干净边界重新接收。

`rxTimeoutValue = 1` 是一个初始联调值，不应理解成固定的 1 ms。它是 UART 外设的超时档位，实际适合的数值取决于波特率和发送端每两个字节之间的最大间隔。若上位机逐字节、间隔很大地发送一帧，值太小会把正常 12 字节帧误判为短帧；正常串口工具连续发送整行数据时通常不会遇到这个问题。

### 4. 为什么要把 FIFO 阈值改为半满

FIFO 阈值决定“FIFO 累积到多少字节时，UART 产生 RX 事件给 DMA”。

当前 UART3 设置为 `ONE_ENTRY`，FIFO 只要有 1 个字节就让 DMA 去搬。这样虽然响应快，但短帧的最后几个字节可能已经被 DMA 全部搬走，FIFO 空了，RX timeout 不一定能提供足够稳定的“本帧结束”信号。

设置成 `DL_UART_RX_FIFO_LEVEL_1_2_FULL` 后，DMA 会批量处理较多字节，而一帧结尾不足半个 FIFO 的残留字节会留在 FIFO。发送端停止后，RX timeout 可以可靠地出现；中断代码再统一丢弃“DMA 已搬运部分”和“FIFO 残留部分”。项目的 WIT 姿态 UART 已经使用这种配置，可作为本项目的现有参考。

这不是唯一实现方式，但它与当前“DMA 接收 + RX timeout”架构匹配，改动最小。

### 5. SysConfig 配置应如何理解

你通常应在 TI 的 SysConfig 图形界面操作，而不是手动输入 JavaScript。上一节的代码只是 SysConfig 自动保存后的文本形式。

进入名为 `UART_0` 的实例后，确认以下项目：

| 图形界面项目 | 目的 | 目标值 |
| --- | --- | --- |
| Peripheral | 选择实际 UART 外设 | UART3 |
| RX Pin | PA25 接收引脚 | PA25 |
| TX Pin | PA26 发送引脚 | PA26 |
| Target Baud Rate | 与上位机一致 | 115200 |
| Enable FIFO | 使用硬件接收缓冲 | 已勾选 |
| RX FIFO Threshold | 让 timeout 接管短帧收尾 | 1/2 full |
| RX Timeout Interrupt Counts | 启用接收空闲检测 | 先试 1 |
| Enabled Interrupts | 告诉 CPU 哪些事件要进入 ISR | DMA_DONE_RX、DMA_DONE_TX、EOT_DONE、RX_TIMEOUT_ERROR |
| DMA RX transfer size | 一帧固定长度 | 12 |

保存 SysConfig 后，工具会重建 `Debug/ti_msp_dl_config.c/.h`。这些 `Debug` 文件是输出物，手动改完下次生成就会消失，所以永远把 `mspm0-modules.syscfg` 当作配置源文件。

### 6. `Host_GetReceivedByteCount()` 逐行解释

建议函数：

```c
static uint16_t Host_GetReceivedByteCount(void)
{
    return (uint16_t)(HOST_PACKET_LEN -
        DL_DMA_getTransferSize(DMA, DMA_CH0_CHAN_ID));
}
```

含义不是“读取一个新字节”，而是读取 DMA 当前还剩多少任务没有完成。

- `HOST_PACKET_LEN` 是总目标数，也就是 12。
- `DL_DMA_getTransferSize(...)` 返回 DMA 剩余要搬的字节数。
- `12 - 剩余数` 就是已经成功搬到 `gRxPacket` 的字节数。

举例：若 DMA 还剩 3，函数返回 `12 - 3 = 9`。RX timeout 分支看到 `9`，就知道出现了 9 字节短帧。

### 7. `Host_AbortAndRearmRxDma()` 逐行解释

建议函数：

```c
static void Host_AbortAndRearmRxDma(void)
{
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);

    while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
        (void)DL_UART_Main_receiveData(UART_0_INST);
    }

    Start_UART_DMA_RX(gRxPacket, HOST_PACKET_LEN);
}
```

第一行 `DL_DMA_disableChannel(...)`：停止当前未完成的搬运任务。若已经收了 9 个字节，就放弃“还要收 3 个字节”的旧任务。

`while` 循环：DMA 停下后，UART FIFO 中可能还有来不及搬运的残留字节。`isRXFIFOEmpty()` 判断 FIFO 是否为空；只要不为空，就调用 `receiveData()` 读走一个字节并故意忽略它。这里的 `(void)` 表示“函数确实返回了数据，但我就是要丢弃它”。

最后的 `Start_UART_DMA_RX(...)`：重新设置 DMA 源地址、目标地址和传输长度，使其再次从 `gRxPacket[0]` 开始等待完整的 12 字节新帧。也就是说，下一次收到的字节成为新一帧的第 0 个字节。

### 8. 满 12 字节后如何判断是否有效

建议逻辑中的核心判断是：

```c
if (gRxPacket[0] == '$' && gRxPacket[11] == '#') {
    /* 认为帧有效，解析数据 */
} else {
    /* 认为帧错位或损坏，丢弃并重新同步 */
}
```

这只是最基础的边界检查：第一个字符必须是 `$`，最后一个字符必须是 `#`。当前协议没有校验和，也没有检查第 2、3、10 个字段是不是逗号或数值，因此它不能发现所有错误，但能发现最常见的“9 字节残帧把后续帧开头吃掉”问题。

有效帧仍调用 `Start_UART_DMA_RX()`，而不是“清 FIFO 后重装”。原因是有效帧后面可能紧跟下一帧；尽快重装 DMA 能减少下一帧开始部分被丢弃的风险。

非法满帧才调用 `Host_AbortAndRearmRxDma()`，因为这时已经认定当前字节边界不可信，宁可丢掉残留，也要重新建立边界。

### 9. 中断处理函数如何工作

可以把 ISR 看成一个 `switch`，UART 告诉 CPU“这次是哪个原因叫你来”：

```c
case DL_UART_MAIN_IIDX_DMA_DONE_RX:
    /* 已完整收满 12 字节。 */
    break;

case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR:
    /* 收到一些字节，但发送端长时间没有再发。 */
    break;
```

两种事件的职责不同：

| 事件 | 意味着什么 | 应做什么 |
| --- | --- | --- |
| `DMA_DONE_RX` | DMA 已经搬完 12 字节 | 校验 `$` 和 `#`，有效则解析，无效则清理 |
| `RX_TIMEOUT_ERROR` 且收到 `1..11` 字节 | 半帧已经结束 | 不解析，清 DMA/FIFO，重新等待 |
| `RX_TIMEOUT_ERROR` 且收到 `0` 字节 | 没有半帧 | 什么都不做 |
| `RX_TIMEOUT_ERROR` 且已收 12 字节 | DMA 完成事件也可能正等待处理 | 不在 timeout 分支抢先覆盖缓冲，交给 `DMA_DONE_RX` |

`gHostRxShortFrameCount` 与 `gHostRxInvalidFrameCount` 是调试计数器：

- 前者增加，说明常出现发到一半就停的帧，或 timeout 设置太小。
- 后者增加，说明收满了 12 字节，但帧开头/结尾不符合协议，通常是错位、额外 CR/LF、波特率不对或线路噪声。

使用 CCS 单步调试时，推荐观察这两个变量和 `gHostFrameReady`。不要在 CPU 暂停状态下发送串口数据并期待 ISR 立即执行；CPU 被断点暂停时，ISR 也不会运行。

### 10. 建议的验证顺序

按下面顺序测试，最容易定位问题：

1. 完成开机任务选择，确保已经执行 `Host_Receive_Start()`。
2. 设置上位机为 `115200, 8N1, 无流控`，外部 TX 接 PA25，共地。
3. 首先发送且只发送 `$2,+12000,6#`，不要附带换行。
4. 观察 `gHostFrameReady` 是否变为 `true`，并检查 `g_Host_Var1 == '2'`、`g_Host_Var2 == 12000`、`g_Host_Var3 == '6'`。
5. 复位或确认接收已处于干净边界后，只发送 9 字节，例如 `$2,+1200`。等待超过 timeout，确认 `gHostRxShortFrameCount` 加 1。
6. 紧接着发送完整 12 字节有效帧，确认它能被正常解析。若成功，说明短帧恢复有效。
7. 发送 12 字节、但不以 `$` 开头或不以 `#` 结尾的数据，确认 `gHostRxInvalidFrameCount` 加 1；再发送有效帧，确认恢复。

### 11. 为什么建议以后还要用 `volatile`

主循环和中断像两个人同时看同一张纸：中断会改 `g_Host_Var1/2/3`，主循环会读它们。编译器在优化时可能认为“主循环没有主动写这些变量，所以值不会突然变化”，从而把旧值缓存起来。

`volatile` 的意思是告诉编译器：这个变量可能在当前代码看不见的地方被硬件或 ISR 修改，每次使用都必须重新从内存读取。它不能解决所有多线程问题，但对于 ISR 写、主循环读的状态标志是必要的第一步。

建议以后将 `gHostFrameReady` 作为唯一的新帧标志：ISR 成功解析后置 `true`，主循环读取数据后清 `false`。这样业务层不必轮询 DMA 完成标志，也不会混淆“收到一帧”和“已经处理一帧”。

### 12. 这套方案适合什么、不适合什么

它适合：帧长度固定为 12、发送频率不高、发生错误时允许丢弃当前帧，并希望用较少改动快速恢复通信的项目。

它不适合：上位机连续高速发送、每一帧都不能丢、帧长会变化、或线路噪声严重的项目。那些场景应使用 DMA 环形缓冲区，逐字节寻找 `$`，收到 `#` 后再验证长度和字段。那是一种更通用的接收器，但需要重写接收架构，不应在尚未理解当前固定帧方案时直接跳过去。
