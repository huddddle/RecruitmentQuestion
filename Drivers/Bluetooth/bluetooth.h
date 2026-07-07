#ifndef _BLUETOOTH_H_
#define _BLUETOOTH_H_

#include <stdint.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"

// 蓝牙模块初始化（初始化DMA发送功能）
void Bluetooth_Init(void);

// === 阻塞发送函数（兼容旧代码） ===
// 参数: str 字符串指针， max_len 最大发送长度限制
void Bluetooth_SendString(const char *str, uint16_t max_len);

// 发送L1, L2的函数也可以保留一下
void Bluetooth_SendData(float l1, float l2);

// === DMA非阻塞发送函数（推荐用于高频控制场景） ===

/**
 * @brief 使用DMA发送角度和PID数据（非阻塞）
 * @param angle 当前角度值
 * @param kp PID的P参数
 * @param ki PID的I参数
 * @param kd PID的D参数
 * @return true 发送成功启动，false 上一次发送未完成
 * @note 此函数立即返回，不会阻塞控制逻辑
 */
bool Bluetooth_SendAnglePID_DMA(float angle, float kp, float ki, float kd);

/**
 * @brief 使用DMA发送自定义字符串（非阻塞）
 * @param str 要发送的字符串
 * @return true 发送成功启动，false 上一次发送未完成或参数错误
 * @note 字符串会被复制到内部缓冲区，调用后可以立即修改原字符串
 */
bool Bluetooth_SendString_DMA(const char *str);

/**
 * @brief 检查DMA发送是否完成
 * @return true 发送完成，false 正在发送
 */
bool Bluetooth_IsTxComplete(void);

#endif /* _BLUETOOTH_H_ */
