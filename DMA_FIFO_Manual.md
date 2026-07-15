# MSPM0G3507 UART DMA 与 FIFO 使用手册

本手册详细讲解如何在德州仪器 (TI) MSPM0G3507 上，结合 **UART FIFO** 与 **DMA** 实现高效、极低 CPU 占用、**全程非阻塞**的串口数据**发送与接收**。

> 本手册与本工程 `Drivers/Hostcom/hostcom.c`、`mspm0-modules.syscfg` 完全对应，所有通道号、选项、代码均可直接照抄验证。

---

## 1. 原理讲解：为什么需要 DMA + FIFO？

在传统串口模式下，CPU 需要逐个字节地把数据送入发送寄存器、或从接收寄存器读出。这要么让 CPU 阻塞“死等”，要么产生频繁的中断打断，浪费性能。为此引入 **FIFO** 和 **DMA** 两个硬件利器。

### 1.1 FIFO（硬件缓存）

FIFO (First-In-First-Out) 是挂在 UART 硬件上的“蓄水池”，MSPM0 的 UART FIFO **深度为 4**。

- **发送时**：CPU/DMA 一口气把数据灌入 TX FIFO，UART 硬件自动慢慢“抽水”发送，防止波形断档。
- **接收时**：外部来的数据先攒在 RX FIFO，达到阈值后再一次性通知 DMA 取走，减少“要水”频率。

### 1.2 DMA（硬件搬运工）

DMA (Direct Memory Access) 是独立于 CPU 的搬运工。只要设定好**起点**、**终点**、**数量**，它就能在后台把数据从内存搬到 UART（发送），或从 UART 搬到内存（接收），**全程不占用 CPU**。

### 1.3 一个必须先懂的关键点：地址是否递增

DMA 每搬一次会决定“源地址 / 目标地址要不要 +1”。这正是本工程踩过的坑：

- **发送**：源是内存数组 → **源地址必须递增**（逐字节取出）；目标是 `TXDATA` 寄存器 → **目标地址固定**。
- **接收**：源是 `RXDATA` 寄存器 → **源地址固定**；目标是内存数组 → **目标地址必须递增**。

> ⚠️ **真实故障案例**：如果发送通道的源地址被配成“不递增”（SysConfig 默认的 `f2f`），DMA 会 12 次都读数组的**第 0 个字节**。于是发 `$2,+12000,6#` 时，上位机收到的是 **12 个 `$`**（`$$$$$$$$$$$$`）——数量正好等于传输长度、内容正好是首字节。解决办法就是把发送通道 Address Mode 选成 `b2f`（见 2.4）。

---

## 2. SysConfig 配置指南（收发双向）

> 重要提示：DMA 物理通道号（CH0 / CH1 …）由你在 SysConfig 里**启用的先后顺序**自动分配，不是固定的。本工程先启用 RX 后启用 TX，因此：**RX = `DMA_CH0`，TX = `DMA_CH1`**。写代码时务必用 SysConfig 生成的宏 `DMA_CH0_CHAN_ID` / `DMA_CH1_CHAN_ID`，并对照自己 `.syscfg` 里每个通道的 `$name` 确认谁是收、谁是发。

### 2.1 基础 UART 配置

1. 左侧添加一个 **UART** 模块，Name 命名为 `UART_0`。
2. **Target Baud Rate** 设为 `115200`，并分配 TX / RX 物理引脚。

### 2.2 开启并配置 FIFO

1. 展开 **Advanced Configuration**，勾选 **Enable FIFO**。
2. **TX FIFO Threshold** 选 **`TX FIFO contains <= 3 entries`**（对应枚举 `DL_UART_TX_FIFO_LEVEL_1_4_EMPTY`；FIFO 只要空出 1 格 DMA 就补货）。
3. **RX FIFO Threshold** 选 **`RX FIFO contains >= 1 entry`**（对应枚举 `DL_UART_RX_FIFO_LEVEL_ONE_ENTRY`；收到 1 字节就触发 DMA 搬走）。

### 2.3 配置中断 (Interrupts)

展开 **Interrupts** 菜单，**仅勾选**以下三项：

- `DMA Done on Transmit`：发送 DMA 搬运完成。
- `End of Transmission`：UART 发送彻底完成（FIFO 也发空了）。
- `DMA Done on Receive`：接收 DMA 收齐指定数量。

### 2.4 配置 DMA (DMA Channel)

1. 在 UART 的 **DMA** 菜单里，勾选 **Enable DMA TX** 和 **Enable DMA RX**，下方会自动生成两个 DMA 通道。
2. 配置触发源：
   - **DMA TX Trigger** 设为 `TX interrupt`（`DL_UART_DMA_INTERRUPT_TX`）。
   - **DMA RX Trigger** 设为 `RX interrupt`（`DL_UART_DMA_INTERRUPT_RX`）。
3. 配置**发送通道 (DMA_CHANNEL_TX → 本工程即 `DMA_CH1`)**：
   - **Address Mode**：选 **`Block to FIFO (b2f)`**。
     （内存数组→固定寄存器；此模式已自动令**源递增、目标固定**，无需另设 increment。这一步选错就会出现 1.3 里的 12 个 `$`。）
   - **Source Length / Destination Length**：均选 `Byte`。
   - **Configure Transfer Size** 勾选，**Transfer Size** 填 `12`（帧长）。
4. 配置**接收通道 (DMA_CHANNEL_RX → 本工程即 `DMA_CH0`)**：
   - **Address Mode**：选 **`FIFO to Block (f2b)`**。
     （固定寄存器→内存数组；此模式已自动令**源固定、目标递增**。）
   - **Source Length / Destination Length**：均选 `Byte`。
   - **Configure Transfer Size** 勾选，**Transfer Size** 填 `12`。
   - 勾选 **Enable Interrupt**（收齐后触发 `DMA_DONE_RX`）。

> 记忆口诀：**发送 b2f（源递增），接收 f2b（目标递增）**。Address Mode 里字母的前半是“源”、后半是“目标”，`Block` 端一定是那个递增的内存数组。

配置完成后重新 **Generate**，`ti_msp_dl_config.c` 才会生成正确的 `DL_DMA_*` 初始化。

---

## 3. 发送部分：代码实战（非阻塞）

发送使用 **`DMA_CH1_CHAN_ID`**（与 SysConfig 中 TX 通道对应）。

```c
#include "ti_msp_dl_config.h"

#define HOST_PACKET_LEN (12)      // 帧长：$2,+12000,6#

uint8_t gTxPacket[128];
volatile bool gCheckUART  = false;   // EOT：UART 真正发完
volatile bool gDMADone_TX = false;   // DMA 已把数据灌进 FIFO

/* 启动 UART DMA 发送（起点=内存数组，终点=TXDATA 寄存器） */
static void Start_UART_DMA_TX(const uint8_t *data, uint16_t length)
{
    DL_DMA_setSrcAddr(DMA,  DMA_CH1_CHAN_ID, (uint32_t)data);
    DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)(&UART_0_INST->TXDATA));
    DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, length);

    gCheckUART  = false;
    gDMADone_TX = false;

    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
    DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);
}

/* 对外发送接口：把三个变量拼成 "$var1,var2,var3#" 再交给 DMA */
void Host_Send(char var1, const char* var2, char var3)
{
    sprintf((char*)gTxPacket, "$%c,%s,%c#", var1, var2, var3);
    Start_UART_DMA_TX(gTxPacket, HOST_PACKET_LEN);   // 非阻塞：发出去就返回
}
```

> 说明：`Host_Send` 不等待发完就返回。若主循环发送非常密集，需要在下一次发送前判断上一帧是否发完（用 `gDMADone_TX` / `gCheckUART`），避免覆盖仍在发送中的 `gTxPacket`。

---

## 4. 接收部分：代码实战（完全非阻塞）

接收使用 **`DMA_CH0_CHAN_ID`**。核心思想：初始化时武装一次 DMA，之后主循环只是**查一下标志位**，没收齐就立刻返回，绝不 `while` 死等。

### 4.1 接收变量与函数

```c
#include <stdlib.h>    // atoi

/* 解算结果 */
char    g_Host_Var1 = '0';
int16_t g_Host_Var2 = 0;
char    g_Host_Var3 = '0';

uint8_t gRxPacket[HOST_PACKET_LEN];
volatile bool gDMADone_RX = false;

/* 启动/重新武装 UART DMA 接收（起点=RXDATA 寄存器，终点=内存数组） */
static void Start_UART_DMA_RX(uint8_t *buffer, uint16_t length)
{
    DL_DMA_setSrcAddr(DMA,  DMA_CH0_CHAN_ID, (uint32_t)(&UART_0_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)buffer);
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, length);

    gDMADone_RX = false;

    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
}

/* 初始化时调用一次，开启后台接收 */
void Host_Receive_Start(void)
{
    Start_UART_DMA_RX(gRxPacket, HOST_PACKET_LEN);
}

/* 非阻塞检查并解算：没收齐立即返回 false，收齐则解算+重新武装并返回 true */
bool Host_Receive_Process(void)
{
    if (!gDMADone_RX) {
        return false;                 // 关键：不死等，直接走人
    }

    bool valid = false;

    /* 帧结构: [0]=$ [1]=var1 [2]=, [3~8]=var2 [9]=, [10]=var3 [11]=# */
    if (gRxPacket[0] == '$' && gRxPacket[11] == '#') {
        g_Host_Var1 = gRxPacket[1];
        g_Host_Var3 = gRxPacket[10];

        gRxPacket[9] = '\0';          // atoi 遇 '\0' 停止，临时截断
        g_Host_Var2 = (int16_t)atoi((char*)&gRxPacket[3]);
        gRxPacket[9] = ',';           // 恢复

        valid = true;
    }

    Start_UART_DMA_RX(gRxPacket, HOST_PACKET_LEN);   // 重新武装，准备下一帧
    return valid;
}
```

### 4.2 统一的中断服务函数（同时处理收发）

```c
void UART_0_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST)) {
        case DL_UART_MAIN_IIDX_EOT_DONE:     gCheckUART  = true; break; // 发送彻底结束
        case DL_UART_MAIN_IIDX_DMA_DONE_TX:  gDMADone_TX = true; break; // TX DMA 搬完
        case DL_UART_MAIN_IIDX_DMA_DONE_RX:  gDMADone_RX = true; break; // RX 收齐
        default: break;
    }
}
```

### 4.3 主循环实战：非阻塞收发 + OLED 显示

```c
int main(void)
{
    SYSCFG_DL_init();
    OLED_Init();

    OLED_ShowString(0, 0, (uint8_t *)"Rx:", 8);   // 接收显示区
    Host_Receive_Start();                          // 开启后台接收（仅一次）

    uint8_t oled_buffer[64];

    while (1) {
        /* 想发就发，非阻塞 */
        Host_Send('2', "+12000", '6');

        /* 非阻塞查询；收到有效帧才刷新 OLED */
        if (Host_Receive_Process()) {
            sprintf((char *)oled_buffer, "%c %d %c    ",
                    g_Host_Var1, g_Host_Var2, g_Host_Var3);
            OLED_ShowString(3 * 8, 0, oled_buffer, 8);
        }

        /* ... 这里可以跑循迹 / PID / 电机等其它任务，CPU 不被串口拖住 ... */
    }
}
```

---

## 5. 避坑与总结

- **通道号别写死凭感觉**：TX/RX 到底是 CH0 还是 CH1，取决于 SysConfig 启用顺序。本工程 **RX=CH0、TX=CH1**；始终用生成的 `DMA_CHx_CHAN_ID` 宏，并对照 `.syscfg` 的 `$name` 核对。
- **收到一串相同字符（如 `$$$$…`）**：99% 是 DMA 地址增量方向错了。发送必须 `b2f`（源递增），接收必须 `f2b`（目标递增）。
- **不要阻塞**：`Host_Receive_Process()` 用“查标志位、没数据就返回”的方式取代 `while(!gDMADone_RX)` 死等；收齐后**必须重新武装 DMA** 才能接收下一帧。
- **帧长要一致**：SysConfig 的 Transfer Size、`HOST_PACKET_LEN`、实际发送字节数三者必须都等于 `12`。
- **帧错位处理**：本例对收到的错帧（帧头/帧尾不符）直接丢弃并重新武装；若上位机字节数不稳定导致长期错位，可改为“逐字节查找帧头 `$`”的对齐方案。
- **发送节流（可选）**：主循环高频调用 `Host_Send` 时，用 `gDMADone_TX`/`gCheckUART` 判断上一帧是否发完，避免覆盖正在发送的 `gTxPacket`。
