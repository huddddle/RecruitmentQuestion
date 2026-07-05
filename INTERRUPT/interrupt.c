#include"interrupt.h"
#include"speed.h"
#include"pid.h"
#include"turn.h"
#include"Motor.h"
#include "global.h"
#include "string.h"
#include "stdlib.h"
#include "global.h"
#include "move.h"



// GPIO外设组中断函数，读取编码器的值
void GROUP1_IRQHandler(void) {
  uint32_t gpioLeft = DL_GPIO_getEnabledInterruptStatus(
      Encoder_PORT, Encoder_Left_A_PIN); // 读取左轮中断状态
  uint32_t gpioRight = DL_GPIO_getEnabledInterruptStatus(
      Encoder_PORT, Encoder_Right_A_PIN); // 读取右轮中断状态

  if (gpioLeft) {
    // 读取左轮B通道的电平状态
    if (DL_GPIO_readPins(Encoder_PORT, Encoder_Left_B_PIN)) {
      Left_count++;
    } else {
      Left_count--;
    }
    DL_GPIO_clearInterruptStatus(Encoder_PORT, Encoder_Left_A_PIN);
  }

  if (gpioRight) {
    // 读取右轮B通道的电平状态，为了编码器数值同一，右轮编码器的操作方式与左轮相反
    if (DL_GPIO_readPins(Encoder_PORT, Encoder_Right_B_PIN)) {
      Right_count--;
    } else {
      Right_count++;
    }
    DL_GPIO_clearInterruptStatus(Encoder_PORT, Encoder_Right_A_PIN);
  }
}

// 定时器中断服务函数
void TIMER_0_INST_IRQHandler(void) {
  // 进入中断之后判断触发了哪一个终端
  switch (DL_TimerA_getPendingInterrupt(TIMER_0_INST)) {
  // ZERO EVENT如果是因为0而触发的中端
  case DL_TIMERG_IIDX_ZERO: {
    SpeedRead();
  } break;
  default:
    break;
  }
}

//时间戳函数
int TimeCount(uint32_t duration,uint32_t turn_start_time)
{

    if ((tick_ms - turn_start_time) < duration)  // 延迟duration毫秒
    {
        // 还没到时间，继续等待
        return 0;  // 继续返回 0
    }
    else
    {
        return 1;
    }
}

bool NonBlockDelay(SoftTimer_t *timer, uint32_t duration)
{
    uint32_t current_time =tick_ms; // 获取当前系统毫秒数

    // 如果定时器还没启动，则记录当前时间并标记为运行中
    if (!timer->is_running) {
        timer->start_time = current_time;
        timer->is_running = true;
        return false; // 还没到时间
    }

    // 检查是否到达指定时间
    if ((current_time - timer->start_time) >= duration) {
        timer->is_running = false; // 延时完成，复位状态，方便下次重新触发
        return true;               // 返回 1，表示时间到了
    }

    return false; // 还没到时间
}

// //定时器控制转向完成后不在转向
// void TIMER_Irtracking_INST_IRQHandler(void)
// {
//   EnableTurnFlag = 1;
//   DL_TimerA_stopCounter(TIMER_Irtracking_INST);
//   NVIC_DisableIRQ(TIMER_Irtracking_INST_INT_IRQN);
// }

