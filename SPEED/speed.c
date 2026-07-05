#include "Motor.h"
#include "global.h"
#include "pid.h"
#include "stdlib.h"
#include "speed.h"

//运动学控制
int32_t Left_count = 0;
int32_t Right_count = 0;
int SpeLeft[1000] = {0};
int SpeRight[1000] = {0};
int l = 0, r = 0; // 定义数组索引index
struct SpeedCondition SpeeCon;
int k1,k2;

Loc_PID DistPID_L = {2.0f, 0.0f, 0.2f, 0, 0, 0};
Loc_PID DistPID_R = {2.0f, 0.0f, 0.2f, 0, 0, 0};

#define SPEED_LIMIT_PWM      300
#define DIST_INTEGRAL_LIMIT  3000

typedef struct {
  int error;
  int last_error;
  int last_last_error;
  int pwm;
} SpeedLoopState;

static SpeedLoopState SpeedLoop_L = {0};
static SpeedLoopState SpeedLoop_R = {0};

static int LimitInt(int value, int min, int max)
{
  if (value > max) {
    return max;
  }
  if (value < min) {
    return min;
  }
  return value;
}

static int IncrementalSpeedPID(SpeedLoopState *state, PID *pid, int true_speed, int target_speed)
{
  float delta_pwm;

  state->error = target_speed - true_speed;
  delta_pwm = pid->Kp * (state->error - state->last_error) +
              pid->Ki * state->error +
              pid->Kd * (state->error - 2 * state->last_error + state->last_last_error);

  state->last_last_error = state->last_error;
  state->last_error = state->error;
  state->pwm += (int)delta_pwm;
  state->pwm = LimitInt(state->pwm, -950, 950);

  return state->pwm;
}

static void SetSignedMotorPWM(int left_pwm, int right_pwm)
{
  if (left_pwm >= 0) {
    Left_Control(1, left_pwm);
  } else {
    Left_Control(0, -left_pwm);
  }

  if (right_pwm >= 0) {
    Right_Control(1, right_pwm);
  } else {
    Right_Control(0, -right_pwm);
  }
}

void Speed_Init(void)
{
  NVIC_EnableIRQ(Encoder_INT_IRQN);      // 开启编码器的中断
  NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN); // 开启定时器中断
  DL_TimerA_startCounter(TIMER_0_INST);  // 开始启用定时器
}

void SpeedRead(void) {

  static int32_t Last_Left_count;
  static int32_t Last_Right_count;

  if (l == 999 || r == 999) {
    l = 0;
    r = 0;
  }
  if (abs(-Left_count + Last_Left_count) < 2000) {
    SpeLeft[l++] = -Left_count + Last_Left_count;
  }
  if (abs(-Right_count + Last_Right_count) < 2000) {
    SpeRight[r++] = -Right_count + Last_Right_count;
  }

  Last_Left_count = Left_count;
  Last_Right_count = Right_count;
}

void SpeedSet(struct SpeedCondition *Spe) {
  static float Current_PWM_L = 0;
  static float Current_PWM_R = 0;

  // 增量式PID返回的是本次调整的增量，需要累加到原有PWM上
  Current_PWM_L += Speed(&SpeedPID, Spe->True_Speed_Left, Spe->Target_Speed_Left);
  Current_PWM_R += Speed(&SpeedPID, Spe->True_Speed_Right, Spe->Target_Speed_Right);

  // 对总输出进行限幅，防止超出驱动能力
  if (Current_PWM_L > 1000) Current_PWM_L = 1000;
  if (Current_PWM_L < -1000) Current_PWM_L = -1000;
  if (Current_PWM_R > 1000) Current_PWM_R = 1000;
  if (Current_PWM_R < -1000) Current_PWM_R = -1000;

  k1 = (int)Current_PWM_L;
  k2 = (int)Current_PWM_R;

  // 执行控制
  if (k1 > 0) Left_Control(1, k1);
  else Left_Control(0, -k1);

  if (k2 > 0) Right_Control(1, k2);
  else Right_Control(0, -k2);
}

void SpeedControl(int target_speed_L, int target_speed_R)
{
  int true_speed_L = 0;
  int true_speed_R = 0;
  int pwm_L;
  int pwm_R;

  target_speed_L = LimitInt(target_speed_L, -SPEED_LIMIT_PWM, SPEED_LIMIT_PWM);
  target_speed_R = LimitInt(target_speed_R, -SPEED_LIMIT_PWM, SPEED_LIMIT_PWM);

  if (l != 0) {
    true_speed_L = SpeLeft[l - 1];
  }
  if (r != 0) {
    true_speed_R = SpeRight[r - 1];
  }

  pwm_L = IncrementalSpeedPID(&SpeedLoop_L, &SpeedPID, true_speed_L, target_speed_L);
  pwm_R = IncrementalSpeedPID(&SpeedLoop_R, &SpeedPID, true_speed_R, target_speed_R);

  SetSignedMotorPWM(pwm_L, pwm_R);
}

void DistanceReset(void)
{
  __disable_irq();
  Left_count = 0;
  Right_count = 0;
  __enable_irq();

  DistPID_L.Error = 0;
  DistPID_L.Last_Error = 0;
  DistPID_L.Integral = 0;
  DistPID_R.Error = 0;
  DistPID_R.Last_Error = 0;
  DistPID_R.Integral = 0;

  SpeedLoop_L.error = 0;
  SpeedLoop_L.last_error = 0;
  SpeedLoop_L.last_last_error = 0;
  SpeedLoop_L.pwm = 0;
  SpeedLoop_R.error = 0;
  SpeedLoop_R.last_error = 0;
  SpeedLoop_R.last_last_error = 0;
  SpeedLoop_R.pwm = 0;
}

void DistanceControl(int target_distance_L, int target_distance_R)
{
  int32_t current_dist_L;
  int32_t current_dist_R;
  int target_speed_L;
  int target_speed_R;

  __disable_irq();
  current_dist_L = -Left_count;
  current_dist_R = -Right_count;
  __enable_irq();

  DistPID_L.Error = target_distance_L - current_dist_L;
  DistPID_L.Integral += DistPID_L.Error;
  DistPID_L.Integral = LimitInt(DistPID_L.Integral, -DIST_INTEGRAL_LIMIT, DIST_INTEGRAL_LIMIT);
  target_speed_L = (int)(DistPID_L.Kp * DistPID_L.Error +
                         DistPID_L.Ki * DistPID_L.Integral +
                         DistPID_L.Kd * (DistPID_L.Error - DistPID_L.Last_Error));
  DistPID_L.Last_Error = DistPID_L.Error;
  target_speed_L = LimitInt(target_speed_L, -SPEED_LIMIT_PWM, SPEED_LIMIT_PWM);

  DistPID_R.Error = target_distance_R - current_dist_R;
  DistPID_R.Integral += DistPID_R.Error;
  DistPID_R.Integral = LimitInt(DistPID_R.Integral, -DIST_INTEGRAL_LIMIT, DIST_INTEGRAL_LIMIT);
  target_speed_R = (int)(DistPID_R.Kp * DistPID_R.Error +
                         DistPID_R.Ki * DistPID_R.Integral +
                         DistPID_R.Kd * (DistPID_R.Error - DistPID_R.Last_Error));
  DistPID_R.Last_Error = DistPID_R.Error;
  target_speed_R = LimitInt(target_speed_R, -SPEED_LIMIT_PWM, SPEED_LIMIT_PWM);

  SpeedControl(target_speed_L, target_speed_R);
}
//    Speed Controller
    // SpeeCon.Target_Speed_Left = 500;
    // SpeeCon.Target_Speed_Right = 500;
    // if (r != 0)
    //   SpeeCon.True_Speed_Right = SpeRight[r - 1];
    // if (l != 0)
    //   SpeeCon.True_Speed_Left = SpeLeft[l - 1];
    // SpeedSet(&SpeeCon);

