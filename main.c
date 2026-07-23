/*
 * Copyright (c) 2021, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "uart_gimbal.h"
#include "servo.h"
#include "main.h"
#include "Motor.h"
#include "move.h"
#include "pid.h"
#include "stdio.h"
#include "stdlib.h"
#include "ti_msp_dl_config.h"
#include "assignment.h"
#include "speed.h"
#include "turn.h"
#include"interrupt.h"
#include "global.h"
#include "wit.h"
#include "trackingiic.h"
#include "adc_angle.h"
#include "hostcom.h"
#include "drive_board.h"

/* ================= 串口电机驱动板演示参数 =================
 * 旧 TB6612 驱动仍使用 Left_Control()/Right_Control()；
 * 新驱动板只使用 DriveBoard_*()，两套接口互不冲突。
 */
#define DRIVE_BOARD_DEMO_ENABLE       (1U)
#define DRIVE_BOARD_DEMO_MIN_SPEED    (10)
#define DRIVE_BOARD_DEMO_MAX_SPEED    (50)
#define DRIVE_BOARD_DEMO_SPEED_STEP   (5)
#define DRIVE_BOARD_DEMO_STEP_TIME_MS (200U)

//  ryx test7 7u
uint8_t oled_buffer[64];
volatile uint8_t gEchoData = 0;
void (*assignment_function[8])(void) = {assignment0, assignment1, assignment2,
                                        assignment3, assignment4, assignment5,
                                        assignment6, assignment7};

int main(void) 
 {
  int16_t driveBoardSpeed = DRIVE_BOARD_DEMO_MIN_SPEED;
  bool driveBoardAccelerating = true;
  uint32_t driveBoardNextUpdateMs = 0U;
  int16_t driveBoardEncoderCounts[DRIVE_BOARD_ENCODER_COUNT] = {0};

  SYSCFG_DL_init();
  SysTick_Init();
  DriveBoard_Init();

  OLED_Init(); 
  WIT_Init();

  Host_Receive_Start(); // 开启上位机DMA 接收
  // Servo_Init();
  NVIC_EnableIRQ(TIMER_SwingUp_INST_INT_IRQN);   //开启角度采集中断 
  Speed_Init();

  Left_Control(1, 0);
  Right_Control(1, 0);

  AssignmentChoose();

  OLED_ShowString(0, 0, (uint8_t *)"Yaw:", 8);
  OLED_ShowString(0, 2, (uint8_t *)"Stage:", 8);
  OLED_ShowString(0, 4, (uint8_t *)"AssiFlag:", 8);
  OLED_ShowString(0, 6, (uint8_t *)"SenNumber:", 8);

  ADC_Angle_Init();



// #if DRIVE_BOARD_DEMO_ENABLE
//   /* DriveBoard_Init() 已根据 drive_board.c 顶部配置完成模式和 PID 设置。
//    * 如某一路编码器方向相反，再单独启用；不要默认全部取反。
//    */
//   // (void)DriveBoard_SetEncoderPolarity(1U, true);
//   // (void)DriveBoard_ClearEncoders();

//   driveBoardNextUpdateMs = (uint32_t)tick_ms;
// #endif

  while (1)
  {
//     /* 必须高频调用：只做状态检查，不等待、不阻塞。 */
//     DriveBoard_Process();

//     /* 收到驱动板主动上报的有效 0x03 帧时，复制四路累计编码器值。 */
//     if (DriveBoard_GetEncoderCounts(driveBoardEncoderCounts)) {
//       /* driveBoardEncoderCounts[0..3] 可在此处参与里程/速度计算。 */
//     }

// #if DRIVE_BOARD_DEMO_ENABLE
//     /* ================= 四电机先加速、后减速循环示例 ================= */
//     if ((int32_t)((uint32_t)tick_ms - driveBoardNextUpdateMs) >= 0) {
//       int16_t command = (int16_t)-driveBoardSpeed;
//       (void)DriveBoard_SetSpeeds(command, command, command, command);
//       driveBoardNextUpdateMs =
//           (uint32_t)tick_ms + DRIVE_BOARD_DEMO_STEP_TIME_MS;

//       if (driveBoardAccelerating) {
//         if (driveBoardSpeed >= DRIVE_BOARD_DEMO_MAX_SPEED) {
//           driveBoardAccelerating = false;
//           driveBoardSpeed -= DRIVE_BOARD_DEMO_SPEED_STEP;
//         } else {
//           driveBoardSpeed += DRIVE_BOARD_DEMO_SPEED_STEP;
//         }
//       } else {
//         if (driveBoardSpeed <= DRIVE_BOARD_DEMO_MIN_SPEED) {
//           driveBoardAccelerating = true;
//           driveBoardSpeed += DRIVE_BOARD_DEMO_SPEED_STEP;
//         } else {
//           driveBoardSpeed -= DRIVE_BOARD_DEMO_SPEED_STEP;
//         }
//       }
//     }
// #endif

    trackSensorUpdate();
sprintf((char *)oled_buffer, "%.4f", wit_data.yaw);
OLED_ShowString(7 * 8, 0, oled_buffer, 16);
sprintf((char *)oled_buffer, "%d", stageFlag);
OLED_ShowString(7 * 8, 2, oled_buffer, 16);
sprintf((char *)oled_buffer, "%d", assignmentFlag);
OLED_ShowString(9 * 8, 4, oled_buffer, 16);
sprintf((char *)oled_buffer, "%d", TrkI2C_IrSensorNumber);
OLED_ShowString(9 * 8, 6, oled_buffer, 16);

assignment_function[assignmentFlag]();
/* 非阻塞检查上位机数据；收到有效帧才刷新 OLED (行首 "Rx:" 之后) */
// if (Host_Receive_Process())
// {
// if (g_Host_Var1 != '4')
// {
//   LightAndSound();
// }
// }
  }
}
