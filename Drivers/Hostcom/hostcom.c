#include "hostcom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 定义在 .h 中声明的全局变量 */
volatile char g_Host_Var1 = '0';
volatile int16_t g_Host_Var2 = 0;
volatile char g_Host_Var3 = '0';

uint8_t gTxPacket[128];
uint8_t gRxPacket[HOST_PACKET_LEN];

volatile bool gCheckUART = false;
volatile bool gDMADone_TX = false;
volatile bool gDMADone_RX = false;

/* 可在调试器中观察这两个计数器，判断是否发生过短帧或完整错帧。 */
volatile uint32_t gHostRxShortFrameCount = 0U;
volatile uint32_t gHostRxInvalidFrameCount = 0U;

/* 中断解析成功后置位；主循环调用 Host_Receive_Process() 后清零。 */
volatile bool gHostFrameReady = false;

/* ====================================================================
 * 内部 DMA 控制函数 (供当前文件调用)
 * ==================================================================== */

static void Start_UART_DMA_TX(const uint8_t *data, uint16_t length)
{
    DL_DMA_setSrcAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)data);
    DL_DMA_setDestAddr(DMA, DMA_CH1_CHAN_ID, (uint32_t)(&UART_0_INST->TXDATA));
    DL_DMA_setTransferSize(DMA, DMA_CH1_CHAN_ID, length);

    gCheckUART = false;
    gDMADone_TX = false;

    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
    DL_DMA_enableChannel(DMA, DMA_CH1_CHAN_ID);
}

static void Start_UART_DMA_RX(uint8_t *buffer, uint16_t length)
{
    DL_DMA_setSrcAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)(&UART_0_INST->RXDATA));
    DL_DMA_setDestAddr(DMA, DMA_CH0_CHAN_ID, (uint32_t)buffer);
    DL_DMA_setTransferSize(DMA, DMA_CH0_CHAN_ID, length);

    gDMADone_RX = false;

    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
    DL_DMA_enableChannel(DMA, DMA_CH0_CHAN_ID);
}

/*
 * 停止 DMA 后统计已经搬入内存的字节，再把 FIFO 中保留的尾字节补到数组。
 * 半满阈值下，连续接收 N 个字节时通常是 DMA 搬 N-1 个、FIFO 留 1 个。
 */
static uint16_t Host_StopRxDmaAndCollectFifo(void)
{
    uint16_t received;

    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);

    uint32_t remaining = DL_DMA_getTransferSize(DMA, DMA_CH0_CHAN_ID);

    if (remaining > HOST_RX_DMA_LEN) {
        return 0U;
    }

    received = (uint16_t)(HOST_RX_DMA_LEN - remaining);

    while ((received < HOST_PACKET_LEN) &&
           !DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
        gRxPacket[received++] =
            (uint8_t)DL_UART_Main_receiveData(UART_0_INST);
    }

    return received;
}

/* 清除旧事件并重新启动 11 字节 DMA；调用前当前 DMA 必须已经停止。 */
static void Host_RearmRxDma(void)
{
    while (!DL_UART_Main_isRXFIFOEmpty(UART_0_INST)) {
        (void)DL_UART_Main_receiveData(UART_0_INST);
    }

    DL_UART_Main_clearInterruptStatus(UART_0_INST,
        DL_UART_MAIN_INTERRUPT_DMA_DONE_RX |
        DL_UART_MAIN_INTERRUPT_RX_TIMEOUT_ERROR);

    Start_UART_DMA_RX(gRxPacket, HOST_RX_DMA_LEN);
}

/*
 * 放弃当前这次接收，并重新准备接收一组完整的 12 字节数据。
 * 必须先停 DMA，再清 UART FIFO，避免旧字节进入下一帧造成持续错位。
 */
static void Host_AbortAndRearmRxDma(void)
{
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
    memset(gRxPacket, 0, sizeof(gRxPacket));
    Host_RearmRxDma();
}

/* ====================================================================
 * 对外提供的用户函数
 * ==================================================================== */

void Host_Send(char var1, const char* var2, char var3)
{
    /* 
     * 1. 使用 sprintf 将传入的变量格式化并拼接到 gTxPacket 数组中
     * 例如传入: '2', "+12000", '6'
     * 拼接结果: "$2,+12000,6#"
     */
    sprintf((char*)gTxPacket, "$%c,%s,%c#", var1, var2, var3);
    
    /* 2. 启动 DMA 发送这 12 个字节 */
    Start_UART_DMA_TX(gTxPacket, HOST_PACKET_LEN);
    

}

void Host_Receive_Start(void)
{
    /* 开启后台接收，目标收满 12 个字节 ($2,+12000,6#)。
     * 只需在初始化时调用一次；之后每收齐一帧，中断会自动重新武装 DMA。 */
    gHostFrameReady = false;
    memset(gRxPacket, 0, sizeof(gRxPacket));
    DL_DMA_disableChannel(DMA, DMA_CH0_CHAN_ID);
    Host_RearmRxDma();
}

/*
 * 该函数只在 UART 中断内部调用：校验、解析，并尽快重新启动 DMA。
 * 与业务层的 Host_Receive_Process() 分开，避免主循环较慢时漏掉后续字节。
 */
static bool Host_ParseCompletedFrame(void)
{
    /* DMA 完成标志尚未置位时，不读取接收缓冲区。 */
    if (!gDMADone_RX) {
        return false;
    }

    /* 2. 数据已收齐，开始极简解算
     * gRxPacket 的结构应该是：
     * [0]=$ [1]=var1 [2]=, [3~8]=var2 [9]=, [10]=var3 [11]=#
     */
    bool valid = false;

    /* 只有以 '$' 开头、以 '#' 结尾的 12 字节数据才是对齐的完整帧。 */
    if (gRxPacket[0] == '$' && gRxPacket[11] == '#') {

        /* 提取第一个和第三个字符变量 (直接保留 ASCII 码) */
        g_Host_Var1 = gRxPacket[1];   // 例如 '2'
        g_Host_Var3 = gRxPacket[10];  // 例如 '6'

        /*
         * 提取中间的字符串并转换为 int16_t 数字。
         * C语言的 atoi() 函数遇到 '\0' 会停止转换。
         * 所以我们把原本的逗号 gRxPacket[9] 临时改成 '\0'。
         */
        gRxPacket[9] = '\0';
        g_Host_Var2 = (int16_t)atoi((char*)&gRxPacket[3]);

        /* 解算完了，把逗号恢复，保持数组原样(可选) */
        gRxPacket[9] = ',';

        valid = true;
    } else {
        /* 收满了 12 字节，但帧头/帧尾不正确：丢弃并彻底重新对齐。 */
        gHostRxInvalidFrameCount++;
        Host_AbortAndRearmRxDma();
        return false;
    }

    /* 3. 重新武装 11 字节 DMA，准备接收下一帧。 */
    Host_RearmRxDma();

    return valid;
}

bool Host_Receive_Process(void)
{
    bool ready;
    uint32_t interruptState = __get_PRIMASK();

    /*
     * 这里只短暂关闭中断，用来安全地“读取并清除”新帧标志。
     * 没有新帧时立即返回，不等待 DMA，也不重复解析数据。
     */
    __disable_irq();
    ready = gHostFrameReady;
    gHostFrameReady = false;
    if (interruptState == 0U) {
        __enable_irq();
    }

    return ready;
}

/*
 * DMA 完成和 RX timeout 共用同一套收尾逻辑：
 * 12 字节才校验解析；1~11 字节一律作为短帧丢弃并重新对齐。
 */
static void Host_HandleRxEvent(void)
{
    uint16_t received = Host_StopRxDmaAndCollectFifo();

    if (received == HOST_PACKET_LEN) {
        gDMADone_RX = true;
        if (Host_ParseCompletedFrame()) {
            gHostFrameReady = true;
        }
    } else {
        if (received > 0U) {
            gHostRxShortFrameCount++;
        }
        Host_AbortAndRearmRxDma();
    }
}

/* ====================================================================
 * UART 中断服务函数
 * ==================================================================== */
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
            Host_HandleRxEvent();
            break;

        case DL_UART_MAIN_IIDX_RX_TIMEOUT_ERROR:
            Host_HandleRxEvent();
            break;

        default:
            break;
    }
}
