#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <stdint.h>
#include <stdbool.h>
#include "interrupt.h"

extern uint8_t turnCompleted;
extern volatile unsigned long tick_ms;



//电机控制
extern int k1;
extern int k2;
extern int Current_Speed_Left;
extern int Current_Speed_Right;
extern int32_t Left_count, Right_count;
extern float Current_Dir;
extern volatile uint8_t gEchoData;
extern int32_t Left_count;
extern int32_t Right_count;
extern int Tracking_Sum;

typedef struct pid_Controller
{
    float Kp;
    float Ki;
    float Kd;
    int Error;
    int Last_Error;
    int Last_Last_Error;
    float Integral;
}PID;
extern PID SpeedPID_L;
extern PID SpeedPID_R;
extern PID TurnPID;
extern PID AnglePID;

typedef struct SpeedCondition
{
    int Target_Speed_Left;
    int Target_Speed_Right;
    int True_Speed_Left;
    int True_Speed_Right;

    int PWM_Left;
    int PWM_Right;
}SpeCon;


//陀螺仪控制
typedef struct {
    float pitch;
    float roll;
    float yaw;
    float temperature;
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t gx;
    int16_t gy;
    int16_t gz;
    int16_t version;
} WIT_Data_t;
extern WIT_Data_t wit_data;

//OLED部分全局变量
extern uint8_t oled_buffer[64];

//K230通信部分
extern char Camera_buffer[20];
extern volatile char ReceiveData;
extern uint8_t BufferIndex;
extern int ReceivingFlag;//接收标志
extern int ProcessFlag;//处理标志
extern int dataCount;
extern int Final_data[20];
extern int CameraData[20];
extern int num_of_data;

//转向环全局变量
extern float Target_Dir;
extern float Current_Dir;
extern int8_t CrossingFlag;
extern int EnableCrossingFlag;
extern int indexx_tracking;
extern unsigned long Tick_Start;
extern uint8_t turnFlag;

//云台信息
struct Data {
  float yaw;
  float pitch;
  float x;
  float y;
};
extern struct Data Yuntai;


//Move part
extern uint8_t x[12] ;
extern uint16_t IrSensorNumber;
extern int last_tracking_error;
extern int indexx_move;
extern int32_t encoderLeftCount;
extern int32_t encoderRightCount;
extern int8_t encoderFlag;
extern volatile uint8_t speedControlTick;

//AssignmentChoose
extern uint8_t assignmentFlag;
extern uint8_t Number_of_circle;
extern void (*assignment_function[8])(void);
extern uint8_t shapeFlag;  // 0:圆形, 1:正方形, 2:三角形, 3:椭圆形


//任务运行状态及参数
extern uint8_t LeftTurnFlag;
extern uint8_t RightTurnFlag;
extern uint8_t TurnOverFlag;
extern int stageFlag;

//超声波模块
extern uint32_t Distance;

//循迹转弯部分
extern uint8_t turnCompleted;


#endif