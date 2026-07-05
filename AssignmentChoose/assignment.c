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
const char *shapeStrings[4] = {
    "Triangle", 
    "Square  ", 
    "Circle  ", 
    "Ellipse "
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
    OLED_ShowString(0, 0, (uint8_t *)"Shape:", 8);
    // 在第二行显示对应的图形英文名称
    OLED_ShowString(0, 2, (uint8_t *)shapeStrings[shapeFlag], 16); 

    if (!DL_GPIO_readPins(Assignment_ButtonLeft_PORT, Assignment_ButtonLeft_PIN)) 
    {
      shapeFlag++;
      shapeFlag %= 4; // 共有4种图形
      mspm0_delay_ms(300); // 延时消抖
    }
  }

  // 等待右键再次释放，确保安全退出设置界面
  while (!DL_GPIO_readPins(Assignment_ButtonRight_PORT, Assignment_ButtonRight_PIN));

  OLED_Clear();
  LightAndSound();
  return;
}

void assignment1(void) 
{
  if (turnFlag > 4 || turnFlag<0 ) 
  {
    zeroparameter();
    backwords();
    mspm0_delay_ms(500);
    stop();
    LightAndSound();
    return;
  }
  if (LeftTurnFlag==0&&RightTurnFlag==0&&TurnOverFlag==0)
  {
    Tracking();
  }
  else if(LeftTurnFlag==1)
  {
    Left_Turn(88);
  }
  else if(RightTurnFlag==1)
  {
    Right_Turn(88);
  }
  else if(TurnOverFlag==1)
  {
    Left_Turn(181);
  }
}

void assignment2(void) {

  if (turnFlag > 6 || turnFlag < 0) {
    zeroparameter();
    backwords();
    mspm0_delay_ms(500);
    stop();
    LightAndSound();
    return;
  }

  if (LeftTurnFlag == 0 && RightTurnFlag == 0 && TurnOverFlag == 0) {
    Tracking();
  } else {
    if (turnCompleted == 3 && turnFlag == 4) {
      Left_Turn(179);
    } else if (LeftTurnFlag == 1) {
      Left_Turn(89);
    } else if (RightTurnFlag == 1) {
      Right_Turn(87);
    }
  }
}

void assignment3(void) 
{
    if (turnFlag > 4 || turnFlag<0 ) 
  {
    zeroparameter();
    backwords();
    mspm0_delay_ms(500);
    stop();
    LightAndSound();
    Bluetooth_SendData(l1, l2);
    return;
  }
  if (LeftTurnFlag==0&&RightTurnFlag==0&&TurnOverFlag==0)
  {
    Tracking();
  }
  else if(LeftTurnFlag==1)
  {
    Left_Turn(88);
  }
  else if(RightTurnFlag==1)
  {
    Right_Turn(87);
  }
  else if(TurnOverFlag==1)
  {
    Left_Turn(181);
  }
}
void assignment4(void) 
{
  if (turnFlag > 4) 
  {
    stop();
    parking();
    zeroparameter();
    LightAndSound();
    return;
  }
  if (LeftTurnFlag == 0 && RightTurnFlag == 0 && TurnOverFlag == 0) {
    Tracking();
  } else if (LeftTurnFlag == 1) {
    Left_Turn(88.0);
  } else if (RightTurnFlag == 1) {
    Right_Turn(88.0);
  } else if (TurnOverFlag == 1) {
    Left_Turn(181.0);
  }
}

void assignment5(void) 
{

}

// 如果一直没有任务就空转
void assignment0(void) {

  OLED_ShowString(10, 6, (uint8_t *)"Completed", 8);
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
}