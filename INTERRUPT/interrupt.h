#ifndef __INTERRUPT_
#define __INTERRUPT_
#include "stdint.h"
#include <stdbool.h>

void GROUP1_IRQHandler(void);
int TimeCount(uint32_t duration, uint32_t turn_start_time);

extern int Current_Speed_Left ;
extern int Current_Speed_Right ;
// 在 interrupt.h 中
extern volatile int32_t Last_Left_count;
extern volatile int32_t Last_Right_count;

// 假设这是你的系统滴答时钟获取函数
extern uint32_t GetTickMs(void); 

typedef struct {
    uint32_t start_time;
    bool is_running;
} SoftTimer_t;

// 非阻塞延时核心函数
bool NonBlockDelay(SoftTimer_t *timer, uint32_t duration);
#endif
