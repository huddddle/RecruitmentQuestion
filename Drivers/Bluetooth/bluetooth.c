#include "bluetooth.h"
#include <string.h>
#include <stdio.h>
#define BT_BUFFER_SIZE 64

void Bluetooth_Init(void)
{
    // SysConfig 的 SYSCFG_DL_init() 已经把 UART 外设初始化好了
    // 只需要发送
    // 留空，保持接口的完整性。
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


