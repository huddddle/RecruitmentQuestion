#include "Motor.h"
#include "global.h"
#include "pid.h"
#include "stdlib.h"
#include "speed.h"
#include "ti_msp_dl_config.h" 

#define Radios 33.3//单位是mm
#define Pi 3.1415926
#define codePerCircle 366.0

// 静态内部变量：保存上一次的PWM输出，因为速度环使用的是"增量式 PID"，必须加以累加
static int Delta_PWM_L = 0;
static int Delta_PWM_R = 0;

// 初始化距离(位置)PID 的参数（Kp, Ki, Kd,...）
// 这里给出一组示例初值，调试时请根据小车质量和电机功率修改！一般位置环只需 P 计算即可较好运行
Loc_PID DistPID_L = {4.0, 0.0, 0.2, 0, 0, 0};
Loc_PID DistPID_R = {4.0, 0.0, 0.2, 0, 0, 0};

// 初始化定时器
void Speed_Init(void)
{
  //电机编码器初始化
  NVIC_EnableIRQ(Encoder_INT_IRQN);      // 开启编码器的中断
  NVIC_EnableIRQ(TIMER_Encoder_INST_INT_IRQN); // 开启定时器中断
  DL_TimerA_startCounter(TIMER_Encoder_INST);  // 开始启用定时器
}

/******************************************************************
 * 函 数 名 称：SpeedControl
 * 函 数 说 明：闭环速度控制（内环）
 * 函 数 形 参：target_speed_L / R 期望的目标速度值（一周期内的脉冲数）
 * 函 数 返 回：无
 * 备       注：采用增量式PID，直接调用了你在 pid.c 写好的函数
******************************************************************/
void SpeedControl(int target_speed_L, int target_speed_R,int dir)
{
    // 调用增量式PID计算偏差所需增加的PWM
    Delta_PWM_L = Speed(&SpeedPID_L, Current_Speed_Left, target_speed_L);
    Delta_PWM_R = Speed(&SpeedPID_R, Current_Speed_Right, target_speed_R);

    if (Delta_PWM_L > 300)  Delta_PWM_L = 300;
    if (Delta_PWM_L < -300) Delta_PWM_L = -300;
    
    if (Delta_PWM_R > 300)  Delta_PWM_R = 300;
    if (Delta_PWM_R < -300) Delta_PWM_R = -300;

    Left_Control(dir,  target_speed_L+Delta_PWM_L);
    Right_Control(dir, target_speed_R+Delta_PWM_R);

}



/******************************************************************
 * 函 数 名 称：DistanceControl
 * 函 数 说 明：闭环距离控制（外环 + 内环串联）
 * 函 数 形 参：target_distance_L / R 期望到达的目标实际距离
 * 函 数 返 回：无
 * 备       注：利用位置式PID计算出所需要的速度，然后投喂给 SpeedControl 
******************************************************************/
volatile int32_t AbsoluateEncoder=0;
static int32_t  startEncoder_L=0;
static int32_t  startEncoder_R=0;
int8_t encoderFlag=0;
#define DISTANCE_CONTROL_PERIOD_MS 10
#define YAW_CORRECTION_KP 4.0f
#define YAW_CORRECTION_LIMIT 80
#define DISTANCE_SPEED_LIMIT 250

static float NormalizeYawError(float error)
{
    while (error > 180.0f) {
        error -= 360.0f;
    }
    while (error < -180.0f) {
        error += 360.0f;
    }
    return error;
}

static int LimitInt(int value, int minValue, int maxValue)
{
    if (value > maxValue) return maxValue;
    if (value < minValue) return minValue;
    return value;
}


int DistanceControlWithYaw(int target_distance, int dir, float target_yaw)
{
    static float startYaw = 0.0f;

    if (!speedControlTick) {
        return 0;
    }
    speedControlTick = 0;

    if (!AbsoluateEncoder) {
        AbsoluateEncoder = (int32_t)((float)target_distance /
                           (2.0f * Pi * Radios) * codePerCircle);

        __disable_irq();
        startEncoder_L = encoderLeftCount;
        startEncoder_R = encoderRightCount;
        __enable_irq();

        startYaw = target_yaw;
        Reset_PID(&DistPID_L);
        Reset_PID(&DistPID_R);
    }

    int32_t current_left;
    int32_t current_right;

    __disable_irq();
    current_left = encoderLeftCount;
    current_right = encoderRightCount;
    __enable_irq();

    int32_t left_delta = current_left - startEncoder_L;
    int32_t right_delta = current_right - startEncoder_R;

    int32_t left_travel = AbsInt32(left_delta);
    int32_t right_travel = AbsInt32(right_delta);
    int32_t avg_travel = (left_travel + right_travel) / 2;

    if (avg_travel >= AbsoluateEncoder) {
        SpeedControl(0, 0, dir);
        AbsoluateEncoder = 0;
        startEncoder_L = 0;
        startEncoder_R = 0;
        Reset_PID(&DistPID_L);
        Reset_PID(&DistPID_R);
        encoderFlag++;
        return 1;
    }

    DistPID_L.Error = AbsoluateEncoder - avg_travel;
    DistPID_L.Integral += DistPID_L.Error;
    DistPID_L.Integral = LimitInt(DistPID_L.Integral, -3000, 3000);

    int base_speed = (int)(DistPID_L.Kp * DistPID_L.Error +
                           DistPID_L.Ki * DistPID_L.Integral +
                           DistPID_L.Kd * (DistPID_L.Error - DistPID_L.Last_Error));
    DistPID_L.Last_Error = DistPID_L.Error;

    base_speed = LimitInt(base_speed, 0, DISTANCE_SPEED_LIMIT);

    float yaw_error = NormalizeYawError(wit_data.yaw - startYaw);
    int yaw_correction = (int)(YAW_CORRECTION_KP * yaw_error);
    yaw_correction = LimitInt(yaw_correction,
                              -YAW_CORRECTION_LIMIT,
                              YAW_CORRECTION_LIMIT);

    if (dir == 0) {
        yaw_correction = -yaw_correction;
    }

//为了迎合招新题 放弃使用基本距离的速度pid计算
    // int target_speed_L = base_speed + yaw_correction;
    // int target_speed_R = base_speed - yaw_correction;
    int target_speed_L = 120 + yaw_correction;
    int target_speed_R = 120 - yaw_correction;

    target_speed_L = LimitInt(target_speed_L, 0, DISTANCE_SPEED_LIMIT);
    target_speed_R = LimitInt(target_speed_R, 0, DISTANCE_SPEED_LIMIT);

    SpeedControl(target_speed_L, target_speed_R, dir);

    return 0;
}

int DistanceControl(int target_distance, int dir)
{
    return DistanceControlWithYaw(target_distance, dir, wit_data.yaw);
}

void Reset_PID(Loc_PID *pid) {
    pid->Integral = 0;
    pid->Last_Error = 0;
    pid->Error = 0;
}

static int32_t AbsInt32(int32_t value)
{
    return value >= 0 ? value : -value;
}