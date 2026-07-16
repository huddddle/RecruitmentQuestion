#include "Motor.h"
#include "global.h"
#include "interrupt.h"
#include "move.h"
#include "main.h"
#include "pid.h"
#include "servo.h"
#include "speed.h"
#include "stdio.h"
#include "stdlib.h"
#include "ti_msp_dl_config.h"
#include "turn.h"
#include "assignment.h"
#include "move.h"
#include "bluetooth.h"
#include "trackingiic.h"
#include "move.h"
#include "ai_sending.h"
#include "hostcom.h"

//github test13241234
static SoftTimer_t Assitimer = {0, false};
uint8_t LeftTurnFlag=0;
uint8_t RightTurnFlag=0;
uint8_t TurnOverFlag=0;

//定义距离默认值
float l1 = 9.9;
float l2 = 3.3;

uint8_t assignmentFlag = 0;
uint8_t Number_of_circle = 0;
uint8_t shapeFlag = 0; // 0:圆形, 1:正方形, 2:三角形, 3:椭圆形
const char *shapeStrings[2] = {
    "head", 
    "tail", 
};


void AssignmentChoose(void) 
{
  OLED_Clear();
  Bluetooth_SendString("Please choose assignment\r\n", 50);
  // ----- 第一阶段：选择任务 -----
  while (DL_GPIO_readPins(Assignment_ButtonRight_PORT, Assignment_ButtonRight_PIN)) 
  {
    OLED_ShowString(0, 0, (uint8_t *)"Assignment:", 8);
    sprintf((char *)oled_buffer, "%d", assignmentFlag);
    OLED_ShowString(9 * 8, 0, oled_buffer, 16);

    if (!DL_GPIO_readPins(Assignment_ButtonLeft_PORT, Assignment_ButtonLeft_PIN)) 
    {
      assignmentFlag++;
      assignmentFlag %= 6; // 假设任务编号为 0~5
      mspm0_delay_ms(300); // 延时消抖
    }
  }

  // 等待右键（确认键）完全释放，防止直接穿透到下一个选择界面
  while (!DL_GPIO_readPins(Assignment_ButtonRight_PORT, Assignment_ButtonRight_PIN));
  mspm0_delay_ms(20); // 释放后的去抖
  OLED_Clear();

  // ----- 第二阶段：选择图形 -----
  while (DL_GPIO_readPins(Assignment_ButtonRight_PORT, Assignment_ButtonRight_PIN)) 
  {
    OLED_ShowString(0, 0, (uint8_t *)"Initial:", 8);
    // 在第二行显示对应的图形英文名称
    OLED_ShowString(0, 2, (uint8_t *)shapeStrings[shapeFlag], 16); 

    if (!DL_GPIO_readPins(Assignment_ButtonLeft_PORT, Assignment_ButtonLeft_PIN)) 
    {
      shapeFlag++;
      shapeFlag %= 2; // 共有4种图形
      mspm0_delay_ms(300); // 延时消抖
    }
  }

  // 等待右键再次释放，确保安全退出设置界面
  while (!DL_GPIO_readPins(Assignment_ButtonRight_PORT, Assignment_ButtonRight_PIN));

  OLED_Clear();
  LightAndSound();
  return;
}

//循迹
int stageFlag = 0;
static uint8_t task1_yaw_locked = 0;
static float task1_base_yaw = 0.0f;

static float Task1TargetYaw(float offset)
{
    float target = task1_base_yaw + offset;
    RangeLimite(&target, 180.0f);
    return target;
}

void assignment1(void)
{
  if (!task1_yaw_locked)
  {
    task1_base_yaw = wit_data.yaw;
    task1_yaw_locked = 1;
  }

  if (stageFlag == 0 && turnCompleted == 0)
  {
    Left_Turn(90);
    if (turnCompleted == 1)stageFlag++;
  }
  else if (stageFlag == 1) 
  {
    DistanceControlWithYaw(530, 1, Task1TargetYaw(90.0f));
    if(encoderFlag == 1)
    { 
      stageFlag = 2; 
      stop();
    } 
  }
  else if (stageFlag == 2) 
  {
    Right_Turn(90);
    if (turnCompleted == 2){stageFlag++;}
  }
  else if (stageFlag == 3) 
  {
    DistanceControlWithYaw(200, 0, Task1TargetYaw(0.0f));
    if(encoderFlag == 2)
    { 
      Host_Send('0', "+12000", '6');
      stageFlag++; 
      stop();
    } 
  }
  else if (stageFlag == 4) 
  {
    DistanceControlWithYaw(950, 1, Task1TargetYaw(0.0f));
    if(encoderFlag == 3)
    { 
      Host_Send('1', "+12000", '6');
      stageFlag++; 
      stop();
    } 
  }
  else if (stageFlag == 5) 
  {
    Right_Turn(90);
    if (turnCompleted == 3)stageFlag++;
  }
   else if (stageFlag == 6) 
  {
    DistanceControlWithYaw(700, 1, Task1TargetYaw(-90.0f));
    if (encoderFlag == 4) {
      stageFlag++;
      stop();
    }
  }
   else if (stageFlag == 7) 
  {
    Right_Turn(90);
    if (turnCompleted == 4)stageFlag++;
  }
  else if(stageFlag==8)
  {
    DistanceControlWithYaw(300, 1, Task1TargetYaw(-180.0f));
    if(encoderFlag == 5)
    {
      Host_Send('1', "+12000", '6');
      stageFlag++; 
      stop();
    } 
  }
  else
  {
  ;
  }
}

void assignment2(void) 
{

}

void assignment3(void)
{

}
void assignment4(void) 
{

}

void assignment5(void) 
{

}

// 如果一直没有任务就空转
void assignment0(void) {

    trackSensorUpdate();
}

void LightAndSound(void)
{
  DL_GPIO_setPins(BEE_PORT, BEE_Bee_Port_PIN );

  DL_GPIO_togglePins(LED_PORT,  LED_USER_LED_PIN);
  delay_ms(200);
  DL_GPIO_togglePins(LED_PORT,  LED_USER_LED_PIN);
  delay_ms(200);
  DL_GPIO_togglePins(LED_PORT,  LED_USER_LED_PIN);
  delay_ms(200);
  DL_GPIO_togglePins(LED_PORT,  LED_USER_LED_PIN);
  delay_ms(200);
  
  DL_GPIO_clearPins(BEE_PORT, BEE_Bee_Port_PIN );
}

void zeroparameter(void)
{
    CrossingFlag = 0;
    LeftTurnFlag = 0;
    RightTurnFlag = 0;
    turnCompleted = 0;
    assignmentFlag = 0;
    TurnOverFlag = 0;

    task1_yaw_locked = 0;
    task1_base_yaw = 0.0f;
}