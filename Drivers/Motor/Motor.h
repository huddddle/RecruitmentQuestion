#ifndef __MOTOR_H
#define __MOTOR_H

#include "ti_msp_dl_config.h"



#define AIN1_OUT(X)  ( (X) ? (DL_GPIO_setPins(TB6612_AIN1_PORT,TB6612_AIN1_PIN)) : (DL_GPIO_clearPins(TB6612_AIN1_PORT,TB6612_AIN1_PIN)) )
#define AIN2_OUT(X)  ( (X) ? (DL_GPIO_setPins(TB6612_AIN2_PORT,TB6612_AIN2_PIN)) : (DL_GPIO_clearPins(TB6612_AIN2_PORT,TB6612_AIN2_PIN)) )

#define BIN1_OUT(X)  ( (X) ? (DL_GPIO_setPins(TB6612_BIN1_PORT,TB6612_BIN1_PIN)) : (DL_GPIO_clearPins(TB6612_BIN1_PORT,TB6612_BIN1_PIN)) )
#define BIN2_OUT(X)  ( (X) ? (DL_GPIO_setPins(TB6612_BIN2_PORT,TB6612_BIN2_PIN)) : (DL_GPIO_clearPins(TB6612_BIN2_PORT,TB6612_BIN2_PIN)) )

#define CAR_APB 150.0f//195 105和轮距相关，可调节转向幅度

void TB6612_Motor_Stop(void);
void Left_Control(uint8_t dir, int32_t speed);
void Right_Control(uint8_t dir, int32_t speed);

#endif  /* __MOTOR_H */