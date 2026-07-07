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
#include "ti_msp_dl_config.h"
#include "../Drivers/Bluetooth/uart_dma_tx.h"
// #include "../Drivers/ADC_Angle/adc_angle.h"

int32_t encoderLeftCount = 0;
int32_t encoderRightCount = 0;
extern float angle;
uint32_t pending;// 定义静态变量用于记录下降沿时的B相状态和有效标志位
static uint8_t right_B_at_A_fall = 0;
static uint8_t right_A_falling_flag = 0;

static uint8_t left_B_at_A_fall = 0;
static uint8_t left_A_falling_flag = 0;

// GPIO外设组中断函数，读取编码器的值
void GROUP1_IRQHandler(void) {
    
    // 获取GPIOB上使能的中断状态
    pending = DL_GPIO_getEnabledInterruptStatus(GPIOB, Encoder_Left_A_PIN | Encoder_Right_A_PIN);

    // ================= 处理右轮编码器 =================
    if (pending & Encoder_Right_A_PIN) {
        // 读取当前 A 相和 B 相的引脚电平
        uint32_t right_A_state = DL_GPIO_readPins(Encoder_PORT, Encoder_Right_A_PIN);
        uint32_t right_B_state = DL_GPIO_readPins(Encoder_PORT, Encoder_Right_B_PIN);

        if ((right_A_state & Encoder_Right_A_PIN) == 0) { 
            // 1. A相当前为低电平 -> 说明本次中断由【下降沿】触发
            right_B_at_A_fall = (right_B_state & Encoder_Right_B_PIN) ? 1 : 0; // 记录此刻B相状态
            right_A_falling_flag = 1; // 标记已经发生过有效下降沿
        } 
        else { 
            // 2. A相当前为高电平 -> 说明本次中断由【上升沿】触发
            if (right_A_falling_flag) { 
                // 必须是在记录过下降沿的前提下，才进行比对（防止系统刚启动时的误判）
                uint8_t current_B = (right_B_state & Encoder_Right_B_PIN) ? 1 : 0;
                
                // 3. 核心消抖逻辑：比较半个周期内B相是否发生了翻转
                if (current_B != right_B_at_A_fall) { 
                    // B相发生了改变，说明电机确实向前/后转动了，脉冲有效
                    // 保持你原有的加减逻辑对应关系
                    if (current_B) {
                        encoderRightCount++;  
                    } else {
                        encoderRightCount--;
                    }
                }
                // 一次完整的周期判断结束，清除标志位，等待下一次下降沿
                right_A_falling_flag = 0; 
            }
        }
        // 清除右轮中断标志
        DL_GPIO_clearInterruptStatus(GPIOB, Encoder_Right_A_PIN);
    }
    
    // ================= 处理左轮编码器 =================
    if (pending & Encoder_Left_A_PIN) {
        uint32_t left_A_state = DL_GPIO_readPins(Encoder_PORT, Encoder_Left_A_PIN);
        uint32_t left_B_state = DL_GPIO_readPins(Encoder_PORT, Encoder_Left_B_PIN);

        if ((left_A_state & Encoder_Left_A_PIN) == 0) { 
            // 1. A相下降沿
            left_B_at_A_fall = (left_B_state & Encoder_Left_B_PIN) ? 1 : 0;
            left_A_falling_flag = 1; 
        } 
        else { 
            // 2. A相上升沿
            if (left_A_falling_flag) { 
                uint8_t current_B = (left_B_state & Encoder_Left_B_PIN) ? 1 : 0;
                
                // 3. 核心比对
                if (current_B != left_B_at_A_fall) { 
                    // 保持你原有的左轮加减逻辑对应关系
                    if (current_B) {
                        encoderLeftCount--;
                    } else {
                        encoderLeftCount++;
                    }
                }
                left_A_falling_flag = 0; 
            }
        }
        // 清除左轮中断标志
        DL_GPIO_clearInterruptStatus(GPIOB, Encoder_Left_A_PIN);
    }
}


int Current_Speed_Left = 0;
int Current_Speed_Right = 0;
volatile int32_t Last_Left_count;
volatile int32_t Last_Right_count;
volatile uint8_t speedControlTick = 0;
// 定时器中断服务函数
void TIMER_Encoder_INST_IRQHandler(void) {
  // 进入中断之后判断触发了哪一个终端
  switch (DL_TimerA_getPendingInterrupt(TIMER_Encoder_INST)) {
  // ZERO EVENT如果是因为0而触发的中端
  case DL_TIMERG_IIDX_ZERO: {
    // //读取编码器数值

    int32_t cur_left, cur_right;

    __disable_irq();
    cur_left = encoderLeftCount;
    cur_right = encoderRightCount;
    __enable_irq();
    // 编码器数值映射到速度电流的中间值
    Current_Speed_Left = (cur_left - Last_Left_count);//计算编码器差值就是点击的速度
    Current_Speed_Right = (cur_right - Last_Right_count);


    Last_Left_count = cur_left;
    Last_Right_count = cur_right;
    speedControlTick = 1;
  }
  break;
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

/**
 * @brief UART_BT (UART1) 中断处理函数
 * @note 处理DMA发送相关中断
 */
void UART_BT_INST_IRQHandler(void)
{
    // 调用DMA发送驱动的中断处理函数
    UART_DMA_TX_IRQHandler();
}
