/**
 * @file ai_sending.h
 * @brief AI自动调参数据发送模块 - 头文件
 */

#ifndef _AI_SENDING_H_
#define _AI_SENDING_H_

#include <stdint.h>

/**
 * @brief 发送倒立摆直立环PID数据给AI调参工具
 * @param angle 当前角度值
 * @param balancePWM 当前PWM输出值
 * @note 数据格式: timestamp_ms,setpoint,input,pwm,error,p,i,d
 */
void AISending(float angle, int balancePWM);

/**
 * @brief 发送倒立摆直立环PID数据（简化版，自动获取参数）
 * @note 从全局变量中自动获取angle和计算PWM
 */
void AISending_Auto(void);

#endif /* _AI_SENDING_H_ */
