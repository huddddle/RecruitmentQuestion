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
#include "K230.h"
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

uint8_t oled_buffer[64];
volatile uint8_t gEchoData = 0;
void (*assignment_function[8])(void)=
{
  assignment0,      // 0: 没有任务
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

  OLED_Init(); // 屏幕初始化
  WIT_Init();  // 陀螺仪初始化
  // K230_Init(); // 与K230串口通信部分初始化
  // Servo_Init();//云台初始化
  Speed_Init();// 定时器和编码器测速部分
  Ultrasonic_Init();

  // 电机转速控制
  Left_Control(1, 0);
  Right_Control(1, 0);


  //运动圈数初始化
  AssignmentChoose();

  //OLED信息显示
  OLED_ShowString(0, 0, (uint8_t *)"AssiFlag:", 8);
  OLED_ShowString(0, 2, (uint8_t *)"Crossing:", 8);
  OLED_ShowString(0, 4, (uint8_t *)"Yaw:", 8);
  OLED_ShowString(0, 6, (uint8_t *)"Status:", 8);

  while (1) 
  {    
    // 显示 assignmentFlag
    sprintf((char *)oled_buffer, "%d", turnCompleted);
    OLED_ShowString(9 * 8, 0, oled_buffer, 16);
    
    // 显示 CrossingFlag
    sprintf((char *)oled_buffer, "%d", turnFlag);
    OLED_ShowString(9 * 8, 2, oled_buffer, 16);
    
    // 显示 CrossingFlag (格式化版本)
    sprintf((char *)oled_buffer, "%-6.1f", wit_data.yaw);
    OLED_ShowString(9 * 8, 4, oled_buffer, 16);

    // sprintf((char *)oled_buffer, "%-6.4f", wit_data.yaw);
    // OLED_ShowString(9 * 8, 6, oled_buffer, 16);

    assignment_function[assignmentFlag]();


    // //Add
    // Distance=Read_Ultrasonic();
  }
}