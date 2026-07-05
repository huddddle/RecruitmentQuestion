#ifndef _BLUETOOTH_H_
#define _BLUETOOTH_H_

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

// 蓝牙模块初始化（纯发送模式可以为空，但保留框架以便后续拓展）
void Bluetooth_Init(void);

// === 你需要的发送自定义字符串函数（带长度上限） ===
// 参数: str 字符串指针， max_len 最大发送长度限制
void Bluetooth_SendString(const char *str, uint16_t max_len);

// 发送L1, L2的函数也可以保留一下
void Bluetooth_SendData(float l1, float l2);

#endif /* _BLUETOOTH_H_ */
