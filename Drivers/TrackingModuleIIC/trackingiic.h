#ifndef TRACKINGIIC_H
#define TRACKINGIIC_H

#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

extern uint8_t x[12];         // 12路传感器二进制数组，1为黑线，0为白
extern uint16_t IrSensorNumber;
extern int Tracking_Sum;            // 加权计算后的偏差总和 (正负值表示偏移方向)
extern int last_tracking_error;     // 上一次的偏差
extern int gTrackErrorChange;       // 偏差变化率 (可用于PID的d项)extern
extern uint16_t gTrackRawState12;   // 原始状态寄存器

extern volatile bool gTrackSensorOnline; // I2C在线状态

#define TrkI2C_x x
#define TrkI2C_IrSensorNumber IrSensorNumber
#define TrkI2C_Tracking_Sum Tracking_Sum
#define TrkI2C_last_tracking_error last_tracking_error
#define TrkI2C_gTrackErrorChange gTrackErrorChange
#define TrkI2C_gTrackRawState12 gTrackRawState12
#define TrkI2C_gTrackSensorOnline gTrackSensorOnline

void trackSensorIndicatorSet(bool sensorOnline);
void trackSensorOledShow(void);

bool trackSensorUpdate(void);
int trackSensorResetData(void);

#endif // TRACKINGIIC_H