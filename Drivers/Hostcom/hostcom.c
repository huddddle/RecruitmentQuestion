#include "hostcom.h"
#include <stdio.h>
#include <stdlib.h>

/* 定义在 .h 中声明的全局变量 */
char g_Host_Var1 = '0';
int16_t g_Host_Var2 = 0;
char g_Host_Var3 = '0';

uint8_t gTxPacket[128];
uint8_t gRxPacket[HOST_PACKET_LEN];

volatile bool gCheckUART = false;
volatile bool gDMADone_TX = false;
volatile bool gDMADone_RX = false;

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
     * 只需在初始化时调用一次；之后每收齐一帧，Host_Receive_Process 会自动重新武装。 */
    Start_UART_DMA_RX(gRxPacket, HOST_PACKET_LEN);
}

bool Host_Receive_Process(void)
{
    /* 1. 非阻塞：数据还没收齐就立刻返回，绝不死等 */
    if (!gDMADone_RX) {
        return false;
    }

    /* 2. 数据已收齐，开始极简解算
     * gRxPacket 的结构应该是：
     * [0]=$ [1]=var1 [2]=, [3~8]=var2 [9]=, [10]=var3 [11]=#
     */
    bool valid = false;

    /* 做一下简单的校验：如果头是 '$'，尾是 '#'，说明数据是对齐的 */
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
    }

    /* 3. 重新武装 DMA，准备接收下一帧 (会把 gDMADone_RX 清零) */
    Start_UART_DMA_RX(gRxPacket, HOST_PACKET_LEN);

    return valid;
}

/* ====================================================================
 * UART 中断服务函数
 * ==================================================================== */
void UART_0_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_0_INST)) {
        case DL_UART_MAIN_IIDX_EOT_DONE:       /* 发送彻底结束 */
            gCheckUART = true;
            break;
        case DL_UART_MAIN_IIDX_DMA_DONE_TX:    /* TX DMA 搬运完毕 */
            gDMADone_TX = true;
            break;
        case DL_UART_MAIN_IIDX_DMA_DONE_RX:    /* RX DMA 收齐了指定数量的数据 */
            gDMADone_RX = true;
            break;
        default:
            break;
    }
}