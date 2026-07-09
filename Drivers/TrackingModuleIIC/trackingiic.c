#include "trackingiic.h"
#include "global.h"
#include "oled_hardware_i2c.h"
#include <stdio.h>

uint8_t x[12]                = {0};
uint16_t IrSensorNumber      = 0;
int Tracking_Sum             = 0;
int last_tracking_error      = 0;
int gTrackErrorChange        = 0;
uint16_t gTrackRawState12    = 0U;
volatile bool gTrackSensorOnline = false;

static const int gTrackWeights[12] = {
    110, 90, 70, 50, 30, 10,
    -10, -30, -50, -70, -90, -110
};

#define TRACK_SENSOR_PCA9555_ADDR_7BIT      (0x20U)
#define TRACK_SENSOR_INPUT_PORT_REGISTER0    (0x00U)
#define TRACK_SENSOR_RAW_MASK                (0x0FFFU)
#define TRACK_SENSOR_I2C_TIMEOUT_LOOPS       (20000U)

// 与OLED共用I2C接口
#if !defined(TRACK_SENSOR_I2C_INST) && defined(I2C_OLED_INST)
#define TRACK_SENSOR_I2C_INST                I2C_OLED_INST
#endif

#if defined(TRACK_SENSOR_I2C_INST)
#define TRACK_SENSOR_I2C_READY               (1)
#else
#define TRACK_SENSOR_I2C_READY               (0)
#endif

#if defined(GPIO_LED_TR_PORT) && defined(GPIO_LED_TR_PIN_0_PIN)
#define TRACK_SENSOR_LED_READY               (1)
#else
#define TRACK_SENSOR_LED_READY               (0)
#endif

void trackSensorIndicatorSet(bool sensorOnline)
{
#if TRACK_SENSOR_LED_READY
    if (sensorOnline) {
        DL_GPIO_clearPins(GPIO_LED_TR_PORT, GPIO_LED_TR_PIN_0_PIN);
    } else {
        DL_GPIO_setPins(GPIO_LED_TR_PORT, GPIO_LED_TR_PIN_0_PIN);
    }
#else
    (void) sensorOnline;
#endif
}

static bool trackSensorControllerHasError(void)
{
#if TRACK_SENSOR_I2C_READY
    const uint32_t interruptMask = DL_I2C_INTERRUPT_CONTROLLER_NACK |
                                   DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST;

    return (((DL_I2C_getControllerStatus(TRACK_SENSOR_I2C_INST) &
              DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) ||
            (DL_I2C_getRawInterruptStatus(TRACK_SENSOR_I2C_INST, interruptMask) != 0U));
#else
    return true;
#endif
}

static void trackSensorRecoverController(void)
{
#if TRACK_SENSOR_I2C_READY
    const uint32_t interruptMask = DL_I2C_INTERRUPT_CONTROLLER_NACK |
                                   DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST;

    DL_I2C_clearInterruptStatus(TRACK_SENSOR_I2C_INST, interruptMask);
    DL_I2C_flushControllerTXFIFO(TRACK_SENSOR_I2C_INST);

    while (!DL_I2C_isControllerRXFIFOEmpty(TRACK_SENSOR_I2C_INST)) {
        (void) DL_I2C_receiveControllerData(TRACK_SENSOR_I2C_INST);
    }
#endif
}

static bool trackSensorWaitForIdle(void)
{
#if TRACK_SENSOR_I2C_READY
    uint32_t timeout = TRACK_SENSOR_I2C_TIMEOUT_LOOPS;

    while (timeout-- > 0U) {
        if (trackSensorControllerHasError()) {
            return false;
        }

        if ((DL_I2C_getControllerStatus(TRACK_SENSOR_I2C_INST) &
             DL_I2C_CONTROLLER_STATUS_IDLE) != 0U) {
            return true;
        }
    }
#endif

    return false;
}

static bool trackSensorWaitForRxData(void)
{
#if TRACK_SENSOR_I2C_READY
    uint32_t timeout = TRACK_SENSOR_I2C_TIMEOUT_LOOPS;

    while (timeout-- > 0U) {
        if (trackSensorControllerHasError()) {
            return false;
        }

        if (!DL_I2C_isControllerRXFIFOEmpty(TRACK_SENSOR_I2C_INST)) {
            return true;
        }
    }
#endif

    return false;
}

static bool trackSensorReadRegisterBytes(uint8_t registerAddress,
    uint8_t *buffer, uint32_t length)
{
#if TRACK_SENSOR_I2C_READY
    uint32_t index;

    if ((buffer == 0) || (length == 0U)) {
        return false;
    }

    if (!trackSensorWaitForIdle()) {
        trackSensorRecoverController();
        return false;
    }

    trackSensorRecoverController();
    DL_I2C_transmitControllerData(TRACK_SENSOR_I2C_INST, registerAddress);
    DL_I2C_startControllerTransfer(TRACK_SENSOR_I2C_INST, TRACK_SENSOR_PCA9555_ADDR_7BIT,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1U);

    if (!trackSensorWaitForIdle()) {
        trackSensorRecoverController();
        return false;
    }

    trackSensorRecoverController();
    DL_I2C_startControllerTransfer(TRACK_SENSOR_I2C_INST, TRACK_SENSOR_PCA9555_ADDR_7BIT,
        DL_I2C_CONTROLLER_DIRECTION_RX, length);

    for (index = 0U; index < length; ++index) {
        if (!trackSensorWaitForRxData()) {
            trackSensorRecoverController();
            return false;
        }

        buffer[index] = DL_I2C_receiveControllerData(TRACK_SENSOR_I2C_INST);
    }

    if (!trackSensorWaitForIdle()) {
        trackSensorRecoverController();
        return false;
    }

    return true;
#else
    (void) registerAddress;
    (void) buffer;
    (void) length;
    return false;
#endif
}

int trackSensorResetData(void)
{
    uint32_t index;

    gTrackRawState12 = 0U;
    IrSensorNumber   = 0U;
    Tracking_Sum     = 0;
    gTrackErrorChange = 0;

    for (index = 0U; index < 12U; ++index) {
        x[index] = 0U;
    }
    return IrSensorNumber;
}

static void trackSensorUpdateDecodedData(uint16_t rawState)
{
    uint32_t index;
    int previousError = Tracking_Sum;

    gTrackRawState12 = rawState;
    IrSensorNumber   = 0U;
    Tracking_Sum     = 0;

    for (index = 0U; index < 12U; ++index) {
        x[index] = (uint8_t) ((rawState >> (11U - index)) & 0x01U);
        IrSensorNumber += x[index];
        Tracking_Sum += (int) x[index] * gTrackWeights[index];
    }

    gTrackErrorChange   = Tracking_Sum - previousError;
    last_tracking_error = Tracking_Sum;
}

bool trackSensorUpdate(void)
{
    uint8_t registerData[2];
    uint16_t rawState;

    if (!trackSensorReadRegisterBytes(TRACK_SENSOR_INPUT_PORT_REGISTER0,
            &registerData[0], 2U)) {
        trackSensorResetData();
        gTrackSensorOnline = false;
        trackSensorIndicatorSet(false);
        return false;
    }

    rawState = ((uint16_t) registerData[1] << 8) | registerData[0];
    rawState &= TRACK_SENSOR_RAW_MASK;

    trackSensorUpdateDecodedData(rawState);
    gTrackSensorOnline = true;
    trackSensorIndicatorSet(true);
    return true;
}

void trackSensorOledShow(void)
{
    char trackBits[13];
    char valueBuffer[32];
    uint32_t index;

    for (index = 0U; index < 12U; ++index) {
        trackBits[index] = x[index] ? '1' : '0';
    }
    trackBits[12] = '\0';

    OLED_ShowString(0, 1, (uint8_t *) "Track:", 8);
    OLED_ShowString(36, 1, (uint8_t *) trackBits, 8);

    OLED_ShowString(0, 3, (uint8_t *) "Cnt:", 8);
    OLED_ShowNum(30, 3, IrSensorNumber, 2, 8);
    OLED_ShowString(84, 3, (uint8_t *) (gTrackSensorOnline ? "ON" : "OFF"), 8);

    OLED_ShowString(0, 5, (uint8_t *) "Sum:", 8);
    (void) snprintf(valueBuffer, sizeof(valueBuffer), "%d", Tracking_Sum);
    OLED_ShowString(30, 5, (uint8_t *) valueBuffer, 8);
}
