#include "move.h"
#include "Motor.h"
#include "global.h"
#include "turn.h"
#include "ti_msp_dl_config.h"
#include "clock.h"
#include "interrupt.h"
#include "assignment.h"


#define turn_threshold 2
#define max_correction 90

int irSpeed=400; // 巡线速度

// 
int32_t pid_output_IRR = 0;
uint8_t x[8] = {0};


int indexx_move = 0;
int EnableCrossingFlag=1;//控制转完之后不在转向
uint16_t IrSensorNumber=0;
SoftTimer_t ModuleTrackingTimer = {0, false};
SoftTimer_t ParkingTimer = {0, false};
extern uint8_t shapeFlag;  // 0:圆形, 1:正方形, 2:三角形, 3:椭圆形
static SoftTimer_t backward_timer = {0, false};   // 后退计时器
static SoftTimer_t forward_timer = {0, false};    // 前进计时器

//底盘功能列表大全，方便进行调试
void Chassis(void) {

  switch (indexx_move) {
    case 1:
      Left_Turn(90);//陀螺仪控制左转90° //转弯状态机，通过CrossingFlag和turnCompleted的状态来 灵活 判断是转弯还是直走
      return;
    case 2:
      Right_Turn(90);//陀螺仪控制右转90°
      return;
    case 3:
      Left_Turn(179);//陀螺仪控制左转180°
      return;
    case 4:
      Right_Turn(179);//陀螺仪控制右转180°
      return;
    case 5:
      indexx_move = Straight(400);//陀螺仪控制走直线（未完成）
      return;
    case 6:
      Tracking();
      return;
    case 7:
      stop();//电机停止
      return;
    case 8:
      indexx_move = trackingLeftTurn();//通过循迹模块判断转弯是否完成     //转弯循迹判断，专门用于防止干扰项目和地图判断的出现
    case 9:
      indexx_move = trackingRightTurn();//通过循迹模块判断转弯是否完成     //转弯循迹判断，专门用于防止干扰项目和地图判断的出现
      return;
    // case 10:
    //   indexx_move = ModuleTracking();//通过循迹模块判断进行转向的循迹
      return;
    case 10:
      backwords(); // 倒车函数，预留给停车功能
      return;
    default:
      return;
  }
}



















//以下是成员函数
int trackingLeftTurn(void)
{
  track_deal_sensor(x);
  if(x[3]==1||x[4]==1)
  {
    turnCompleted++;
    return 10;  
  }
  else
  {
    Left_Control(0, 200);    
    Right_Control(1, 200);   
    return 8;  // 继续转向
  }
}

//以下是成员函数
int trackingRightTurn(void)
{
  track_deal_sensor(x);
  if(x[3]==1||x[4]==1)
  {
    turnCompleted++;
    return 10;  
  }
  else
  {
    Left_Control(1, 200);    
    Right_Control(0, 200);   
    return 9;  // 继续转向
  }
}


static uint32_t crossing_wait_start = 0;
static uint8_t crossing_waiting = 0;
static int crossing_turn_dir = 0;   // 1: 左转, 2: 右转

static void ModuleTracking_RunFollow(void)
{
  int turn_pwm=0;
  int Tracking_Sum = 0;

  if(IrSensorNumber > 0 && IrSensorNumber <= 2) {
    if(x[0]) Tracking_Sum += 60;
    if(x[1]) Tracking_Sum += 40;
    if(x[2]) Tracking_Sum += 20;
    if(x[3]) Tracking_Sum += 10;
    if(x[4]) Tracking_Sum -= 10;
    if(x[5]) Tracking_Sum -= 20;
    if(x[6]) Tracking_Sum -= 40;
    if(x[7]) Tracking_Sum -= 60;
  }

  float Kp = 1.0f;
  float Kd = 0.4f;
  int last_tracking_error = 0;
  int error_change = Tracking_Sum - last_tracking_error;
  int adjustpwm = (int)(Kp * Tracking_Sum + Kd * error_change);
  last_tracking_error = Tracking_Sum;

  Left_Control(1, irSpeed - adjustpwm);
  Right_Control(1, irSpeed + adjustpwm);
}

// int ModuleTracking(void)
// {
//   track_deal_sensor(x);

//   if (crossing_waiting)
//   {
//     // 就像写 delay(1000) 一样，但它是非阻塞的！
//     if (!NonBlockDelay(&ModuleTrackingTimer, 300)) 
//     {
//       ModuleTracking_RunFollow();
//       DL_GPIO_setPins(BEE_PORT, BEE_Bee_Port_PIN );
//       return 9;
//     }
//     DL_GPIO_clearPins(BEE_PORT, BEE_Bee_Port_PIN);
//     track_deal_sensor(x);
//     crossing_waiting = 0;

//     if (IrSensorNumber == 0&& (crossing_turn_dir == 1 || crossing_turn_dir == 2))
//     {
//       if (crossing_turn_dir == 1)
//       {
//         CrossingFlag++;
//         crossing_turn_dir = 0;
//         return 1;
//       }
//       else if (crossing_turn_dir == 2)
//       {
//         CrossingFlag++;
//         crossing_turn_dir = 0;
//         return 2;
//       }
//     }

//     crossing_turn_dir = 0;
//     ModuleTracking_RunFollow();
//     return 9;
//   }

//   if (IrSensorNumber >= 3 && IrSensorNumber <= 5)
//   {
//     int left_score = x[0]*3 + x[1]*2 + x[2]*1;
//     int right_score = x[7]*3 + x[6]*2 + x[5]*1;

//     if (left_score > right_score)
//     {
//       crossing_waiting = 1;
//       crossing_turn_dir = 1;
//       ModuleTracking_RunFollow();
//       return 9;
//     }
//     else if (right_score > left_score)
//     {
//       crossing_waiting = 1;
//       crossing_turn_dir = 2;
//       ModuleTracking_RunFollow();
//       return 9;
//     }
//   }

//   ModuleTracking_RunFollow();
//   return 9;
// }



static SoftTimer_t Trackingtimer = {0, false};    // 前进计时器
// uint8_t LeftturningPoint=0;
// uint8_t RightturningPoint=0;
void Tracking(void) {
  // static int8_t LeftturningPoint=0;//循迹转向防抖
  // static int8_t RightturningPoint=0;//循迹转向防抖

  // 转弯识别
  track_deal_sensor(x);
  if (IrSensorNumber >= 3 && IrSensorNumber <= 6) {
    // 使用加权比较，给最外侧的灯（x[0]和x[7]）更高的权重，确保转向意图清晰
    int left_score = x[0] * 3 + x[1] * 2 + x[2] * 1;
    int right_score = x[7] * 3 + x[6] * 2 + x[5] * 1;

    // left_score>right_score ? LeftturningPoint++,RightturningPoint--
    // :LeftturningPoint--,RightturningPoint++ ;

    if (left_score > right_score) // 疑似左转路口进行判定
    {
      mspm0_delay_ms(65);
      if (track_deal_sensor(x) != 0) // 还有路是说是十字路口
      {
        CrossingFlag++;LeftTurnFlag = 0;RightTurnFlag = 0;
        return;
      } else {
        turnFlag++; LeftTurnFlag = 1;RightTurnFlag = 0;
        return;
      }
    } else if (right_score > left_score) // 疑似右转路口进行判定
    {
      mspm0_delay_ms(75);
      if (track_deal_sensor(x) != 0) // 还有路是说是十字路口
      {
        CrossingFlag++;
        LeftTurnFlag = 0;
        RightTurnFlag = 0;
        return;
      } else {
        turnFlag++;
        LeftTurnFlag = 0;
        RightTurnFlag = 1;
        return;
      }
    }
  }

  int Tracking_Sum = 0;
  // 3. 正常循迹偏差计算
  if(IrSensorNumber > 0 && IrSensorNumber <=2) {
    if(x[0]) Tracking_Sum += 60;
    if(x[1]) Tracking_Sum += 40;
    if(x[2]) Tracking_Sum += 20;
    if(x[3]) Tracking_Sum += 10;
    if(x[4]) Tracking_Sum -= 10;
    if(x[5]) Tracking_Sum -= 20;
    if(x[6]) Tracking_Sum -= 40;
    if(x[7]) Tracking_Sum -= 60;
  }

  float Kp = 1.0f; // 循迹比例
  float Kd = 0.4f; // 循迹微分（抑制震荡）
  // 3. PD 控制器计算
  int last_tracking_error = 0;
  int error_change = Tracking_Sum - last_tracking_error;

  int adjustpwm = (int)(Kp * Tracking_Sum + Kd * error_change);
  last_tracking_error = Tracking_Sum;

  Left_Control(1, irSpeed - adjustpwm);      // ✅ IRR_SPEED → Speed
  Right_Control(1, irSpeed + adjustpwm);     // ✅ IRR_SPEED → Speed
}




// IO口巡线探头的处理,遇黑线，指示灯亮，低电平，输出0
int track_deal_sensor(uint8_t s[]) 
{
  IrSensorNumber=0;
  s[0] = DL_GPIO_readPins(IR_X_X1_PORT, IR_X_X1_PIN) > 0 ? 1 : 0;
  s[1] = DL_GPIO_readPins(IR_X_X2_PORT, IR_X_X2_PIN) > 0 ? 1 : 0;
  s[2] = DL_GPIO_readPins(IR_X_X3_PORT, IR_X_X3_PIN) > 0 ? 1 : 0;
  s[3] = DL_GPIO_readPins(IR_X_X4_PORT, IR_X_X4_PIN) > 0 ? 1 : 0;
  s[4] = DL_GPIO_readPins(IR_X_X5_PORT, IR_X_X5_PIN) > 0 ? 1 : 0;
  s[5] = DL_GPIO_readPins(IR_X_X6_PORT, IR_X_X6_PIN) > 0 ? 1 : 0;
  s[6] = DL_GPIO_readPins(IR_X_X7_PORT, IR_X_X7_PIN) > 0 ? 1 : 0;
  s[7] = DL_GPIO_readPins(IR_X_X8_PORT, IR_X_X8_PIN) > 0 ? 1 : 0;
  for(int i=0;i<8;i++)
  {
    IrSensorNumber+=s[i];
  }
  return IrSensorNumber;
}

// ================== 陀螺仪闭环直线行走函数 ==================
// 静态变量用于保存闭环状态
static float straight_target_yaw = 0;      // 目标偏航角
static int straight_initialized = 0;       // 初始化标志
static float straight_last_yaw_error = 0;  // 上一次的偏航误差

int Straight(float base_speed)
{
  if (!straight_initialized) {
    straight_target_yaw = wit_data.yaw;
    straight_initialized = 1;
    straight_last_yaw_error = 0;
    return 5;  // 初始化完成，继续执行
  }

  float Kp_straight = 8.0f;  // 比例系数（调整转向灵敏度）
  float Kd_straight = 0.80f;   // 微分系数（抑制震荡）
  

  float yaw_error = wit_data.yaw - straight_target_yaw;
  RangeLimite(&yaw_error, 180.0f); // 将误差限制在 -180° 到 180° 范围内，避免过度转向

  // ============ PD 控制器计算 ============
  float yaw_error_change = yaw_error - straight_last_yaw_error;
  int yaw_correction = (int)(Kp_straight * yaw_error + Kd_straight * yaw_error_change);
  straight_last_yaw_error = yaw_error;


  // 防止补偿过度
  if (yaw_correction > max_correction) {
    yaw_correction = max_correction;
  } else if (yaw_correction < -max_correction) {
    yaw_correction = -max_correction;
  }

  // ============ 电机控制逻辑 ============
  int left_speed = (int)base_speed + yaw_correction;
  int right_speed = (int)base_speed - yaw_correction;

  // 确保速度不为负
  if (left_speed < 0) left_speed = 0;
  if (right_speed < 0) right_speed = 0;

  // 执行电机控制：方向 1=前进，速度为计算后的值
  Left_Control(1, left_speed);
  Right_Control(1, right_speed);


  return 5;  // 持续返回5表示直线行走进行中
}

// ============ 重置直线行走状态函数 ============
void Straight_Reset(void)
{
  straight_initialized = 0;
  straight_target_yaw = 0;
  straight_last_yaw_error = 0;
}

int stop(void)
  {
    Left_Control(1, 0);
    Right_Control(1, 0);
    return 0;
  }

// ============ 停车函数（状态机方式） ============
  void parking(void) 
  {
    mspm0_delay_ms(300);
    while (turnCompleted != 5) {
      Left_Turn(178);
    }

    uint32_t time1 = (shapeFlag == 0 || shapeFlag == 1) ? 420 : 1520;
    uint32_t time2 = (shapeFlag == 0 || shapeFlag == 2) ? 850 : 1900;

    Left_Control(1, 300);
    Right_Control(1, 300);
    mspm0_delay_ms(time1);

    while (turnCompleted != 6) {
      Right_Turn(89);
    }

    Left_Control(1, 300);
    Right_Control(1, 300);
    mspm0_delay_ms(time2);
    stop();
  }



void backwords(void)
{
  Left_Control(0, 300);   // 0表示反向，速度300
  Right_Control(0, 300);
}