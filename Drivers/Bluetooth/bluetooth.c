#include "bluetooth.h"
#include "uart_dma_tx.h"
#include <string.h>
#include <stdio.h>
#define BT_BUFFER_SIZE 64

// 静态缓冲区用于DMA发送（必须保持有效直到发送完成）
static char gBtTxBuffer[BT_BUFFER_SIZE];

void Bluetooth_Init(void)
{
    // SysConfig 的 SYSCFG_DL_init() 已经把 UART 外设初始化好了
    // 初始化DMA发送功能
    UART_DMA_TX_Init();
}

/**
 * @brief 发送带有长度限制的自定义字符串
 * @param str 要发送的字符串
 * @param max_len 限制的最大发送长度 (防止溢出卡死)
 */
void Bluetooth_SendString(const char *str, uint16_t max_len)
{
    if (str == NULL || max_len == 0) return;
    
    uint16_t count = 0;
    
    // 遇到结束符 '\0' 或 达到最大长度时停止发送
    while (*str != '\0' && count < max_len) 
    {
        // 阻塞发送单个字节（如果串口正在发，会等待发完这个字节）
        DL_UART_transmitDataBlocking(UART_BT_INST, (uint8_t)(*str));
        str++;
        count++;
    }
}


/**
 * @brief 发送题目要求的数据格式
 */
void Bluetooth_SendData(float l1, float l2)
{
    char buffer[BT_BUFFER_SIZE];

    // 把浮点数格式化进 buffer
    snprintf(buffer, sizeof(buffer), "l1=%.2f, l2=%.2f\r\n", l1, l2);

    // 调用上面写的限制长度的函数，防止格式化出错时无限发送
    Bluetooth_SendString(buffer, BT_BUFFER_SIZE);
}

/**
 * @brief 使用DMA发送角度和PID数据（非阻塞）
 * @param angle 当前角度值
 * @param kp PID的P参数
 * @param ki PID的I参数
 * @param kd PID的D参数
 * @return true 发送成功启动，false 上一次发送未完成
 * @note 此函数立即返回，不会阻塞控制逻辑
 */
bool Bluetooth_SendAnglePID_DMA(float angle, float kp, float ki, float kd)
{
    // 格式化数据到静态缓冲区
    int len = snprintf(gBtTxBuffer, BT_BUFFER_SIZE,
                       "Angle=%.2f,Kp=%.3f,Ki=%.3f,Kd=%.3f\r\n",
                       angle, kp, ki, kd);

    if (len <= 0 || len >= BT_BUFFER_SIZE) {
        return false;  // 格式化失败
    }

    // 通过DMA发送（非阻塞）
    return UART_DMA_TX_Send((uint8_t*)gBtTxBuffer, len);
}

/**
 * @brief 使用DMA发送自定义字符串（非阻塞）
 * @param str 要发送的字符串
 * @return true 发送成功启动，false 上一次发送未完成或参数错误
 * @note 字符串会被复制到内部缓冲区，调用后可以立即修改原字符串
 */
bool Bluetooth_SendString_DMA(const char *str)
{
    if (str == NULL) {
        return false;
    }

    // 复制字符串到静态缓冲区（限制长度）
    size_t len = 0;
    while (str[len] != '\0' && len < (BT_BUFFER_SIZE - 1)) {
        gBtTxBuffer[len] = str[len];
        len++;
    }
    gBtTxBuffer[len] = '\0';

    if (len == 0) {
        return false;
    }

    // 通过DMA发送（非阻塞）
    return UART_DMA_TX_Send((uint8_t*)gBtTxBuffer, len);
}

/**
 * @brief 检查DMA发送是否完成
 * @return true 发送完成，false 正在发送
 */
bool Bluetooth_IsTxComplete(void)
{
    return UART_DMA_TX_IsComplete();
}


