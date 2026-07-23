#include "assignment.h"
#include "Motor.h"
#include "ai_sending.h"
#include "bluetooth.h"
#include "global.h"
#include "hostcom.h"
#include "interrupt.h"
#include "main.h"
#include "move.h"
#include "pid.h"
#include "servo.h"
#include "speed.h"
#include "stdio.h"
#include "stdlib.h"
#include "ti_msp_dl_config.h"
#include "trackingiic.h"
#include "turn.h"

// github test13241234
static SoftTimer_t Assitimer = {0, false};
uint8_t LeftTurnFlag = 0;
uint8_t RightTurnFlag = 0;
uint8_t TurnOverFlag = 0;

// 定义距离默认值
float l1 = 9.9;
float l2 = 3.3;

uint8_t assignmentFlag = 0;
uint8_t Number_of_circle = 0;
uint8_t shapeFlag = 0; // 0:圆形, 1:正方形, 2:三角形, 3:椭圆形
const char *shapeStrings[2] = {
    "head",
    "tail",
};

void AssignmentChoose(void) {
  OLED_Clear();
  // ----- 第一阶段：选择任务 -----
  while (DL_GPIO_readPins(Assignment_ButtonRight_PORT,
                          Assignment_ButtonRight_PIN)) {
    OLED_ShowString(0, 0, (uint8_t *)"Assignment:", 8);
    sprintf((char *)oled_buffer, "%d", assignmentFlag);
    OLED_ShowString(9 * 8, 0, oled_buffer, 16);

    if (!DL_GPIO_readPins(Assignment_ButtonLeft_PORT,
                          Assignment_ButtonLeft_PIN)) {
      assignmentFlag++;
      assignmentFlag %= 8; // 假设任务编号为 0~5
      mspm0_delay_ms(300); // 延时消抖
    }
  }

  // 等待右键（确认键）完全释放，防止直接穿透到下一个选择界面
  while (!DL_GPIO_readPins(Assignment_ButtonRight_PORT,
                           Assignment_ButtonRight_PIN))
    ;
  mspm0_delay_ms(20); // 释放后的去抖
  OLED_Clear();

  // ----- 第二阶段：选择图形 -----
  while (DL_GPIO_readPins(Assignment_ButtonRight_PORT,
                          Assignment_ButtonRight_PIN)) {
    OLED_ShowString(0, 0, (uint8_t *)"Initial:", 8);
    // 在第二行显示对应的图形英文名称
    OLED_ShowString(0, 2, (uint8_t *)shapeStrings[shapeFlag], 16);

    if (!DL_GPIO_readPins(Assignment_ButtonLeft_PORT,
                          Assignment_ButtonLeft_PIN)) {
      shapeFlag++;
      shapeFlag %= 2;      // 共有4种图形
      mspm0_delay_ms(300); // 延时消抖
    }
  }

  // 等待右键再次释放，确保安全退出设置界面
  while (!DL_GPIO_readPins(Assignment_ButtonRight_PORT,
                           Assignment_ButtonRight_PIN))
    ;

  OLED_Clear();
  LightAndSound();
  /************告知云台当前执行哪一个任务*********** */
  Host_Send((char)('a' + assignmentFlag - 1), "000000", '0');
  return;
}

// 循迹
int stageFlag = 0;
static uint8_t task1_yaw_locked = 0;
static float task1_base_yaw = 0.0f;

static float Task1TargetYaw(float offset) {
  float target = task1_base_yaw + offset;
  RangeLimite(&target, 180.0f);
  return target;
}

void TaskYawInitialization(void) {
  if (!task1_yaw_locked) {
    task1_base_yaw = wit_data.yaw;
    task1_yaw_locked = 1;
  }
}

void assignment1(void) {

  TaskYawInitialization();

  switch (stageFlag) {
  case 0:
    DistanceControlWithYaw(850, 1, Task1TargetYaw(0.0f), 100, false);
    if (encoderFlag == 1) {
      stageFlag = 1;
      stop();
    }
    break;

  case 1:
    Right_Turn(90);
    if (turnCompleted == 1)
      stageFlag++;
    break;

  case 2:
    DistanceControlWithYaw(140, 1, Task1TargetYaw(-90.0f), 100, false);
    if (encoderFlag == 2) {
      stageFlag++;
      stop();
    }
    break;

  case 3:
    Right_Turn(90);
    if (turnCompleted == 2)
      stageFlag++;
    break;

  case 4:
    DistanceControlWithYaw(380, 1, Task1TargetYaw(-180.0f), 100, false);
    if (encoderFlag == 3) {
      LightAndSound();
      Host_Send('0', "000000", '0');
      stageFlag++;
      stop();
      mspm0_delay_ms(10000);
    }
    break;

  case 5:
    DistanceControlWithYaw(570, 1, Task1TargetYaw(-180.0f), 100, false);
    if (encoderFlag == 4) {
      stageFlag++;
      stop();
    }
    break;

  case 6:
    Right_Turn(90);
    if (turnCompleted == 3)
      stageFlag++;
    break;

  case 7:
    DistanceControlWithYaw(650, 1, Task1TargetYaw(90.0f), 100, false);
    if (encoderFlag == 5) {
      stageFlag++;
      stop();
    }
    break;

  case 8:
    Right_Turn(90);
    if (turnCompleted == 4)
      stageFlag++;

    break;

  case 9:
    DistanceControlWithYaw(230, 0, Task1TargetYaw(-180.0f), 100, false);
    if (encoderFlag == 6) {
      LightAndSound();
      Host_Send('1', "000000", '0');
      stageFlag++;
      stop();
    }
    break;
  case 10:
    DistanceControlWithYaw(830, 1, Task1TargetYaw(0.0f), 100, false);
    if (encoderFlag == 7) {
      stageFlag++;
      stop();
    }
    break;

  default:
    break;
  }
}

void assignment2(void) {

  TaskYawInitialization();

  switch (stageFlag) {
  case 0:
    DistanceControlWithYaw(850, 1, Task1TargetYaw(0.0f), 100, false);
    if (encoderFlag == 1) {
      stageFlag = 1;
      stop();
    }
    break;

  case 1:
    Right_Turn(90);
    if (turnCompleted == 1)
      stageFlag++;
    break;

  case 2:
    DistanceControlWithYaw(140, 1, Task1TargetYaw(-90.0f), 100, false);
    if (encoderFlag == 2) {
      stageFlag++;
      stop();
    }
    break;

  case 3:
    Right_Turn(90);
    if (turnCompleted == 2)
      stageFlag++;
    break;

  case 4:
    DistanceControlWithYaw(380, 1, Task1TargetYaw(-180.0f), 100, false);
    if (encoderFlag == 3) {
      LightAndSound();
      Host_Send('0', "000000", '0');
      stageFlag++;
      stop();
      mspm0_delay_ms(10000);
    }
    break;

  case 5:
    DistanceControlWithYaw(570, 1, Task1TargetYaw(-180.0f), 100, false);
    if (encoderFlag == 4) {
      stageFlag++;
      stop();
    }
    break;

  case 6:
    Right_Turn(90);
    if (turnCompleted == 3)
      stageFlag++;
    break;

  case 7:
    DistanceControlWithYaw(650, 1, Task1TargetYaw(90.0f), 100, false);
    if (encoderFlag == 5) {
      stageFlag++;
      stop();
    }
    break;

  case 8:
    Right_Turn(90);
    if (turnCompleted == 4)
      stageFlag++;

    break;

  case 9:
    DistanceControlWithYaw(230, 0, Task1TargetYaw(-180.0f), 100, false);
    if (encoderFlag == 6) {
      LightAndSound();
      Host_Send('1', "000000", '0');
      stageFlag++;
      stop();
    }
    break;
  case 10:
    DistanceControlWithYaw(830, 1, Task1TargetYaw(0.0f), 100, false);
    if (encoderFlag == 7) {
      stageFlag++;
      stop();
    }
    break;

  default:
    break;
  }
}

void assignment3(void) {
  TaskYawInitialization();

  switch (stageFlag) {
  case 0:
    if (turnCompleted == 0) {
      Left_Turn(90);
    }
    if (turnCompleted == 1)
      stageFlag++;
    break;

  case 1:
    DistanceControlWithYaw(560, 1, Task1TargetYaw(90.0f), 100, false);
    if (encoderFlag == 1) {
      stageFlag = 2;
      stop();
    }
    break;

  case 2:
    Right_Turn(90);
    if (turnCompleted == 2)
      stageFlag++;
    break;

  case 3:
    DistanceControlWithYaw(200, 0, Task1TargetYaw(0.0f), 100, false);
    if (encoderFlag == 2) {
      LightAndSound();
      Host_Send('0', "000000", '0');
      stageFlag++;
      stop();
    }
    break;

  case 4:
    DistanceControlWithYaw(980, 1, Task1TargetYaw(0.0f), 100, false);
    if (encoderFlag == 3) {
      stop();
      LightAndSound();
      Host_Send('1', "000000", '0');
      stageFlag++;
    }
    break;

  case 5:
    Right_Turn(90);
    if (turnCompleted == 3) {
      stageFlag++;
      stop();
    }
    break;

  case 6:
    DistanceControlWithYaw(760, 1, Task1TargetYaw(-90.0f), 100, false);
    if (encoderFlag == 4) {
      stageFlag++;
      stop();
    }
    break;

  case 7:
    Right_Turn(90);
    if (turnCompleted == 4)
      stageFlag++;
    break;

  case 8:
    DistanceControlWithYaw(280, 1, Task1TargetYaw(-180.0f), 100, false);
    if (encoderFlag == 5) {
      stop();
      LightAndSound();
      Host_Send('2', "000000", '0');
      stageFlag++;
    }
    break;

  default:
    break;
  }
}
void assignment4(void) {
  TaskYawInitialization();

  switch (stageFlag) {
  case 0:
    if (turnCompleted == 0) {
      Left_Turn(90);
    }
    if (turnCompleted == 1)
      stageFlag++;
    break;

  case 1:
    DistanceControlWithYaw(560, 1, Task1TargetYaw(90.0f), 100, false);
    if (encoderFlag == 1) {
      stageFlag = 2;
      stop();
    }
    break;

  case 2:
    Right_Turn(90);
    if (turnCompleted == 2)
      stageFlag++;
    break;

  case 3:
    DistanceControlWithYaw(200, 0, Task1TargetYaw(0.0f), 100, false);
    if (encoderFlag == 2) {
      stop();
      LightAndSound();
      Host_Send('0', "000000", '0');
      stageFlag++;
    }
    break;

  case 4:
    DistanceControlWithYaw(980, 1, Task1TargetYaw(0.0f), 100, false);
    if (encoderFlag == 3) {
      stop();
      LightAndSound();
      Host_Send('1', "000000", '0');
      stageFlag++;
    }
    break;

  case 5:
    Right_Turn(90);
    if (turnCompleted == 3)
      stageFlag++;
    break;

  case 6:
    DistanceControlWithYaw(760, 1, Task1TargetYaw(-90.0f), 100, false);
    if (encoderFlag == 4) {
      stageFlag++;
      stop();
    }
    break;

  case 7:
    Right_Turn(90);
    if (turnCompleted == 4)
      stageFlag++;
    break;

  case 8:
    DistanceControlWithYaw(280, 1, Task1TargetYaw(-180.0f), 100, false);
    if (encoderFlag == 5) {
      stop();
      Host_Send('2', "000000", '0');
      stageFlag++;

      LightAndSound();
    }
    break;

  default:
    break;
  }
}

void assignment5(void) {
  // case 0 和 case 2 分别使用独立计数器，避免两个直线阶段互相影响。
  // 计数器跨多次主循环保存；连续检测到灰线达到阈值后才允许切换状态。
  static uint8_t case0_line_detect_count = 0U;
  static uint8_t case2_line_detect_count = 0U;
  static const uint8_t line_detect_confirm_count = 4U;

  TaskYawInitialization();

  switch (stageFlag) {
  case 0:
    TargettedDirectionWithYaw(1,Task1TargetYaw(0.0f),500);

    if (IrSensorNumber != 0U) {
      // 本次检测到灰线，连续有效次数加一；达到上限后保持，防止溢出。
      if (case0_line_detect_count < line_detect_confirm_count) {
        case0_line_detect_count++;
      }

      if (case0_line_detect_count >= line_detect_confirm_count) {
        // 连续多次检测到灰线后才确认到达，避免单次抖动造成任务误切换。
        case0_line_detect_count = 0U;
        stageFlag = 1;
        stop();
      }
    } else {
      // 中间任意一次未检测到灰线，都视为抖动或尚未到达，重新开始计数。
      case0_line_detect_count = 0U;
    }
    break;

  case 1:
    Tracking();
    break;

  case 2:
    TargettedDirectionWithYaw(1,Task1TargetYaw(-180.0f),500);

    if (IrSensorNumber != 0U) {
      // 本次检测到灰线，连续有效次数加一；达到上限后保持，防止溢出。
      if (case2_line_detect_count < line_detect_confirm_count) {
        case2_line_detect_count++;
      }

      if (case2_line_detect_count >= line_detect_confirm_count) {
        // 连续多次检测到灰线后才确认到达，避免单次抖动造成任务误切换。
        case2_line_detect_count = 0U;
        stageFlag = 3;
        stop();
      }
    } else {
      // 中间任意一次未检测到灰线，都视为抖动或尚未到达，重新开始计数。
      case2_line_detect_count = 0U;
    }
    break;

  case 3:
    Tracking();
    break;

  case 4:
   stageFlag=0;
    break;
    return;

  default:
    break;
  }
}

void assignment6(void) {
Tracking();
}
void assignment7(void) {
  Left_Control(1, 600);
  Right_Control(1, 600);
}

// 如果一直没有任务就空转
void assignment0(void) {
  LightAndSound();
  trackSensorUpdate();
}

void LightAndSound(void) {
  DL_GPIO_setPins(BEE_PORT, BEE_Bee_Port_PIN);

  DL_GPIO_togglePins(LED_PORT, LED_USER_LED_PIN);
  delay_ms(20);
  DL_GPIO_togglePins(LED_PORT, LED_USER_LED_PIN);
  delay_ms(20);
  DL_GPIO_togglePins(LED_PORT, LED_USER_LED_PIN);
  delay_ms(20);
  DL_GPIO_togglePins(LED_PORT, LED_USER_LED_PIN);
  delay_ms(20);

  DL_GPIO_clearPins(BEE_PORT, BEE_Bee_Port_PIN);
}

void zeroparameter(void) {
  CrossingFlag = 0;
  LeftTurnFlag = 0;
  RightTurnFlag = 0;
  turnCompleted = 0;
  assignmentFlag = 0;
  TurnOverFlag = 0;

  task1_yaw_locked = 0;
  task1_base_yaw = 0.0f;
}
