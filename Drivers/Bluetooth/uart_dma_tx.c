/**
 * @file uart_dma_tx.c
 * @brief UART DMA+FIFO发送驱动 - 实现文件
 * @details 基于官方例程uart_tx_console_multibyte_repeated_fifo_dma实现
 *          使用DMA和FIFO减少CPU占用，适合高频控制场景
 */

#include "uart_dma_tx.h"
#include <string.h>

// DMA通道ID定义 - 使用通道2
#define UART_BT_DMA_CHANNEL_ID    (2)

// 发送状态标志
static volatile bool gDmaTxComplete = false;    // DMA传输完成标志
static volatile bool gUartTxComplete = false;   // UART发送完成标志（EOT）
static volatile bool gTxBusy = false;           // 发送忙标志

/**
 * @brief 初始化UART DMA发送功能
 */
void UART_DMA_TX_Init(void)
{
    // 清除状态标志
    gDmaTxComplete = false;
    gUartTxComplete = false;
    gTxBusy = false;

    // 启用UART_BT的FIFO
    DL_UART_enableFIFOs(UART_BT_INST);

    // 设置FIFO阈值（发送FIFO为1个字节时触发）
    DL_UART_setTXFIFOThreshold(UART_BT_INST, DL_UART_TX_FIFO_LEVEL_ONE_ENTRY);

    // 启用UART中断：DMA完成和EOT（End of Transmission）
    DL_UART_enableInterrupt(UART_BT_INST,
        DL_UART_INTERRUPT_DMA_DONE_TX | DL_UART_INTERRUPT_EOT_DONE);

    // 启用DMA发送事件
    DL_UART_enableDMATransmitEvent(UART_BT_INST);

    // 配置DMA通道
    DL_DMA_setSrcAddr(DMA, UART_BT_DMA_CHANNEL_ID, 0);  // 源地址稍后设置
    DL_DMA_setDestAddr(DMA, UART_BT_DMA_CHANNEL_ID, (uint32_t)(&UART_BT_INST->TXDATA));
    DL_DMA_setTransferSize(DMA, UART_BT_DMA_CHANNEL_ID, 0);  // 传输大小稍后设置

    // 配置DMA为byte-to-FIFO模式
    DL_DMA_setSrcIncrement(DMA, UART_BT_DMA_CHANNEL_ID, DL_DMA_ADDR_INCREMENT);
    DL_DMA_setDestIncrement(DMA, UART_BT_DMA_CHANNEL_ID, DL_DMA_ADDR_UNCHANGED);

    // 设置传输宽度
    DL_DMA_setTransferMode(DMA, UART_BT_DMA_CHANNEL_ID, DL_DMA_SINGLE_TRANSFER_MODE);

    // 设置DMA触发源为UART_BT的TX（添加触发类型参数）
    DL_DMA_setTrigger(DMA, UART_BT_DMA_CHANNEL_ID, DMA_UART1_TX_TRIG, DL_DMA_TRIGGER_TYPE_EXTERNAL);

    // 启用UART中断（在NVIC中）
    NVIC_EnableIRQ(UART_BT_INST_INT_IRQN);
}

/**
 * @brief 通过DMA发送数据（非阻塞）
 */
bool UART_DMA_TX_Send(const uint8_t *data, uint16_t size)
{
    // 检查是否有发送正在进行
    if (gTxBusy) {
        return false;  // 上一次发送未完成
    }

    if (data == NULL || size == 0) {
        return false;
    }

    // 设置忙标志
    gTxBusy = true;
    gDmaTxComplete = false;
    gUartTxComplete = false;

    // 配置DMA传输
    DL_DMA_setSrcAddr(DMA, UART_BT_DMA_CHANNEL_ID, (uint32_t)(data));
    DL_DMA_setDestAddr(DMA, UART_BT_DMA_CHANNEL_ID, (uint32_t)(&UART_BT_INST->TXDATA));
    DL_DMA_setTransferSize(DMA, UART_BT_DMA_CHANNEL_ID, size);

    // 禁用睡眠退出（确保DMA传输完成）
    DL_SYSCTL_disableSleepOnExit();

    // 启动DMA传输
    DL_DMA_enableChannel(DMA, UART_BT_DMA_CHANNEL_ID);

    return true;
}

/**
 * @brief 通过DMA发送数据（阻塞等待完成）
 */
void UART_DMA_TX_SendBlocking(const uint8_t *data, uint16_t size)
{
    // 启动发送
    if (!UART_DMA_TX_Send(data, size)) {
        // 如果发送失败（上一次未完成），等待完成
        while (gTxBusy) {
            __WFE();
        }
        // 重试
        UART_DMA_TX_Send(data, size);
    }

    // 等待DMA传输完成
    while (!gDmaTxComplete) {
        __WFE();
    }

    // 等待UART发送完成（EOT）
    while (!gUartTxComplete) {
        __WFE();
    }

    // 清除标志
    gDmaTxComplete = false;
    gUartTxComplete = false;
    gTxBusy = false;
}

/**
 * @brief 检查DMA发送是否完成
 */
bool UART_DMA_TX_IsComplete(void)
{
    return (!gTxBusy && gDmaTxComplete && gUartTxComplete);
}

/**
 * @brief UART中断处理函数
 */
void UART_DMA_TX_IRQHandler(void)
{
    switch (DL_UART_getPendingInterrupt(UART_BT_INST)) {
        case DL_UART_IIDX_DMA_DONE_TX:
            // DMA传输完成（数据已全部写入UART FIFO）
            gDmaTxComplete = true;
            break;

        case DL_UART_IIDX_EOT_DONE:
            // UART发送完成（数据已全部从FIFO发送出去）
            gUartTxComplete = true;
            gTxBusy = false;  // 清除忙标志
            break;

        default:
            break;
    }
}
