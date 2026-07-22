#include "drive_board.h"

#include <stddef.h>

#include "clock.h"
#include "ti_msp_dl_config.h"

#define DRIVE_BOARD_ADDRESS             (0x0AU)
#define MODBUS_READ_HOLDING_REGISTERS   (0x03U)
#define MODBUS_WRITE_SINGLE_REGISTER    (0x06U)
#define MODBUS_WRITE_MULTIPLE_REGISTERS (0x10U)

#define DRIVE_BOARD_REG_SPEED_1         (0x0000U)
#define DRIVE_BOARD_REG_ENCODER_CLEAR_1 (0x0004U)
#define DRIVE_BOARD_REG_CLOSED_LOOP     (0x0008U)
#define DRIVE_BOARD_REG_ENCODER_POL_1   (0x0009U)
#define DRIVE_BOARD_REG_PID_1_KP        (0x0015U)

#define DRIVE_BOARD_MAX_FRAME_BYTES     (33U)
#define DRIVE_BOARD_QUEUE_DEPTH         (8U)
#define DRIVE_BOARD_SPEED_GAP_MS        (2U)
#define DRIVE_BOARD_CONFIG_GAP_MS       (20U)
#define DRIVE_BOARD_RX_MAX_BYTES        (13U)

/* ================= 用户启动配置（集中在本文件修改） =================
 * 1: 上电初始化为速度闭环，并自动写入下面四路 PID。
 * 0: 保持驱动板上电默认的直接控制模式，不发送闭环和 PID 配置帧。
 */
#define DRIVE_BOARD_START_CLOSED_LOOP    (1U)

#define DRIVE_BOARD_MOTOR1_KP            (40.0f)
#define DRIVE_BOARD_MOTOR1_KI            (4.9f)
#define DRIVE_BOARD_MOTOR1_KD            (0.0f)
#define DRIVE_BOARD_MOTOR2_KP            (40.0f)
#define DRIVE_BOARD_MOTOR2_KI            (4.9f)
#define DRIVE_BOARD_MOTOR2_KD            (0.0f)
#define DRIVE_BOARD_MOTOR3_KP            (40.0f)
#define DRIVE_BOARD_MOTOR3_KI            (4.9f)
#define DRIVE_BOARD_MOTOR3_KD            (0.0f)
#define DRIVE_BOARD_MOTOR4_KP            (40.0f)
#define DRIVE_BOARD_MOTOR4_KI            (4.9f)
#define DRIVE_BOARD_MOTOR4_KD            (0.0f)

typedef struct
{
    uint8_t data[DRIVE_BOARD_MAX_FRAME_BYTES];
    uint8_t length;
    uint8_t postGapMs;
} DriveBoard_TxFrame;

volatile uint32_t gDriveBoardQueueOverflowCount = 0U;
volatile uint32_t gDriveBoardRxFrameCount = 0U;

static DriveBoard_TxFrame gTxQueue[DRIVE_BOARD_QUEUE_DEPTH];
static volatile uint8_t gTxHead = 0U;
static volatile uint8_t gTxTail = 0U;
static volatile uint8_t gTxCount = 0U;
static volatile bool gTxActive = false;
static volatile bool gGapActive = false;
static volatile uint32_t gGapDeadlineMs = 0U;
static volatile int16_t gEncoderCounts[DRIVE_BOARD_ENCODER_COUNT] = {0};
static volatile bool gEncoderDataNew = false;

static const DriveBoard_PID_t gStartupPid[DRIVE_BOARD_MOTOR_COUNT] = {
    {DRIVE_BOARD_MOTOR1_KP, DRIVE_BOARD_MOTOR1_KI, DRIVE_BOARD_MOTOR1_KD},
    {DRIVE_BOARD_MOTOR2_KP, DRIVE_BOARD_MOTOR2_KI, DRIVE_BOARD_MOTOR2_KD},
    {DRIVE_BOARD_MOTOR3_KP, DRIVE_BOARD_MOTOR3_KI, DRIVE_BOARD_MOTOR3_KD},
    {DRIVE_BOARD_MOTOR4_KP, DRIVE_BOARD_MOTOR4_KI, DRIVE_BOARD_MOTOR4_KD}
};

static uint16_t DriveBoard_CRC16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFFU;

    while (length-- > 0U) {
        uint8_t bit;
        crc ^= *data++;
        for (bit = 0U; bit < 8U; bit++) {
            crc = ((crc & 1U) != 0U) ?
                (uint16_t)((crc >> 1U) ^ 0xA001U) : (uint16_t)(crc >> 1U);
        }
    }
    return crc;
}

static void DriveBoard_StartNextTransfer(void)
{
    DriveBoard_TxFrame *frame;

    if (gTxActive || gGapActive || (gTxCount == 0U)) {
        return;
    }

    frame = &gTxQueue[gTxTail];
    gTxActive = true;

    DL_DMA_disableChannel(DMA, MSPMotor_TX_DMA_CHAN_ID);
    DL_DMA_setSrcAddr(DMA, MSPMotor_TX_DMA_CHAN_ID,
        (uint32_t)&frame->data[0]);
    DL_DMA_setDestAddr(DMA, MSPMotor_TX_DMA_CHAN_ID,
        (uint32_t)&MSPMotor_INST->TXDATA);
    DL_DMA_setTransferSize(
        DMA, MSPMotor_TX_DMA_CHAN_ID, (uint16_t)frame->length);
    DL_DMA_enableChannel(DMA, MSPMotor_TX_DMA_CHAN_ID);
}

static bool DriveBoard_QueueFrame(
    const uint8_t *data, uint8_t length, uint8_t postGapMs)
{
    DriveBoard_TxFrame *frame;
    uint32_t interruptState;
    uint8_t index;

    if ((data == NULL) || (length == 0U) ||
        (length > DRIVE_BOARD_MAX_FRAME_BYTES)) {
        return false;
    }

    interruptState = __get_PRIMASK();
    __disable_irq();
    if (gTxCount >= DRIVE_BOARD_QUEUE_DEPTH) {
        gDriveBoardQueueOverflowCount++;
        if (interruptState == 0U) {
            __enable_irq();
        }
        return false;
    }

    frame = &gTxQueue[gTxHead];
    for (index = 0U; index < length; index++) {
        frame->data[index] = data[index];
    }
    frame->length = length;
    frame->postGapMs = postGapMs;
    gTxHead = (uint8_t)((gTxHead + 1U) % DRIVE_BOARD_QUEUE_DEPTH);
    gTxCount++;
    DriveBoard_StartNextTransfer();

    if (interruptState == 0U) {
        __enable_irq();
    }
    return true;
}

static bool DriveBoard_SendFrame(
    uint8_t *frame, uint8_t lengthWithoutCrc, uint8_t postGapMs)
{
    uint16_t crc = DriveBoard_CRC16(frame, lengthWithoutCrc);
    frame[lengthWithoutCrc] = (uint8_t)(crc & 0xFFU);
    frame[lengthWithoutCrc + 1U] = (uint8_t)(crc >> 8U);
    return DriveBoard_QueueFrame(
        frame, (uint8_t)(lengthWithoutCrc + 2U), postGapMs);
}

static bool DriveBoard_WriteSingle(
    uint16_t address, uint16_t value, uint8_t postGapMs)
{
    uint8_t frame[8];

    frame[0] = DRIVE_BOARD_ADDRESS;
    frame[1] = MODBUS_WRITE_SINGLE_REGISTER;
    frame[2] = (uint8_t)(address >> 8U);
    frame[3] = (uint8_t)address;
    frame[4] = (uint8_t)(value >> 8U);
    frame[5] = (uint8_t)value;
    return DriveBoard_SendFrame(frame, 6U, postGapMs);
}

static bool DriveBoard_WriteMultiple(uint16_t startAddress,
                                     const uint16_t *values,
                                     uint8_t registerCount,
                                     uint8_t postGapMs)
{
    uint8_t frame[DRIVE_BOARD_MAX_FRAME_BYTES];
    uint8_t index = 0U;
    uint8_t valueIndex;

    if ((values == NULL) || (registerCount == 0U) ||
        (registerCount > 12U)) {
        return false;
    }

    frame[index++] = DRIVE_BOARD_ADDRESS;
    frame[index++] = MODBUS_WRITE_MULTIPLE_REGISTERS;
    frame[index++] = (uint8_t)(startAddress >> 8U);
    frame[index++] = (uint8_t)startAddress;
    frame[index++] = 0U;
    frame[index++] = registerCount;
    frame[index++] = (uint8_t)(registerCount * 2U);

    for (valueIndex = 0U; valueIndex < registerCount; valueIndex++) {
        frame[index++] = (uint8_t)(values[valueIndex] >> 8U);
        frame[index++] = (uint8_t)values[valueIndex];
    }
    return DriveBoard_SendFrame(frame, index, postGapMs);
}

static uint16_t DriveBoard_PIDToRegister(float value)
{
    if (value <= 0.0f) {
        return 0U;
    }
    if (value >= 65.535f) {
        return UINT16_MAX;
    }
    return (uint16_t)(value * 1000.0f);
}

static void DriveBoard_ParseByte(uint8_t data)
{
    static uint8_t frame[DRIVE_BOARD_RX_MAX_BYTES];
    static uint8_t index = 0U;
    static uint8_t expectedLength = 0U;

    if (index == 0U) {
        if (data == DRIVE_BOARD_ADDRESS) {
            frame[index++] = data;
        }
        return;
    }
    if (index == 1U) {
        if (data != MODBUS_READ_HOLDING_REGISTERS) {
            index = 0U;
            return;
        }
        frame[index++] = data;
        return;
    }
    if (index == 2U) {
        if ((data == 0U) || ((data & 1U) != 0U) ||
            (data > (DRIVE_BOARD_ENCODER_COUNT * 2U))) {
            index = 0U;
            return;
        }
        frame[index++] = data;
        expectedLength = (uint8_t)(data + 5U);
        return;
    }

    frame[index++] = data;
    if (index == expectedLength) {
        uint16_t receivedCrc = (uint16_t)frame[index - 2U] |
            ((uint16_t)frame[index - 1U] << 8U);
        uint16_t calculatedCrc =
            DriveBoard_CRC16(frame, (uint16_t)(index - 2U));

        if (receivedCrc == calculatedCrc) {
            uint8_t count = (uint8_t)(frame[2] / 2U);
            uint8_t motor;
            for (motor = 0U; motor < count; motor++) {
                uint8_t dataIndex = (uint8_t)(3U + motor * 2U);
                gEncoderCounts[motor] = (int16_t)(
                    ((uint16_t)frame[dataIndex] << 8U) |
                    frame[dataIndex + 1U]);
            }
            gEncoderDataNew = true;
            gDriveBoardRxFrameCount++;
        }
        index = 0U;
        expectedLength = 0U;
    }
}

void DriveBoard_Init(void)
{
    gTxHead = 0U;
    gTxTail = 0U;
    gTxCount = 0U;
    gTxActive = false;
    gGapActive = false;
    gDriveBoardQueueOverflowCount = 0U;
    gDriveBoardRxFrameCount = 0U;
    gEncoderDataNew = false;

    DL_DMA_disableChannel(DMA, MSPMotor_TX_DMA_CHAN_ID);
    NVIC_ClearPendingIRQ(MSPMotor_INST_INT_IRQN);
    NVIC_EnableIRQ(MSPMotor_INST_INT_IRQN);

#if DRIVE_BOARD_START_CLOSED_LOOP
    /* 两帧进入同一 DMA 队列：闭环帧 EOT 后等待 20 ms，再发送 PID。 */
    (void)DriveBoard_SetClosedLoop();
    (void)DriveBoard_SetPID4(&gStartupPid[0], &gStartupPid[1],
                             &gStartupPid[2], &gStartupPid[3]);
#endif
}

void DriveBoard_Process(void)
{
    uint32_t interruptState = __get_PRIMASK();
    __disable_irq();
    if (gGapActive &&
        ((int32_t)((uint32_t)tick_ms - gGapDeadlineMs) >= 0)) {
        gGapActive = false;
        DriveBoard_StartNextTransfer();
    }
    if (interruptState == 0U) {
        __enable_irq();
    }
}

bool DriveBoard_SetClosedLoop(void)
{
    return DriveBoard_WriteSingle(DRIVE_BOARD_REG_CLOSED_LOOP, 1U,
        DRIVE_BOARD_CONFIG_GAP_MS);
}

bool DriveBoard_SetSpeeds(
    int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4)
{
    const uint16_t values[DRIVE_BOARD_MOTOR_COUNT] = {
        (uint16_t)motor1, (uint16_t)motor2,
        (uint16_t)motor3, (uint16_t)motor4
    };
    return DriveBoard_WriteMultiple(DRIVE_BOARD_REG_SPEED_1, values,
        DRIVE_BOARD_MOTOR_COUNT, DRIVE_BOARD_SPEED_GAP_MS);
}

bool DriveBoard_SetPID4(const DriveBoard_PID_t *motor1,
                        const DriveBoard_PID_t *motor2,
                        const DriveBoard_PID_t *motor3,
                        const DriveBoard_PID_t *motor4)
{
    const DriveBoard_PID_t *motors[DRIVE_BOARD_MOTOR_COUNT] = {
        motor1, motor2, motor3, motor4
    };
    uint16_t values[DRIVE_BOARD_MOTOR_COUNT * 3U];
    uint8_t motor;

    for (motor = 0U; motor < DRIVE_BOARD_MOTOR_COUNT; motor++) {
        if (motors[motor] == NULL) {
            return false;
        }
        values[motor * 3U] = DriveBoard_PIDToRegister(motors[motor]->kp);
        values[motor * 3U + 1U] = DriveBoard_PIDToRegister(motors[motor]->ki);
        values[motor * 3U + 2U] = DriveBoard_PIDToRegister(motors[motor]->kd);
    }
    return DriveBoard_WriteMultiple(DRIVE_BOARD_REG_PID_1_KP, values,
        DRIVE_BOARD_MOTOR_COUNT * 3U, DRIVE_BOARD_CONFIG_GAP_MS);
}

bool DriveBoard_SetEncoderPolarity(uint8_t motorIndex, bool reversed)
{
    if ((motorIndex == 0U) || (motorIndex > DRIVE_BOARD_MOTOR_COUNT)) {
        return false;
    }
    return DriveBoard_WriteSingle((uint16_t)(
        DRIVE_BOARD_REG_ENCODER_POL_1 + motorIndex - 1U),
        reversed ? 1U : 0U, DRIVE_BOARD_CONFIG_GAP_MS);
}

bool DriveBoard_ClearEncoders(void)
{
    const uint16_t zeros[DRIVE_BOARD_ENCODER_COUNT] = {0U, 0U, 0U, 0U};
    return DriveBoard_WriteMultiple(DRIVE_BOARD_REG_ENCODER_CLEAR_1, zeros,
        DRIVE_BOARD_ENCODER_COUNT, DRIVE_BOARD_CONFIG_GAP_MS);
}

bool DriveBoard_GetEncoderCounts(int16_t counts[DRIVE_BOARD_ENCODER_COUNT])
{
    uint32_t interruptState;
    uint8_t motor;
    bool hadNewData;

    if (counts == NULL) {
        return false;
    }
    interruptState = __get_PRIMASK();
    __disable_irq();
    for (motor = 0U; motor < DRIVE_BOARD_ENCODER_COUNT; motor++) {
        counts[motor] = gEncoderCounts[motor];
    }
    hadNewData = gEncoderDataNew;
    gEncoderDataNew = false;
    if (interruptState == 0U) {
        __enable_irq();
    }
    return hadNewData;
}

bool DriveBoard_IsTxIdle(void)
{
    return (gTxCount == 0U) && !gTxActive && !gGapActive;
}

void MSPMotor_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(MSPMotor_INST)) {
        case DL_UART_MAIN_IIDX_EOT_DONE:
            if (gTxActive) {
                uint8_t postGapMs = gTxQueue[gTxTail].postGapMs;
                DL_DMA_disableChannel(DMA, MSPMotor_TX_DMA_CHAN_ID);
                gTxActive = false;
                gTxTail = (uint8_t)((gTxTail + 1U) % DRIVE_BOARD_QUEUE_DEPTH);
                gTxCount--;
                gGapDeadlineMs = (uint32_t)tick_ms + postGapMs;
                gGapActive = (postGapMs != 0U);
            }
            break;

        case DL_UART_MAIN_IIDX_RX:
            while (!DL_UART_Main_isRXFIFOEmpty(MSPMotor_INST)) {
                DriveBoard_ParseByte(
                    (uint8_t)DL_UART_Main_receiveData(MSPMotor_INST));
            }
            break;

        default:
            break;
    }
}
