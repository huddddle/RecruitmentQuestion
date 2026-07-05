#include "turn.h"
#include "Motor.h"
#include "global.h"
#include "main.h"
#include "pid.h"
#include "speed.h"
#include "move.h"
#include "interrupt.h"

// 转向
float Target_Dir;
int8_t CrossingFlag=0;//记录转向控制
float Current_Dir=-1000.00;
uint32_t turn_start_time = 0;  // 转向时间戳
uint8_t turnCompleted=0;

uint8_t turnFlag=0;
static SoftTimer_t Turntimer = {0, false};    // 前进计时器
void Left_Turn(float degree)
{
  static int turn_step = 0;

  // ========== 状态 0：记录初始时间（绝对只执行一次） ==========
  if (turn_step == 0) // 已经判断好了是路口，延时70ms等车身过线
  {
    if (NonBlockDelay(&Turntimer, 110))
    {
      turn_step = 1;
    }
    return;
  }

  // ========== 状态 1：非阻塞等待 1 秒 ==========
  if (turn_step == 1)
  {
    Current_Dir = wit_data.yaw;
    Target_Dir = Current_Dir + degree;
    RangeLimite(&Target_Dir, 180.0);
    turn_step = 2; // 初始化完成，进入真正转弯控制状态
    return; 
  }


  // ========== 状态 2：执行电机转向 ==========
  if (turn_step == 2)
  {
    if (fabsf(wit_data.yaw - Target_Dir) < 3.0f)
    {
      turn_step = 3; // 转向完成，进入停止状态
    }
    else
    {
      // 转向没有完成，继续控制电机
      Left_Control(0, 150);  // 左轮正转
      Right_Control(1, 150); // 右轮反转
    }
    return; // 继续转向
  }

  if (turn_step == 3)
  {
    // 停止电机
    Left_Control(1, 0);
    Right_Control(1, 0);
    if (NonBlockDelay(&Turntimer, 30))
    {
      // 延迟完成，转向真正结束
      Current_Dir = -1000.00;
      LeftTurnFlag = 0;
      RightTurnFlag = 0;
      TurnOverFlag = 0;
      turn_step = 0; // 重置状态，准备下一次转向
      turnCompleted++;
      return;
    }
  }
  return;
}

// 右转 x° 函数
void Right_Turn(float degree)
{
  static int turn_step = 0;

  // ========== 状态 0：记录初始时间（绝对只执行一次） ==========
  if (turn_step == 0)
  {
    if (NonBlockDelay(&Turntimer, 113))
    {

      turn_step = 1;
    }
    return;
  }

  // ========== 状态 1：非阻塞等待 1 秒 ==========
  if (turn_step == 1)
  {

    Current_Dir = wit_data.yaw;
    Target_Dir = Current_Dir - degree;
    RangeLimite(&Target_Dir, 180.0);
    turn_step = 2; // 初始化完成，进入真正转弯控制状态
  }

  // ========== 状态 2：执行电机转向 ==========
  if (turn_step == 2)
  {
    if (fabsf(wit_data.yaw - Target_Dir) < 3.0f)
    {
      turn_step = 3; // 转向完成，进入停止状态
    }
    else
    {
      // 转向没有完成，继续控制电机
      Left_Control(1, 150);  // 左轮正转
      Right_Control(0, 150); // 右轮反转
    }
    return;
  }

  if (turn_step == 3)
  {
    // 停止电机
    Left_Control(1, 0);
    Right_Control(1, 0);
    if (NonBlockDelay(&Turntimer, 30))
    {
      // 延迟完成，转向真正结束
      Current_Dir = -1000.00;
      LeftTurnFlag = 0;
      RightTurnFlag = 0;
      TurnOverFlag = 0;
      turn_step = 0; // 重置状态，准备下一次转向
      turnCompleted++;
      return;
    }
  }
  return;
}

void RangeLimite(float*x,float range) {
  if (*x > range) {
    *x -= 2*range;
  }
  if (*x < -range) {
    *x += 2*range;
  }
}

