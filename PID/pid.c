#include "pid.h"
#include "Motor.h"
#include"global.h"

// PID结构体
PID SpeedPID_L = {1, 0.005, 0, 0, 0, 0, 0};
PID SpeedPID_R = {1, 0.005, 0, 0, 0, 0, 0};
PID TurnPID = {1, 0, 0, 0, 0, 0, 0};
PID AnglePID = {40.0, 2.0, 0, 0, 0, 0, 0};  // 从 Kp=20 改为 12, Ki改为0.08
PID PosiPID = {12, 0.6, 0, 0, 0, 0, 0};  // 从 Kp=20 改为 12, Ki改为0.08

// 速度PID控制器 (改为标准增量式算法)
int Speed(PID *Example, int True_Speed, int Target_Speed) {
  float Delta_PWM = 0;

  Example->Error = Target_Speed - True_Speed; // 计算当前偏差

  // 增量式公式: Delta = Kp*(E - E1) + Ki*E + Kd*(E - 2*E1 + E2)
  Delta_PWM = Example->Kp * (Example->Error - Example->Last_Error) + 
              Example->Ki * Example->Error + 
              Example->Kd * (Example->Error - 2 * Example->Last_Error + Example->Last_Last_Error);

  Example->Last_Last_Error = Example->Last_Error;
  Example->Last_Error = Example->Error;

  return (int)Delta_PWM;
}

// 位置式PID控制器 (改进版：含积分限幅和输出限幅)
int Loc_PID_Control(PID *Example, float Current_Value, float Target_Value) {
  float Out = 0;
  
  #define INTEGRAL_LIMIT 2000.0f  // 积分限幅：防止积分饱和
  #define OUTPUT_LIMIT 600        // 输出限幅：防止电机过载

  Example->Error = Target_Value - Current_Value; // 计算当前偏差
  Example->Integral += Example->Error;           // 积分累加

  // 积分限幅（防止积分饱和）
  if (Example->Integral > INTEGRAL_LIMIT) {
    Example->Integral = INTEGRAL_LIMIT;
  } else if (Example->Integral < -INTEGRAL_LIMIT) {
    Example->Integral = -INTEGRAL_LIMIT;
  }

  // 位置式公式: Out = Kp * e(k) + Ki * Sum(e) + Kd * (e(k) - e(k-1))
  Out = Example->Kp * Example->Error + 
        Example->Ki * Example->Integral + 
        Example->Kd * (Example->Error - Example->Last_Error);

  // 输出限幅（防止电机驱动过载）
  if (Out > OUTPUT_LIMIT) {
    Out = OUTPUT_LIMIT;
  } else if (Out < -OUTPUT_LIMIT) {
    Out = -OUTPUT_LIMIT;
  }

  Example->Last_Error = Example->Error; // 更新上一次偏差

  return (int)Out;
}

