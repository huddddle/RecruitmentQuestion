#include"interrupt.h"
#include "uart_gimbal.h"
#include "string.h"
#include "stdlib.h"
#include "global.h"
#include "ti_msp_dl_config.h"
#include "assignment.h"


//与K230通信
char Camera_buffer[20];
volatile char ReceiveData;
uint8_t BufferIndex = 0;
int ReceivingFlag=0;//接收标志
int ProcessFlag=0;//处理标志
int dataCount=0;
int Final_data[20];
int CameraData[20];
int num_of_data=0;


// // K230串口中断服务函数
// void UART_Camera_INST_IRQHandler(void)
// {
//   ReceiveData = DL_UART_Main_receiveData(UART_Camera_INST);
//   if(ReceiveData=='$')
//   {
//     //开始接收数据
//     BufferIndex=0;
//     ReceivingFlag=1;
//   }
//   else if(ReceiveData=='#')
//   {
//     if(ReceivingFlag)
//     {
//       ReceivingFlag=0;
//       Camera_buffer[BufferIndex]='\0';//确保字符串以‘null’结尾
//       //处理接收到的数据
//       ProcessFlag=1;
//       Process();
//     }
//   } else if (ReceivingFlag) // 进行数据接收
//   {
//     if (BufferIndex < sizeof(Camera_buffer) - 1)
//      {
//       Camera_buffer[BufferIndex++] = ReceiveData;
//     }
//   }
// }

// void Process(void) {
//   if (ProcessFlag != 1)return;

//   ProcessFlag = 0;
//   char *token;
//   char tempBuffer[100];
//   strcpy(tempBuffer, Camera_buffer);

//   token = strtok(tempBuffer, ",");
//   dataCount = 0;

//   while (token != NULL && dataCount < 99) 
//   {
//     Final_data[dataCount++] = atoi(token);
//     token = strtok(NULL, ",");
//   }
//     DL_GPIO_togglePins(LED_PORT, LED_USER_LED_PIN);
//     GetCameraData();
// }

// void GetCameraData(void)
// {
//     for (int i = 0; i < 4; i++)
//     {
//         CameraData[i] = Final_data[i];
//     }
//     assignmentFlag=CameraData[0];
// }

//K230初始化
// void K230_Init(void)
// {
//   NVIC_ClearPendingIRQ(UART_Camera_INST_INT_IRQN);
//   NVIC_EnableIRQ(UART_Camera_INST_INT_IRQN);
// }
