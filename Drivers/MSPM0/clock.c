#include "ti_msp_dl_config.h"
#include "clock.h"
#include "global.h"
volatile unsigned long tick_ms;
volatile uint32_t start_time;
unsigned long Tick_Start=0;

int mspm0_delay_ms(unsigned long num_ms)
{
    start_time = tick_ms;
    while (tick_ms - start_time < num_ms);
    return 0;
}

int mspm0_get_clock_ms(unsigned long *count)
{
    if (!count)
        return 1;
    count[0] = tick_ms;
    return 0;
}

void SysTick_Init(void)
{
    DL_SYSTICK_config(CPUCLK_FREQ/1000);
    NVIC_SetPriority(SysTick_IRQn, 0);
}

void IrtrackingTimer(unsigned long num_ms)
{
    if (Tick_Start != 0) 
    {
        if (tick_ms- Tick_Start >= num_ms) 
        {
            Tick_Start = 0;              // 秒表归零，任务完成
            indexx_tracking*=-1;
            return;                      // 立即去执行转弯
        }
    }
    else
    {
        Tick_Start = tick_ms; // 按下秒表！记录当前时刻
    }
}
