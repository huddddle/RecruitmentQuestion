#ifndef HOST_COM_H
#define HOST_COM_H

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <stdbool.h>

/* ================= 1. 解算后存放的三个全局变量 ================= */
extern char g_Host_Var1;      // 第一个变量 (ASCII字符，例如 '2')
extern int16_t g_Host_Var2;   // 第二个变量 (转为整数，例如 12000)
extern char g_Host_Var3;      // 第三个变量 (ASCII字符，例如 '6')

/* ================= 2. 底层收发标志位与缓冲区 ================= */
#define HOST_PACKET_LEN (12)  // 数据总长度: $2,+12000,6# (12个字节)

extern uint8_t gTxPacket[128];
extern uint8_t gRxPacket[HOST_PACKET_LEN];

extern volatile bool gCheckUART;
extern volatile bool gDMADone_TX;
extern volatile bool gDMADone_RX;

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
 * @brief 非阻塞地检查并解算上位机数据
 * @note  数据没收齐时立刻返回 false，不做任何等待；
 *        收齐并校验通过时解算到三个全局变量并重新武装 DMA，返回 true
 * @return true = 本次收到一帧有效数据；false = 暂无新数据或校验失败
 */
bool Host_Receive_Process(void);

void UART_0_INST_IRQHandler(void);

#endif // HOST_COM_H
