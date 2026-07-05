#ifndef __SERVO_H__
#define __SERVO_H__

#include "ti_msp_dl_config.h"

#define YAW_MIDDLE_COMPAREVALUE (480)//对应135°
#define PITCH_MIDDLE_COMPAREVALUE (480)//对应135°

void Set_Servo_Angle(float yaw,float pitch);
void _yawRangeLimite(float *x);
void _pitchRangeLimite(float *y);
void AttitudeAlgorithm(float x,float y);
void setXY(float X,float Y);
void Servo_Init(void);

#endif /* __SERVO_H__ */