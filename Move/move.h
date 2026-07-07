#ifndef __MOVE_H
#define __MOVE_H

#include "ti_msp_dl_config.h"
#include "global.h"

// 定义一个名为 Mode 的枚举类型（必须在使用前定义）
typedef enum
{
   stand = 0 ,    // 默认值为 0
  maintain = 1  // 默认值为 1
} Mode ;

void track_deal_four(uint8_t *s);
float APP_ELE_PID_Calc(int8_t actual_value);
void Chassis(void);
void IrSpeed(int k);
void CountTimerInit(void);
int track_deal_sensor(uint8_t s[]) ;
int stop(void);
void Tracking(void);
int Straight(float dir);
void Straight_Reset(void);
int trackingLeftTurn(void);
int trackingRightTurn(void);
int ModuleTracking(void);
void parking(void);
void backwords(void);
void balance(void);

// 导出angle和balancePWM供AI调参使用
extern float angle;
extern int balancePWM;
extern Mode BalanceMode;

// 定义全局计时器变量
static SoftTimer_t assignment3timer = {0, false};

// 定义assignment3的模式标志
enum Assignment3State {
  PHASE_TRACKING_HALF_SEC = 0,     // 0.5秒循迹
  PHASE_GYRO_STRAIGHT_1_5_SEC = 1, // 1.5秒陀螺仪直线
  PHASE_TRACKING_AGAIN = 2         // 再次循迹
};
static uint8_t assignment3_state = PHASE_TRACKING_HALF_SEC;

extern uint8_t shapeFlag; // 0:圆形, 1:正方形, 2:三角形, 3:椭圆形

#endif