#include "adc_angle.h"
#include "ti_msp_dl_config.h"

#define SYS_POTENTIOMETER_MAX_ANGLE 333.3f

volatile uint16_t adcAngleRaw = 0;

void ADC_Angle_Init(void)
{
    // 修改为直接软件触发连续转换，绕过复杂的DMA机制
    DL_ADC12_startConversion(ADC12_Angle_INST);
}

float ADC_Angle_GetDegree(void)
{
    // 每次直接去读取ADC MEM0里的最新结果，避免阻塞
    // 由于 repeatMode = true, ADC 会在触发后自动持续采样
    adcAngleRaw = DL_ADC12_getMemResult(ADC12_Angle_INST, DL_ADC12_MEM_IDX_0);
    return ((float)adcAngleRaw / 4095.0f) * SYS_POTENTIOMETER_MAX_ANGLE;
}
