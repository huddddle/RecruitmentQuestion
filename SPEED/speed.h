#ifndef SPEED_H
#define SPEED_H

#include "global.h"

// ====== 距离（位置）控制器专属的 PID 结构体 ======
typedef struct {
    float Kp;
    float Ki;
    float Kd;
    int Error;
    int Last_Error;
    int Integral;
} Loc_PID;

// 声明外部的两个位置结构体
extern Loc_PID DistPID_L;
extern Loc_PID DistPID_R;

// 初始化声明
void Speed_Init(void);
void Reset_PID(Loc_PID *pid);
static int32_t AbsInt32(int32_t value);
// ====== 控制接口声明 ======
// 速度控制：传入参数为每周期左右桥期望的脉冲跳变值
void SpeedControl(int target_speed_L, int target_speed_R,int dir);

// 距离控制：传入参数为左右轮期望到达的绝对总脉冲位置
extern volatile int32_t AbsoluateEncoder; 
int DistanceControl(int target_distance,int dir);
int DistanceControlWithYaw(int target_distance, int dir, float target_yaw, int basic_speed, bool Distance_pid);

#endif
