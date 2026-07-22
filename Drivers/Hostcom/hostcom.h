#ifndef HOST_COM_H
#define HOST_COM_H

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdbool.h>

/* ================= 1. 解算后存放的三个全局变量 ================= */
extern volatile char g_Host_Var1;      // 第一个变量 (ASCII字符，例如 '2')
extern volatile int16_t g_Host_Var2;   // 第二个变量 (转为整数，例如 12000)
extern volatile char g_Host_Var3;      // 第三个变量 (ASCII字符，例如 '6')

/* ================= 2. 底层收发标志位与缓冲区 ================= */
#define HOST_PACKET_LEN (12)  // 数据总长度: $2,+12000,6# (12个字节)
#define HOST_RX_DMA_LEN (HOST_PACKET_LEN - 1U) // DMA 收前11字节，FIFO保留最后1字节

extern uint8_t gTxPacket[128];
extern uint8_t gRxPacket[HOST_PACKET_LEN];

extern volatile bool gCheckUART;
extern volatile bool gDMADone_TX;
extern volatile bool gDMADone_RX;

/* 调试计数：分别记录不足 12 字节的短帧、以及帧头/帧尾错误的完整帧。 */
extern volatile uint32_t gHostRxShortFrameCount;
extern volatile uint32_t gHostRxInvalidFrameCount;
extern volatile bool gHostFrameReady;

/* ================= 3. 供主程序调用的函数 ================= */

/**
 * @brief 发送数据给上位机
 * @param var1 第1个变量 (单个字符)
 * @param var2 第2个变量 (字符串，必须是6个字符，例如 "+12000")
 * @param var3 第3个变量 (单个字符)
 */
void Host_Send(char var1, const char* var2, char var3);

/**
 * @brief 开启后台 DMA 接收 (仅需在初始化时调用一次)
 */
void Host_Receive_Start(void);

/**
 * @brief 非阻塞地领取一次“收到有效新帧”通知
 * @note  校验、解析和 DMA 重装已经在 UART 中断中完成；本函数只读取并清除
 *        gHostFrameReady，适合在主循环中反复调用，不会等待串口数据。
 * @return true = 从上次调用后收到过有效新帧；false = 暂无有效新帧
 */
bool Host_Receive_Process(void);

void UART_0_INST_IRQHandler(void);

#endif // HOST_COM_H
