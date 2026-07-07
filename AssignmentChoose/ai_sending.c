/**
 * @file ai_sending.c
 * @brief AI自动调参数据发送模块
 * @details 按照llm-pid-tuner要求的CSV格式发送倒立摆PID数据
 */

#include "ai_sending.h"
#include "bluetooth.h"
#include "global.h"
#include "pid.h"
#include <stdio.h>

// 倒立摆垂直位置设定值
#define Middle 215.99f

// 静态缓冲区用于格式化CSV数据
static char ai_data_buffer[128];

/**
 * @brief 发送倒立摆直立环PID数据给AI调参工具
 * @details 数据格式: timestamp_ms,setpoint,input,pwm,error,p,i,d
 *
 * @param angle 当前角度值
 * @param balancePWM 当前PWM输出值
 *
 * @note 此函数应该在控制循环中定期调用（建议10-20Hz）
 * @note 使用DMA非阻塞发送，不会影响控制循环性能
 */
void AISending(float angle, int balancePWM)
{
    // 1. 获取时间戳（毫秒）
    uint32_t timestamp = tick_ms;

    // 2. 设定值（倒立摆垂直位置）
    float setpoint = Middle;  // 215.99度

    // 3. 当前值（当前角度）
    float input = angle;

    // 4. PWM输出
    int pwm = balancePWM;

    // 5. 误差
    float error = setpoint - input;

    // 6. PID三个分量的计算
    // P分量 = Kp * error
    float p_term = AnglePID.Kp * error;

    // I分量 = Ki * integral
    float i_term = AnglePID.Ki * AnglePID.Integral;

    // D分量 = Kd * (error - last_error)
    float d_term = AnglePID.Kd * (error - AnglePID.Last_Error);

    // 7. 格式化为CSV字符串
    // 格式: timestamp_ms,setpoint,input,pwm,error,p,i,d
    int len = snprintf(ai_data_buffer, sizeof(ai_data_buffer),
                       "%u,%.2f,%.2f,%d,%.2f,%.2f,%.2f,%.2f\r\n",
                       (unsigned int)timestamp,
                       setpoint,
                       input,
                       pwm,
                       error,
                       p_term,
                       i_term,
                       d_term);

    // 8. 通过DMA发送（非阻塞）
    if (len > 0 && len < sizeof(ai_data_buffer)) {
        Bluetooth_SendString_DMA(ai_data_buffer);
    }
}

/**
 * @brief 发送倒立摆直立环PID数据（简化版，自动获取angle和PWM）
 * @details 从全局变量中获取当前状态并发送
 *
 * @note 需要在调用前确保angle和balancePWM已经更新
 */
void AISending_Auto(void)
{
    // 从全局变量获取当前角度
    extern float angle;

    // 重新计算PWM（与balance()函数中的逻辑一致）
    int balancePWM = Loc_PID_Control(&AnglePID, angle, Middle);

    // 调用完整版发送函数
    AISending(angle, balancePWM);
}
