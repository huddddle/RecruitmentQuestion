#ifndef TRACKINGIIC_H
#define TRACKINGIIC_H

#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

extern uint8_t TrkI2C_x[12];         // I2C循迹12路数据，0为白，1为黑
extern uint16_t TrkI2C_IrSensorNumber; // I2C循迹触发数量
extern int TrkI2C_Tracking_Sum;      // I2C循迹加权偏差
extern int TrkI2C_last_tracking_error; // I2C循迹上次偏差
extern int gTrackErrorChange;       // 偏差变化率 (可用于PID的d项)extern
extern uint16_t gTrackRawState12;   // 原始状态寄存器

extern volatile bool gTrackSensorOnline; // I2C在线状态

void trackSensorIndicatorSet(bool sensorOnline);

bool trackSensorUpdate(void);
int trackSensorResetData(void);

#endif // TRACKINGIIC_H