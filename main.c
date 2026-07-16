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

//  ryx test
uint8_t oled_buffer[64];
volatile uint8_t gEchoData = 0;
void (*assignment_function[8])(void)=
{
  assignment0, 
  assignment1, 
  assignment2, 
  assignment3, 
  assignment4, 
  assignment5, 
};

int main(void) 
 {
  SYSCFG_DL_init();
  SysTick_Init();

  OLED_Init(); 
  WIT_Init();


  // Servo_Init();
  NVIC_EnableIRQ(TIMER_SwingUp_INST_INT_IRQN);   //开启角度采集中断 
  Speed_Init();

  Left_Control(1, 0);
  Right_Control(1, 0);

  AssignmentChoose();

  OLED_ShowString(0, 0, (uint8_t *)"Rx:", 8);
  OLED_ShowString(0, 2, (uint8_t *)"Stage:", 8);
  OLED_ShowString(0, 4, (uint8_t *)"AssiFlag:", 8);
  OLED_ShowString(0, 6, (uint8_t *)"SenNumber:", 8);

  ADC_Angle_Init();

  Host_Receive_Start();                          // 开启后台 DMA 接收（只需一次）

  while (1)
  {
    trackSensorUpdate();

    sprintf((char *)oled_buffer, "%d", stageFlag);
    OLED_ShowString(7 * 8, 2, oled_buffer, 16);

    sprintf((char *)oled_buffer, "%d", assignmentFlag);
    OLED_ShowString(9 * 8, 4, oled_buffer, 16);

    sprintf((char *)oled_buffer, "%d", TrkI2C_IrSensorNumber);
    OLED_ShowString(9 * 8, 6, oled_buffer, 16);

    // trackSensorOledShow();
    assignment_function[assignmentFlag]();
    //   DistanceControl(650, 1);


    /* 非阻塞检查上位机数据；收到有效帧才刷新 OLED (行首 "Rx:" 之后) */
    // if (Host_Receive_Process())
    // {
    //   sprintf((char *)oled_buffer, "%c %d %c    ",
    //           g_Host_Var1, g_Host_Var2, g_Host_Var3);
    //   OLED_ShowString(3 * 8, 0, oled_buffer, 8);
    // }
    // mspm0_delay_ms(100);
    // Left_Control(1, 550);
    // Right_Control(1, 550);
  }
}
