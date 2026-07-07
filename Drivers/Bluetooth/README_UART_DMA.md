# UART DMA发送驱动使用说明

## 概述

本驱动基于TI官方例程`uart_tx_console_multibyte_repeated_fifo_dma`实现，为BalanceCar项目提供了带DMA和FIFO的UART发送功能，用于实时传输小车角度值和PID参数，同时最小化CPU占用，避免干扰高频控制逻辑。

## 文件结构

```
BalanceCar/Drivers/Bluetooth/
├── uart_dma_tx.h          # DMA发送驱动头文件
├── uart_dma_tx.c          # DMA发送驱动实现
├── bluetooth.h            # 蓝牙模块接口（已更新）
├── bluetooth.c            # 蓝牙模块实现（已更新）
└── uart_dma_example.c     # 使用示例代码
```

## 核心特性

1. **DMA传输** - 数据传输由DMA完成，CPU只需启动传输即可
2. **FIFO缓冲** - 使用UART硬件FIFO，进一步减少中断次数
3. **非阻塞发送** - 发送函数立即返回，不会阻塞控制循环
4. **状态检查** - 可以查询发送是否完成，避免数据覆盖

## 工作原理
****
```
应用层调用发送函数
    ↓
数据复制到静态缓冲区
    ↓
配置DMA传输参数
    ↓
启动DMA传输（函数立即返回）
    ↓
DMA自动将数据搬运到UART FIFO
    ↓****
UART硬件自动发送FIFO中的数据
    ↓
发送完成后触发中断，更新状态标志
```

## 配置说明

### 1. SysConfig配置（已完成）

在`mspm0-modules.syscfg`中为UART_BT添加了以下配置：

```javascript
UART2.enableFIFO = true;                              // 启用FIFO
UART2.txFifoThreshold = "DL_UART_TX_FIFO_LEVEL_ONE_ENTRY";
UART2.enabledInterrupts = ["DMA_DONE_TX","EOT_DONE"]; // 启用DMA和EOT中断
UART2.enabledDMATXTriggers = "DL_UART_DMA_INTERRUPT_TX";
UART2.DMA_CHANNEL_TX.$name = "DMA_CH2";               // 使用DMA通道2
UART2.DMA_CHANNEL_TX.addressMode = "b2f";             // byte-to-FIFO模式
```

### 2. DMA通道分配

- **DMA_CH0**: UART_AI接收（已有）
- **DMA_CH1**: UART_WIT接收（已有）
- **DMA_CH2**: UART_BT发送（新增）

### 3. 中断处理

在`INTERRUPT/interrupt.c`中添加了UART_BT中断处理函数：

```c
void UART_BT_INST_IRQHandler(void)
{
    UART_DMA_TX_IRQHandler();
}
```

## API使用说明

### 初始化

```c
#include "bluetooth.h"

// 在main函数中，SYSCFG_DL_init()之后调用
Bluetooth_Init();
```

### 发送角度和PID数据（推荐）

```c
float angle = 45.5f;
float kp = 1.5f, ki = 0.1f, kd = 0.5f;

// 非阻塞发送
if (Bluetooth_SendAnglePID_DMA(angle, kp, ki, kd)) {
    // 发送成功启动
} else {
    // 上一次发送未完成，本次跳过
}
```

**输出格式**: `Angle=45.50,Kp=1.500,Ki=0.100,Kd=0.500\r\n`

### 发送自定义字符串

```c
const char *msg = "System Ready\r\n";

if (Bluetooth_SendString_DMA(msg)) {
    // 发送成功启动
} else {
    // 发送失败
}
```

### 检查发送状态

```c
if (Bluetooth_IsTxComplete()) {
    // 发送已完成，可以启动下一次发送
}
```

## 使用建议

### ✅ 推荐做法

1. **在主循环中定期发送**（降低发送频率）

```c
uint32_t last_send_time = 0;
const uint32_t SEND_INTERVAL_MS = 100;  // 每100ms发送一次

while (1) {
    // 高频控制逻辑（1kHz）
    ControlLoop();
    
    // 低频数据发送（10Hz）
    if ((GetSystemTick() - last_send_time) >= SEND_INTERVAL_MS) {
        if (Bluetooth_SendAnglePID_DMA(angle, kp, ki, kd)) {
            last_send_time = GetSystemTick();
        }
    }
}
```

2. **在定时器中断中发送**（降低发送频率）

```c
void TIMER_IRQHandler(void) {
    static uint32_t counter = 0;
    
    // 每100次控制循环发送一次
    if (++counter >= 100) {
        counter = 0;
        Bluetooth_SendAnglePID_DMA(angle, kp, ki, kd);
    }
}
```

3. **检查返回值，避免数据丢失**

```c
if (!Bluetooth_SendAnglePID_DMA(angle, kp, ki, kd)) {
    // 上一次发送未完成，可以记录丢失次数
    lost_count++;
}
```

### ❌ 不推荐做法

1. **在高频循环中连续发送**

```c
// ❌ 错误：会导致大量发送失败
while (1) {
    Bluetooth_SendAnglePID_DMA(angle, kp, ki, kd);  // 每次循环都发送
    delay_us(100);  // 10kHz发送频率，远超串口带宽
}
```

2. **阻塞等待发送完成**（在高频控制循环中）

```c
// ❌ 错误：会阻塞控制循环
Bluetooth_SendAnglePID_DMA(angle, kp, ki, kd);
while (!Bluetooth_IsTxComplete()) {
    // 等待发送完成，阻塞了控制逻辑
}
```

## 性能分析

### CPU占用对比

| 发送方式 | CPU占用 | 说明 |
|---------|---------|------|
| 阻塞发送 | ~100% | CPU忙等待每个字节发送完成 |
| 中断发送 | ~30% | 每个字节触发一次中断 |
| **DMA+FIFO** | **<5%** | 只在传输开始和结束时触发中断 |

### 发送时间估算

以9600波特率为例，发送一条数据：
- 数据长度：约40字节
- 发送时间：40 × 10 / 9600 ≈ 42ms
- 推荐发送间隔：≥50ms（20Hz）

## 故障排查

### 问题1：发送总是返回false

**原因**：上一次发送未完成

**解决**：
- 增加发送间隔
- 检查波特率配置是否正确
- 确认UART和DMA初始化正确

### 问题2：数据发送不完整

**原因**：数据缓冲区被覆盖

**解决**：
- 使用`Bluetooth_IsTxComplete()`检查发送状态
- 不要在发送完成前修改发送缓冲区

### 问题3：编译错误 - DMA_CH2未定义

**原因**：SysConfig未重新生成配置文件

**解决**：
1. 打开`mspm0-modules.syscfg`
2. 保存文件（触发重新生成）
3. 重新编译项目

## 兼容性说明

### 保留的旧接口

为了兼容现有代码，保留了阻塞发送接口：

```c
// 阻塞发送（不推荐在高频循环中使用）
Bluetooth_SendString("Hello\r\n", 7);
Bluetooth_SendData(1.23f, 4.56f);
```

### 迁移建议

将现有的阻塞发送代码迁移到DMA发送：

```c
// 旧代码
Bluetooth_SendString("Data\r\n", 6);

// 新代码
Bluetooth_SendString_DMA("Data\r\n");
```

## 注意事项

1. **缓冲区大小限制**：单次发送最大64字节
2. **发送频率限制**：建议≤20Hz（取决于波特率和数据长度）
3. **中断优先级**：确保UART中断优先级低于关键控制中断
4. **数据有效性**：发送完成前不要修改数据缓冲区

## 参考资料

- TI官方例程：`uart_tx_console_multibyte_repeated_fifo_dma`
- MSPM0 SDK文档：DMA章节
- MSPM0 SDK文档：UART章节
