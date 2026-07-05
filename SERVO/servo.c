#include "servo.h"
#include "global.h"


/******************************************************************
   配置占空比 范围 0 ~ (per-1)
   t = 0.5ms-------------------舵机会转动 0 °
   t = 1.0ms-------------------舵机会转动 45°
   t = 1.5ms-------------------舵机会转动 90°
   t = 2.0ms-------------------舵机会转动 135°
   t = 2.5ms-------------------舵机会转动180°  
  // 计算PWM占空比
  // 0.5ms对应的计数 = 20
  // 2.5ms对应的计数 = 100
******************************************************************/


/*
注：在坐标系中
X轴移动角度为-120~+150
Y轴移动的角度为-135~+135
*/

struct Data Yuntai;

void setXY(float X,float Y)
{
  Yuntai.x=X;
  Yuntai.y=Y;
  AttitudeAlgorithm( Yuntai.x, Yuntai.y);
  return;
}

// void AttitudeAlgorithm(float x,float y)
// {
//   Yuntai.pitch = atan(y / Yuntai.distance) / Pai * 180.0;
//   Yuntai.yaw = atan(x / Yuntai.distance) / Pai * 180.0;
//   Set_Servo_Angle(Yuntai.yaw,Yuntai.pitch);
// }

void Set_Servo_Angle(float yaw,float pitch) 
{
  uint32_t period = 6400;
  // 0位置矫正

  yaw+=40;
  pitch+=40;

  _yawRangeLimite(&yaw);
  _pitchRangeLimite(&pitch);
  float min_count = 160.0f;  // 最小CompareValue
  float max_count = 800.0f; // 最大CompareValue
  float range = max_count - min_count; // 占空比大小范围
  float PitchValue = min_count + (((float)pitch / 180.0f) * range);
  float YawValue = min_count + (((float)yaw / 180.0f) * range);

  DL_TimerA_setCaptureCompareValue(
      PWM_SERVO_INST, (int)(period - (YAW_MIDDLE_COMPAREVALUE + YawValue)),
      GPIO_PWM_SERVO_C0_IDX);
  DL_TimerA_setCaptureCompareValue(
      PWM_SERVO_INST, (int)(period - (PITCH_MIDDLE_COMPAREVALUE + PitchValue)),
      GPIO_PWM_SERVO_C1_IDX);
}


void _yawRangeLimite(float *x) {
  if (*x >90.00) {
    *x = 90.00;
  }
  if (*x <-90.0) {
    *x = -90.00;
  }
}
void _pitchRangeLimite(float *y) {
  if (*y >90.00) {
    *y = 90.00;
  }
  if (*y <-90.0) {
    *y = -90.00;
  }
}

void Servo_Init(void)
{
    // 舵机PWM初始化
  DL_TimerG_startCounter(PWM_SERVO_INST);
}



