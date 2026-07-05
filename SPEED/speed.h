#ifndef SPEED
#define SPEED

#include "global.h"

typedef struct {
    float Kp;
    float Ki;
    float Kd;
    int Error;
    int Last_Error;
    int Integral;
} Loc_PID;

extern Loc_PID DistPID_L;
extern Loc_PID DistPID_R;

void Speed_Init(void);
void SpeedRead(void);
void SpeedSet(struct SpeedCondition *Spe) ;
void SpeedControl(int target_speed_L, int target_speed_R);
void DistanceReset(void);
void DistanceControl(int target_distance_L, int target_distance_R);


#endif
