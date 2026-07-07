/**
 * @file uart_dma_tx.h
 * @brief UART DMA+FIFO发送驱动 - 头文件
 * @details 基于官方例程uart_tx_console_multibyte_repeated_fifo_dma实现
 *          使用DMA和FIFO减少CPU占用，适合高频控制场景
 */

#ifndef _UART_DMA_TX_H_
#define _UART_DMA_TX_H_

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

/**
 * @brief 初始化UART DMA发送功能
 * @details 配置UART_BT的FIFO和DMA通道，启用相关中断
 * @note 必须在SYSCFG_DL_init()之后调用
 */
void UART_DMA_TX_Init(void);

/**
 * @brief 通过DMA发送数据（非阻塞）
 * @param data 要发送的数据指针
 * @param size 数据长度（字节数）
 * @return true 发送成功启动，false 上一次发送未完成
 * @note 此函数会立即返回，不会阻塞CPU
 *       数据缓冲区在发送完成前必须保持有效
 */
bool UART_DMA_TX_Send(const uint8_t *data, uint16_t size);

/**
 * @brief 通过DMA发送数据（阻塞等待完成）
 * @param data 要发送的数据指针
 * @param size 数据长度（字节数）
 * @note 此函数会等待发送完成后才返回
 *       使用WFE低功耗等待，不会空转占用CPU
 */
void UART_DMA_TX_SendBlocking(const uint8_t *data, uint16_t size);

/**
 * @brief 检查DMA发送是否完成
 * @return true 发送完成，false 正在发送
 */
bool UART_DMA_TX_IsComplete(void);

/**
 * @brief UART中断处理函数（需要在中断向量中调用）
 * @note 处理DMA_DONE_TX和EOT_DONE中断
 */
void UART_DMA_TX_IRQHandler(void);

#endif /* _UART_DMA_TX_H_ */
