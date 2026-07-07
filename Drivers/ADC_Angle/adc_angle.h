#ifndef ADC_ANGLE_H
#define ADC_ANGLE_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

extern volatile uint16_t adcAngleRaw;

void ADC_Angle_Init(void);
float ADC_Angle_GetDegree(void);

#endif // ADC_ANGLE_H
