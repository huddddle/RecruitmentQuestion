#include "trackingiic.h"

uint8_t TrkI2C_x[12]                = {0};
uint16_t TrkI2C_IrSensorNumber      = 0;
int TrkI2C_Tracking_Sum             = 0;
int TrkI2C_last_tracking_error      = 0;
int TrkI2C_gTrackErrorChange        = 0;
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

#if defined(I2C_TR_INST)
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

    return (((DL_I2C_getControllerStatus(I2C_TR_INST) &
              DL_I2C_CONTROLLER_STATUS_ERROR) != 0U) ||
            (DL_I2C_getRawInterruptStatus(I2C_TR_INST, interruptMask) != 0U));
#else
    return true;
#endif
}

static void trackSensorRecoverController(void)
{
#if TRACK_SENSOR_I2C_READY
    const uint32_t interruptMask = DL_I2C_INTERRUPT_CONTROLLER_NACK |
                                   DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST;

    DL_I2C_clearInterruptStatus(I2C_TR_INST, interruptMask);
    DL_I2C_flushControllerTXFIFO(I2C_TR_INST);

    while (!DL_I2C_isControllerRXFIFOEmpty(I2C_TR_INST)) {
        (void) DL_I2C_receiveControllerData(I2C_TR_INST);
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

        if ((DL_I2C_getControllerStatus(I2C_TR_INST) &
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

        if (!DL_I2C_isControllerRXFIFOEmpty(I2C_TR_INST)) {
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
    DL_I2C_transmitControllerData(I2C_TR_INST, registerAddress);
    DL_I2C_startControllerTransfer(I2C_TR_INST, TRACK_SENSOR_PCA9555_ADDR_7BIT,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1U);

    if (!trackSensorWaitForIdle()) {
        trackSensorRecoverController();
        return false;
    }

    trackSensorRecoverController();
    DL_I2C_startControllerTransfer(I2C_TR_INST, TRACK_SENSOR_PCA9555_ADDR_7BIT,
        DL_I2C_CONTROLLER_DIRECTION_RX, length);

    for (index = 0U; index < length; ++index) {
        if (!trackSensorWaitForRxData()) {
            trackSensorRecoverController();
            return false;
        }

        buffer[index] = DL_I2C_receiveControllerData(I2C_TR_INST);
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
    TrkI2C_IrSensorNumber   = 0U;
    TrkI2C_Tracking_Sum     = 0;
    TrkI2C_gTrackErrorChange = 0;

    for (index = 0U; index < 12U; ++index) {
        TrkI2C_x[index] = 0U;
    }
    return TrkI2C_IrSensorNumber;
}

static void trackSensorUpdateDecodedData(uint16_t rawState)
{
    uint32_t index;
    int previousError = TrkI2C_Tracking_Sum;

    gTrackRawState12 = rawState;
    TrkI2C_IrSensorNumber   = 0U;
    TrkI2C_Tracking_Sum     = 0;

    for (index = 0U; index < 12U; ++index) {
        TrkI2C_x[index] = (uint8_t) ((rawState >> (11U - index)) & 0x01U);
        TrkI2C_IrSensorNumber += TrkI2C_x[index];
        TrkI2C_Tracking_Sum += (int) TrkI2C_x[index] * gTrackWeights[index];
    }

    TrkI2C_gTrackErrorChange   = TrkI2C_Tracking_Sum - previousError;
    TrkI2C_last_tracking_error = TrkI2C_Tracking_Sum;
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
