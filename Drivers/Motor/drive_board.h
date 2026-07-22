#ifndef DRIVE_BOARD_H
#define DRIVE_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRIVE_BOARD_MOTOR_COUNT       (4U)
#define DRIVE_BOARD_ENCODER_COUNT     (4U)

typedef struct
{
    float kp;
    float ki;
    float kd;
} DriveBoard_PID_t;

/* 独立于旧 TB6612 Motor.h 的串口驱动板接口，所有名称均使用 DriveBoard_ 前缀。 */
/* 初始化 UART DMA 队列，并按 drive_board.c 顶部配置设置启动模式和 PID。 */
void DriveBoard_Init(void);
void DriveBoard_Process(void);

bool DriveBoard_SetClosedLoop(void);
bool DriveBoard_SetSpeeds(
    int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4);
bool DriveBoard_SetPID4(const DriveBoard_PID_t *motor1,
                        const DriveBoard_PID_t *motor2,
                        const DriveBoard_PID_t *motor3,
                        const DriveBoard_PID_t *motor4);
bool DriveBoard_SetEncoderPolarity(uint8_t motorIndex, bool reversed);
bool DriveBoard_ClearEncoders(void);

/* 驱动板会主动上报 0x03 编码器帧，不需要主控周期发送读取命令。 */
bool DriveBoard_GetEncoderCounts(int16_t counts[DRIVE_BOARD_ENCODER_COUNT]);
bool DriveBoard_IsTxIdle(void);

extern volatile uint32_t gDriveBoardQueueOverflowCount;
extern volatile uint32_t gDriveBoardRxFrameCount;

#ifdef __cplusplus
}
#endif

#endif /* DRIVE_BOARD_H */
