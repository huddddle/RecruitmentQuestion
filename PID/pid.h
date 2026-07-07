#ifndef __PID_H_
#define __PID_H_

#include "wit.h"
#include "global.h"

int Speed(PID*Example, int True_Speed, int Target_Speed);
int Loc_PID_Control(PID *Example, float Current_Value, float Target_Value) ;

#endif