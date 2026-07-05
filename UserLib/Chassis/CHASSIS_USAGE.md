# Chassis 类库使用指南

## 📋 概述

`Chassis` 是一个面向对象的三轮车底盘控制库，集成了以下子系统：
- **Motor** - 电机控制
- **SpeedController** - 速度闭环控制（PID）
- **TurnController** - 非阻塞式转向控制
- **LineTracker** - 红外循迹系统
- **IMUSensor** - 陀螺仪（9DOF）
- **DisplayManager** - OLED 显示管理

## 🚀 快速开始

### 1. 在 main.c 中初始化

```c
#include "chassis.h"

// 创建底盘对象
Chassis chassis;

int main(void) {
    SYSCFG_DL_init();
    SysTick_Init();
    
    // 初始化底盘（包含所有子系统）
    chassis.Init();
    
    while (1) {
        // 非阻塞式主循环
        chassis.LineWalking();  // 循迹行走
        // 或其他任务...
    }
}
```

### 2. 基本运动控制

```c
// 前进 500 速度
chassis.Forward(500);

// 后退 300 速度
chassis.Backward(300);

// 停止
chassis.Stop();

// 紧急停止
chassis.EmergencyStop();
```

### 3. 转向控制

```c
// 左转 90 度（返回值：0=完成, 1=进行中）
if (chassis.LeftTurn(90) == 0) {
    // 转向完成，执行下一步
}

// 右转 45 度
if (chassis.RightTurn(45) == 0) {
    // 转向完成
}

// 转向到指定偏航角（0-360 度）
chassis.TurnToYaw(180);

// 保持方向直线行走
chassis.Straight(200, 180);  // speed=200, target_yaw=180
```

### 4. 循迹控制

```c
// 启用循迹
chassis.EnableLineTracking(true);

// 在主循环中调用
while (1) {
    chassis.LineWalking();
}

// 禁用循迹
chassis.EnableLineTracking(false);
```

### 5. 差分转向

```c
// 向左转，速度 200
chassis.TurnLeft(200, 45);

// 向右转，速度 200
chassis.TurnRight(200, 45);
```

### 6. 获取状态信息

```c
Chassis::State state = chassis.GetState();

printf("偏航角: %.2f°\n", state.yaw);
printf("左速度: %d\n", state.left_speed);
printf("右速度: %d\n", state.right_speed);
printf("距离: %lu mm\n", state.distance_mm);
printf("红外传感器: %d\n", state.ir_sensors);
printf("循迹误差: %d\n", state.tracking_error);
printf("转向中: %s\n", state.is_turning ? "是" : "否");
printf("循迹中: %s\n", state.is_tracking ? "是" : "否");
```

### 7. OLED 显示

```c
// 获取显示管理器
DisplayManager *display = chassis.GetDisplay();

// 显示字符串
display->ShowString(0, 0, (uint8_t *)"Hello", 8);

// 显示数字
display->ShowNumber(0, 1, 12345, 5, 8);

// 显示浮点数
display->ShowFloat(0, 2, 3.14159, 2);

// 显示完整的底盘状态仪表板
display->ShowDashboard(&chassis);
```

## 🔧 参数配置

### 设置最大速度

```c
chassis.SetMaxSpeed(600);  // 设置最大速度为 600
```

### 设置转向速度

```c
chassis.SetTurnSpeed(150);  // 转向时电机 PWM 值
```

### 设置循迹速度

```c
chassis.SetTrackingSpeed(250);  // 循迹时的基础速度
```

### 设置转向精度

```c
chassis.SetTurnPrecision(3.0f);  // 精度：3 度
```

### 设置 PID 参数

```c
chassis.SetPIDGains(1.0f, 0.01f, 0.5f);  // Kp, Ki, Kd
```

## 📊 子系统详解

### Motor 子类

```c
Motor *motor = chassis.GetMotor();

// 直接控制电机
motor->SetLeftSpeed(Motor::FORWARD, 500);
motor->SetRightSpeed(Motor::FORWARD, 500);

// 差分转向
motor->DiffTurn(300, 500);

// 获取当前速度
int left = motor->GetLeftSpeed();
int right = motor->GetRightSpeed();
```

### SpeedController 子类

```c
SpeedController *speed = chassis.GetSpeedController();

// 设置目标速度
speed->SetTargetSpeed(250, 250);

// 或设置统一速度
speed->SetUniformSpeed(300);

// 获取实际速度
int actual_left = speed->GetLeftActualSpeed();
int actual_right = speed->GetRightActualSpeed();
```

### TurnController 子类

```c
TurnController *turn = chassis.GetTurnController();

// 获取转向状态
bool is_turning = turn->IsTurning();
float target = turn->GetTargetYaw();
float current = turn->GetCurrentYaw();

// 设置参数
turn->SetPrecision(2.0f);        // 精度 2 度
turn->SetInitDelay(800);         // 初始化延迟 800ms
turn->SetStopDelay(1200);        // 停止延迟 1200ms
turn->SetTurnSpeed(180);         // 转向 PWM
```

### LineTracker 子类

```c
LineTracker *tracker = chassis.GetLineTracker();

// 读取传感器
tracker->ReadSensors();

// 获取激活传感器数量
uint8_t count = tracker->GetActiveSensorCount();

// 获取传感器掩码
uint8_t mask = tracker->GetSensorMask();

// 检查路口
if (tracker->IsAtIntersection()) {
    // 在十字路口
    tracker->TurnAtIntersection(1);  // 1=左转, -1=右转
}

// 检查是否检测到黑线
if (tracker->IsLineDetected()) {
    // 检测到黑线
}
```

### IMUSensor 子类

```c
IMUSensor *imu = chassis.GetIMU();

// 获取欧拉角
float yaw = imu->GetYaw();
float pitch = imu->GetPitch();
float roll = imu->GetRoll();

// 获取加速度
int16_t ax = imu->GetAccX();
int16_t ay = imu->GetAccY();
int16_t az = imu->GetAccZ();

// 获取角速度
int16_t gx = imu->GetGyroX();
int16_t gy = imu->GetGyroY();
int16_t gz = imu->GetGyroZ();

// 更新数据（需要定期调用）
imu->Update();

// 角度限制工具
float limited = IMUSensor::LimitAngle360(450);  // 返回 90
```

## ⚙️ 工作流程示例

### 例 1：简单前进后转向

```c
enum State { MOVING_FORWARD, TURNING_LEFT, DONE };
State current_state = MOVING_FORWARD;

while (1) {
    switch (current_state) {
        case MOVING_FORWARD:
            chassis.Forward(300);
            // 当检测到某个条件时切换状态
            if (chassis.GetState().distance_mm < 50) {
                current_state = TURNING_LEFT;
            }
            break;
            
        case TURNING_LEFT:
            if (chassis.LeftTurn(90) == 0) {  // 非阻塞！
                current_state = DONE;
            }
            break;
            
        case DONE:
            chassis.Stop();
            break;
    }
    
    // 显示状态
    chassis.GetDisplay()->ShowDashboard(&chassis);
}
```

### 例 2：循迹并在路口转向

```c
while (1) {
    // 主循环中持续调用循迹函数
    chassis.LineWalking();
    
    // 获取当前状态
    Chassis::State state = chassis.GetState();
    
    // 显示信息
    chassis.GetDisplay()->ShowChassisStatus(
        state.yaw, 
        state.left_speed, 
        state.right_speed, 
        state.distance_mm
    );
    
    // 路口检测并转向由 LineTracker 内部处理
}
```

### 例 3：多任务调度

```c
enum TaskID { TASK_FORWARD = 0, TASK_TURN = 1, TASK_TRACK = 2 };
TaskID current_task = TASK_FORWARD;

while (1) {
    Chassis::State state = chassis.GetState();
    
    switch (current_task) {
        case TASK_FORWARD:
            chassis.Forward(200);
            // 任务完成条件检查
            if (state.distance_mm > 500) {
                current_task = TASK_TURN;
            }
            break;
            
        case TASK_TURN:
            if (chassis.LeftTurn(45) == 0) {
                current_task = TASK_TRACK;
            }
            break;
            
        case TASK_TRACK:
            chassis.LineWalking();
            // 循迹持续进行
            break;
    }
    
    // 所有操作都是非阻塞的，可以同时处理多个任务
}
```

## 🎯 特性

### ✅ 优势

1. **非阻塞式设计** - 所有操作不会冻结主循环
2. **封装完整** - 集成了所有硬件模块
3. **易于扩展** - 子类设计，便于添加新功能
4. **实时反馈** - 随时查询系统状态
5. **PID 控制** - 自动的速度和转向控制
6. **陀螺仪集成** - 精确的方向控制

### ⚠️ 注意事项

1. 所有转向操作返回值需要检查：
   - `0` = 转向完成
   - `1` = 转向进行中

2. 主循环需要持续调用来维护状态：
   ```c
   while (1) {
       // 持续调用，不能长时间阻塞
       chassis.GetState();
       // 其他任务...
   }
   ```

3. 转向/循迹是状态机，需要多次调用才能完成

4. 如果使用循迹，请不要同时调用其他转向功能

## 📝 更新日志

### V2.0.0 (2026-04-29)

- ✨ 新增 Motor、SpeedController、TurnController 等子类
- ✨ 新增 LineTracker 循迹子系统
- ✨ 新增 IMUSensor 陀螺仪整合
- ✨ 新增 DisplayManager OLED 显示管理
- 🔄 改进转向控制为非阻塞式（使用时间戳）
- 🔄 完整实现所有功能方法

## 🐛 已知问题

- 距离控制功能 (`DriveDistance`) 需要编码器集成改进
- 暂不支持轨迹记录和回放

---

**开发者**: Landon Yang  
**最后更新**: 2026-04-29
