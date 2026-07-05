#include "pid.h"
#include "Motor.h"
#include"global.h"

// PID结构体
PID SpeedPID = {1, 0.005, 0}; // 4,0.02,0//////////1,0.05,0
PID TurnPID = {1, 0, 0};

// 速度PID控制器 (改为标准增量式算法)
int Speed(PID *Example, int True_Speed, int Target_Speed) {
  static int Error = 0, Last_Error = 0, Last_Last_Error = 0;
  float Delta_PWM = 0;

  Error = Target_Speed - True_Speed; // 计算当前偏差

  // 增量式公式: Delta = Kp*(E - E1) + Ki*E + Kd*(E - 2*E1 + E2)
  Delta_PWM = Example->Kp * (Error - Last_Error) + 
              Example->Ki * Error + 
              Example->Kd * (Error - 2 * Last_Error + Last_Last_Error);

  Last_Last_Error = Last_Error;
  Last_Error = Error;

  return (int)Delta_PWM;
}

