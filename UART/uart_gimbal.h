#ifndef __K230_
#define __K230_

void K230_Init(void);
void UART_Camera_INST_IRQHandler(void);
void Process(void);
void GetCameraData(void);

#endif