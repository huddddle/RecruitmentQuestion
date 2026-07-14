# 编码器距离控制不走直线问题排查

## 结论摘要

当前距离控制能让左右轮分别追目标脉冲数，但还没有真正做“直线约束”。如果左右电机、轮径、摩擦、编码器方向或速度反馈存在细微差异，车辆会自然跑偏。下面按优先级列出我在代码中看到的主要风险点和改进建议。

## 主要发现

1. `main.c:104-105` 当前注释掉了任务调度，直接在 `while(1)` 中反复调用 `DistanceControl(650, 1)`，并且没有使用返回值。
   - 影响：到达 650mm 后 `DistanceControl` 会把 `AbsoluateEncoder` 清零；下一轮主循环又会重新初始化并继续跑同一段距离，车辆不会稳定停在目标点。
   - 建议：到达距离后立即 `stop()`，并进入停止状态或下一任务阶段，例如用 `if (DistanceControl(650, 1)) { stop(); stageFlag++; }` 这类状态机写法。

2. `SPEED/speed.c:83-123` 左右轮是两个独立的位置 PID，各自根据自己的编码器误差生成目标速度。
   - 影响：这只能保证“左右轮都尽量走到目标脉冲”，不能保证行驶过程中左右同步；一侧电机更强或地面摩擦不同，就会先快后慢，车身轨迹会弯。
   - 建议：新增直线约束，使用 `left_delta - right_delta` 或 `wit_data.yaw - start_yaw` 作为纠偏量，让左右输出成对修正。

3. `mspm0-modules.syscfg:276` 中 `TIMER_Encoder` 周期是 `0.5s`，但 `DistanceControl/SpeedControl` 在主循环中高速反复调用。
   - 影响：速度反馈 `Current_Speed_Left/Right` 每 500ms 才更新一次，速度环在绝大多数调用中用的是旧速度，控制滞后很大，容易出现左右轮响应不一致。
   - 建议：把编码器速度采样和速度 PID 固定到 10-20ms 的定时节拍中执行，主循环只更新目标距离/目标速度。

4. `SPEED/speed.c:40-41` 对左右速度统一使用 `(int)(0.9079 * Current_Speed + 25.66)` 作为反馈速度。
   - 影响：静止时也会被当成约 25 个脉冲速度；反向或低速时这个固定偏置会明显破坏误差计算，左右轮也没有分别标定。
   - 建议：先用原始编码器增量闭环，确认方向和比例正确后，再分别标定左右轮的速度映射；如果要加死区补偿，应加在 PWM 输出侧，不要直接加到速度反馈侧。

5. `SPEED/speed.c:49-50` 会把 `target_speed + Delta_PWM` 直接传给电机控制函数；但 `Drivers/Motor/Motor.c:32-50` 和 `63-83` 会把负 PWM 钳为 0。
   - 影响：距离 PID 接近终点或超调时可能输出负值，但电机实际不能反向修正，只会变成停转。一侧停、一侧还在追，就容易在终点附近偏航。
   - 建议：引入有符号电机命令，根据正负自动选择方向；或在距离控制中设计减速区和到达保持策略，避免一侧直接被钳成 0。

6. `INTERRUPT/interrupt.c:52-55` 与 `INTERRUPT/interrupt.c:84-87` 中左右编码器加减方向相反。
   - 影响：这可能是因为左右电机镜像安装所做的补偿，也可能是方向写反。若前进时某一侧计数减少，距离控制的完成判断和 PID 误差都会不可靠。
   - 建议：低速前进时打印或显示 `encoderLeftCount`、`encoderRightCount`，确认两者都随前进增加；如果不是，需要调整编码器方向或在距离算法中统一符号。

## 改进建议

1. 增加真正的直线距离控制：
   - 用平均距离控制前进量：`avg_delta = (left_delta + right_delta) / 2`。
   - 用编码器差或陀螺仪航向做纠偏：`sync_error = left_delta - right_delta` 或 `yaw_error = wit_data.yaw - start_yaw`。
   - 输出示例：`left_pwm = base_pwm - correction`，`right_pwm = base_pwm + correction`。

2. 调整控制节拍：
   - 编码器速度采样建议改为 10-20ms。
   - 速度 PID 也只在同一个定时节拍执行一次，不要在主循环中无限频率重复调用。

3. 到达距离后锁定状态：
   - `DistanceControl` 返回 1 后立即停止，并设置状态机进入下一阶段。
   - 避免 `AbsoluateEncoder` 清零后被主循环重新初始化，导致同一距离任务反复开始。

4. 做左右轮基础标定：
   - 固定 PWM 下分别记录左右轮 `Current_Speed_*` 和累计编码器增量。
   - 校验前进时左右计数符号一致。
   - 分别建立左右轮低速死区补偿，补偿应作用在 PWM 输出上。

5. 增加调试输出：
   - OLED 或蓝牙打印 `left_delta`、`right_delta`、`current_speed_L`、`current_speed_R`、`yaw_error`、`pwm_L`、`pwm_R`。
   - 先确认“传感器方向正确、反馈周期合理、左右轮同 PWM 下速度接近”，再调 PID。

## 建议优先级

1. 先修主循环状态机：不要无限重复 `DistanceControl(650, 1)`。
2. 实测确认左右编码器前进时计数都增加。
3. 把速度采样周期从 0.5s 改到 10-20ms。
4. 去掉速度反馈中的固定 `+25.66` 偏置，先用原始编码器速度闭环。
5. 在距离控制中加入左右同步或 yaw 纠偏。

## 验证清单

- 前进低速时，`encoderLeftCount` 和 `encoderRightCount` 都稳定增加。
- 同一 PWM 下，左右轮速度差在可接受范围内。
- `DistanceControl(650, 1)` 返回 1 后电机停止，不会再次自动启动。
- 直线控制过程中，`left_delta - right_delta` 或 `yaw_error` 能被纠偏项压回接近 0。
- 车辆在 650mm、800mm、1000mm 三种距离下都能稳定停车，且终点姿态没有明显偏航。

## 具体代码修改方案：陀螺仪纠偏 + 固定周期速度控制

下面是建议你后续实际改代码时可以照着做的版本。本节只是说明方案，当前没有直接修改源码。

### 方案目标

- 编码器速度采样、速度 PID、距离外环都按同一个固定周期 `T` 执行，建议先用 `T = 10ms`，如果车身抖动明显再改成 `20ms`。
- 距离控制不再只让左右轮各自追目标，而是用平均编码器距离控制前进量，用陀螺仪 yaw 做直线纠偏。
- 到达距离后只返回完成状态，不在下一轮主循环自动重新开始同一段距离。

### 1. 修改 `mspm0-modules.syscfg`：把编码器定时器周期改成 10ms

位置：`mspm0-modules.syscfg:271-277`

将：

```c
TIMER1.timerClkDiv      = 8;
TIMER1.timerClkPrescale = 200;
TIMER1.interrupts       = ["ZERO"];
TIMER1.timerMode        = "PERIODIC_UP";
TIMER1.timerStartTimer  = true;
TIMER1.timerPeriod      = "0.5s";
TIMER1.$name            = "TIMER_Encoder";
```

改为：

```c
TIMER1.timerClkDiv      = 8;
TIMER1.timerClkPrescale = 200;
TIMER1.interrupts       = ["ZERO"];
TIMER1.timerMode        = "PERIODIC_UP";
TIMER1.timerStartTimer  = true;
TIMER1.timerPeriod      = "10 ms";
TIMER1.$name            = "TIMER_Encoder";
```

说明：你的 `Current_Speed_Left/Right` 是在 `TIMER_Encoder_INST_IRQHandler` 中由编码器差分得到的。现在 0.5s 更新一次太慢，速度环会严重滞后。改为 10ms 后，速度反馈和控制节拍才能对齐。

### 2. 修改 `INTERRUPT/interrupt.h`：导出控制周期标志位

位置：`INTERRUPT/interrupt.h` 中 `extern int Current_Speed_Right ;` 后面。

加入：

```c
extern volatile uint8_t speedControlTick;
```

说明：这个标志位由编码器定时器中断置 1，主循环或距离控制函数看到它为 1 时才执行一次控制，执行后清 0。

### 3. 修改 `INTERRUPT/interrupt.c`：每个编码器采样周期置一次控制标志

位置：`INTERRUPT/interrupt.c:99-102`

将：

```c
int Current_Speed_Left = 0;
int Current_Speed_Right = 0;
volatile int32_t Last_Left_count;
volatile int32_t Last_Right_count;
```

改为：

```c
int Current_Speed_Left = 0;
int Current_Speed_Right = 0;
volatile int32_t Last_Left_count;
volatile int32_t Last_Right_count;
volatile uint8_t speedControlTick = 0;
```

位置：`INTERRUPT/interrupt.c:118-123`

将：

```c
Current_Speed_Left = (cur_left - Last_Left_count);
Current_Speed_Right = (cur_right - Last_Right_count);

Last_Left_count = cur_left;
Last_Right_count = cur_right;
```

改为：

```c
Current_Speed_Left = cur_left - Last_Left_count;
Current_Speed_Right = cur_right - Last_Right_count;

Last_Left_count = cur_left;
Last_Right_count = cur_right;
speedControlTick = 1;
```

说明：这样速度采样、距离外环、速度 PID 都由同一个定时器节拍触发，避免主循环跑多快控制就执行多快。

### 4. 修改 `SPEED/speed.c`：去掉速度反馈固定偏置

位置：`SPEED/speed.c:40-41`

将：

```c
Delta_PWM_L = Speed(&SpeedPID_L, (int)(0.9079*Current_Speed_Left+25.66), target_speed_L);
Delta_PWM_R = Speed(&SpeedPID_R, (int)(0.9079*Current_Speed_Right+25.66), target_speed_R);
```

先改为：

```c
Delta_PWM_L = Speed(&SpeedPID_L, Current_Speed_Left, target_speed_L);
Delta_PWM_R = Speed(&SpeedPID_R, Current_Speed_Right, target_speed_R);
```

说明：固定 `+25.66` 会让静止状态也被当成有速度，低速直线控制时很容易让左右轮误差判断失真。建议先用原始编码器增量跑通，再单独标定左右轮。

### 5. 修改 `SPEED/speed.c`：新增 yaw 误差计算工具函数

位置：`SPEED/speed.c` 的 `DistanceControl` 前面，建议放在 `encoderFlag` 定义之后。

加入：

```c
#define DISTANCE_CONTROL_PERIOD_MS 10
#define YAW_CORRECTION_KP 3.0f
#define YAW_CORRECTION_LIMIT 80
#define DISTANCE_SPEED_LIMIT 300

static float NormalizeYawError(float error)
{
    if (error > 180.0f) {
        error -= 360.0f;
    } else if (error < -180.0f) {
        error += 360.0f;
    }
    return error;
}

static int LimitInt(int value, int minValue, int maxValue)
{
    if (value > maxValue) return maxValue;
    if (value < minValue) return minValue;
    return value;
}
```

说明：`NormalizeYawError` 用来处理 yaw 从 179 跳到 -179 的边界问题。`LimitInt` 用来统一限制速度和纠偏量，避免一侧 PWM 被修得太大。

### 6. 修改 `SPEED/speed.c`：用陀螺仪纠偏重写 `DistanceControl`

位置：替换 `SPEED/speed.c:67-137` 的整个 `DistanceControl` 函数。

建议将原函数：

```c
int DistanceControl(int target_distance,int dir) 
{
  ...
}
```

替换为：

```c
int DistanceControl(int target_distance, int dir)
{
    static float startYaw = 0.0f;

    if (!speedControlTick) {
        return 0;
    }
    speedControlTick = 0;

    if (!AbsoluateEncoder) {
        AbsoluateEncoder = (int32_t)((float)target_distance / (2.0f * Pi * Radios) * codePerCircle);
        startEncoder_L = encoderLeftCount;
        startEncoder_R = encoderRightCount;
        startYaw = wit_data.yaw;
        Reset_PID(&DistPID_L);
        Reset_PID(&DistPID_R);
    }

    int32_t left_delta = encoderLeftCount - startEncoder_L;
    int32_t right_delta = encoderRightCount - startEncoder_R;
    int32_t avg_delta = (left_delta + right_delta) / 2;

    DistPID_L.Error = AbsoluateEncoder - avg_delta;
    DistPID_L.Integral += DistPID_L.Error;
    DistPID_L.Integral = LimitInt(DistPID_L.Integral, -3000, 3000);

    int base_speed = (int)(DistPID_L.Kp * DistPID_L.Error +
                           DistPID_L.Ki * DistPID_L.Integral +
                           DistPID_L.Kd * (DistPID_L.Error - DistPID_L.Last_Error));
    DistPID_L.Last_Error = DistPID_L.Error;
    base_speed = LimitInt(base_speed, -DISTANCE_SPEED_LIMIT, DISTANCE_SPEED_LIMIT);

    float yaw_error = NormalizeYawError(wit_data.yaw - startYaw);
    int yaw_correction = (int)(YAW_CORRECTION_KP * yaw_error);
    yaw_correction = LimitInt(yaw_correction, -YAW_CORRECTION_LIMIT, YAW_CORRECTION_LIMIT);

    int target_speed_L = base_speed - yaw_correction;
    int target_speed_R = base_speed + yaw_correction;

    target_speed_L = LimitInt(target_speed_L, -DISTANCE_SPEED_LIMIT, DISTANCE_SPEED_LIMIT);
    target_speed_R = LimitInt(target_speed_R, -DISTANCE_SPEED_LIMIT, DISTANCE_SPEED_LIMIT);

    if (target_speed_L < 0 || target_speed_R < 0) {
        target_speed_L = 0;
        target_speed_R = 0;
    }

    SpeedControl(target_speed_L, target_speed_R, dir);

    if (avg_delta >= AbsoluateEncoder) {
        AbsoluateEncoder = 0;
        startEncoder_L = 0;
        startEncoder_R = 0;
        encoderFlag++;
        return 1;
    }

    return 0;
}
```

说明：

- `avg_delta` 负责距离闭环，避免左右轮分别抢自己的目标。
- `yaw_error` 负责直线纠偏，车头向一侧偏时，通过左右目标速度差拉回来。
- 这版仍保留 `SpeedControl(target_speed_L, target_speed_R, dir)`，改动范围较小。
- 如果你实测发现纠偏方向反了，把这两行对调符号即可：

```c
int target_speed_L = base_speed + yaw_correction;
int target_speed_R = base_speed - yaw_correction;
```

### 7. 修改 `main.c`：距离完成后停止，不要反复重启

位置：`main.c:104-105`

将：

```c
//  assignment_function[assignmentFlag]();
DistanceControl(650, 1);
```

改为：

```c
// assignment_function[assignmentFlag]();
static uint8_t distance_done = 0;

if (!distance_done) {
    if (DistanceControl(650, 1)) {
        stop();
        distance_done = 1;
    }
}
```

说明：这是单段 650mm 测试写法。正式任务里更建议恢复：

```c
assignment_function[assignmentFlag]();
```

然后在 `assignment1()` 这种状态机里用 `DistanceControl()` 的返回值推进 `stageFlag`。

### 8. 建议实测调参顺序

1. 先把 `YAW_CORRECTION_KP` 设成 `0.0f`，确认固定周期距离控制能稳定停下。
2. 确认前进时 `left_delta`、`right_delta`、`avg_delta` 都增加。
3. 再把 `YAW_CORRECTION_KP` 从 `1.0f` 开始逐步加到 `3.0f`、`5.0f`。
4. 如果车身左右摆动，降低 `YAW_CORRECTION_KP` 或把周期改成 `20 ms`。
5. 如果修正太慢，提高 `YAW_CORRECTION_KP`，但不要先动距离 PID。

### 9. 需要特别注意的点

- 这套方案默认 `wit_data.yaw` 在直行过程中能稳定更新。如果陀螺仪串口更新慢或丢包，纠偏会变钝。
- 这套方案默认前进时左右编码器计数都是增加。如果不是，必须先修编码器符号。
- 当前 `SpeedControl` 仍然不支持负目标速度，所以建议先只做前进距离控制。后退距离控制要另做有符号速度命令。
- 如果 `SpeedControl` 内部的增量式 PID 仍然抖动，下一步应把速度 PID 的“上一轮 PWM 累计量”和“目标速度”分开，不要用 `target_speed + Delta_PWM` 直接当 PWM。

## 任务一全局 yaw 记忆方案

你的想法是对的，而且很适合当前 `assignment1()` 的运动结构。现在 `DistanceControl()` 的 `yawcorrection` 逻辑是在每一段距离刚开始时执行：

```c
startYaw = wit_data.yaw;
```

这意味着每次左转或右转结束后，哪怕转弯少了 1 度或多了 1 度，下一段直行都会把这个带误差的实际 yaw 当成新的直行目标。这样多次转弯之后，小误差就会被一段段继承，最后全局位置和方向都会慢慢漂掉。

更好的做法是：在任务一开始时锁定一次 `task1_base_yaw = wit_data.yaw`，后续每一段直行不再使用“转弯后的实际 yaw”，而是使用“任务一起始 yaw + 理论累计转角”作为 `yawcorrection` 的目标 yaw。

需要特别注意：不能让左转 90 度后的直行仍然直接修正到 `task1_base_yaw` 原值，否则小车会试图回到起始朝向。任务一当前流程是：

```c
stage 0: Left_Turn(90)
stage 1: DistanceControl(650, 1)
stage 2: Right_Turn(90)
stage 3: DistanceControl(800, 1)
stage 4: Right_Turn(90)
stage 5: DistanceControl(1000, 1)
```

因此三段直行更合理的目标 yaw 应该是：

```c
stage 1: task1_base_yaw + 90.0f
stage 3: task1_base_yaw + 0.0f
stage 5: task1_base_yaw - 90.0f
```

这样即使第一次左转实际只转到了 `base + 88.5`，第一段直行也会继续向 `base + 90` 修正；后面右转也是同理，不会把上一段的转弯误差当成新的标准。

### 建议 1：给距离控制增加指定目标 yaw 的接口

位置：`SPEED/speed.h`

保留原来的 `DistanceControl()`，再新增一个带目标 yaw 的接口，避免影响其他任务：

```c
int DistanceControl(int target_distance, int dir);
int DistanceControlWithYaw(int target_distance, int dir, float target_yaw);
```

位置：`SPEED/speed.c`

建议把现有 `DistanceControl()` 的主体改名为 `DistanceControlWithYaw()`，并把初始化时的：

```c
startYaw = wit_data.yaw;
```

改成：

```c
startYaw = target_yaw;
```

然后保留一个兼容包装函数：

```c
int DistanceControl(int target_distance, int dir)
{
    return DistanceControlWithYaw(target_distance, dir, wit_data.yaw);
}
```

这样普通距离控制仍然保持“以当前 yaw 为直行目标”的旧行为；任务一则可以显式传入全局规划 yaw。

同时建议把 `NormalizeYawError()` 改成更稳的循环限制，防止目标 yaw 或当前 yaw 跨过正负 180 度边界：

```c
static float NormalizeYawError(float error)
{
    while (error > 180.0f) {
        error -= 360.0f;
    }
    while (error < -180.0f) {
        error += 360.0f;
    }
    return error;
}
```

`yawcorrection` 部分继续使用：

```c
float yaw_error = NormalizeYawError(wit_data.yaw - startYaw);
int yaw_correction = (int)(YAW_CORRECTION_KP * yaw_error);
```

如果实车发现修正方向反了，只需要对调左右目标速度里的符号：

```c
int target_speed_L = base_speed + yaw_correction;
int target_speed_R = base_speed - yaw_correction;
```

或：

```c
int target_speed_L = base_speed - yaw_correction;
int target_speed_R = base_speed + yaw_correction;
```

### 建议 2：在任务一开始锁定 base yaw

位置：`AssignmentChoose/assignment.c`

在 `assignment1()` 附近增加任务一专用的静态变量和目标 yaw 计算函数：

```c
static uint8_t task1_yaw_locked = 0;
static float task1_base_yaw = 0.0f;

static float Task1TargetYaw(float offset)
{
    float target = task1_base_yaw + offset;
    RangeLimite(&target, 180.0f);
    return target;
}
```

在 `assignment1()` 开头锁定一次 yaw：

```c
void assignment1(void)
{
  if (!task1_yaw_locked) {
    task1_base_yaw = wit_data.yaw;
    task1_yaw_locked = 1;
  }

  ...
}
```

然后把三段直行改成显式传入理论目标 yaw：

```c
else if (stageFlag == 1)
{
  DistanceControlWithYaw(650, 1, Task1TargetYaw(90.0f));
  if (encoderFlag == 1)
  {
    stageFlag = 2;
    stop();
  }
}
else if (stageFlag == 3)
{
  DistanceControlWithYaw(800, 1, Task1TargetYaw(0.0f));
  if (encoderFlag == 2)
  {
    stageFlag = 4;
    stop();
  }
}
else if (stageFlag == 5)
{
  DistanceControlWithYaw(1000, 1, Task1TargetYaw(-90.0f));
  if (encoderFlag == 3)
  {
    stageFlag = 6;
    stop();
  }
}
```

这就是你想要的“记住当前 `wit_data.yaw`，并在后续转弯后的编码器控速 yaw correction 中使用记住的 yaw”。更精确地说，是使用记住的 yaw 推导出来的全局理论 yaw。

### 建议 3：任务结束或重新选任务时清除锁定

如果任务一只跑一次，可以在 `stageFlag == 6` 后保持锁定不动。但如果后面可能重新开始任务一，建议在 `zeroparameter()` 或任务完成处清掉：

```c
task1_yaw_locked = 0;
task1_base_yaw = 0.0f;
```

如果 `zeroparameter()` 和这些静态变量在同一个 `assignment.c` 文件里，直接清除即可；如果后面拆到别的文件，再提供一个 `ResetAssignment1Yaw()` 函数会更干净。

### 验证方法

建议先通过 OLED 或蓝牙打印下面几个量：

```c
task1_base_yaw
Task1TargetYaw(...)
wit_data.yaw
yaw_error
yaw_correction
```

实测时重点看三件事：

1. 任务一刚开始时 `task1_base_yaw` 只锁定一次，不要每个循环都刷新。
2. 左转 90 度后的第一段直行，目标 yaw 应接近 `base + 90`。
3. 第二次右转后的第三段直行，目标 yaw 应接近 `base - 90`。

如果这三点成立，转弯产生的小角度误差就不会继续成为下一段直行的“新标准”，全局运动能力会比现在稳定。

## 任务一单独锁定 yaw 的完整代码建议

下面给出“建议 2”的可落地版本：只在任务一里锁定一次起始 yaw，然后三段直行分别使用由起始 yaw 推导出来的理论目标 yaw。这样不会影响其他任务，也不会把转弯后的实际小误差当成下一段直行的新基准。

### 1. 修改 `SPEED/speed.h`：增加带目标 yaw 的距离控制接口

位置：`SPEED/speed.h` 中原来的声明：

```c
int DistanceControl(int target_distance,int dir);
```

建议改为：

```c
int DistanceControl(int target_distance, int dir);
int DistanceControlWithYaw(int target_distance, int dir, float target_yaw);
```

说明：`DistanceControl()` 保留给其他普通场景使用；`DistanceControlWithYaw()` 专门给任务一这种“全局规划航向”使用。

### 2. 修改 `SPEED/speed.c`：把现有 `DistanceControl` 改成可指定 yaw

位置：`SPEED/speed.c` 当前 `DistanceControl()` 函数。

把函数头：

```c
int DistanceControl(int target_distance, int dir)
{
    static float startYaw = 0.0f;
```

改为：

```c
int DistanceControlWithYaw(int target_distance, int dir, float target_yaw)
{
    static float startYaw = 0.0f;
```

然后在初始化距离任务的位置，把：

```c
startYaw = wit_data.yaw;
```

改为：

```c
startYaw = target_yaw;
```

也就是这一段应变成：

```c
if (!AbsoluateEncoder) {
    AbsoluateEncoder = (int32_t)((float)target_distance / (2.0f * Pi * Radios) * codePerCircle);
    startEncoder_L = encoderLeftCount;
    startEncoder_R = encoderRightCount;
    startYaw = target_yaw;
    Reset_PID(&DistPID_L);
    Reset_PID(&DistPID_R);
}
```

在这个函数结束后，再补一个兼容旧接口的包装函数：

```c
int DistanceControl(int target_distance, int dir)
{
    return DistanceControlWithYaw(target_distance, dir, wit_data.yaw);
}
```

这样旧代码继续调用 `DistanceControl(650, 1)` 也能工作；任务一改用 `DistanceControlWithYaw()`。

同时建议把当前的 `NormalizeYawError()` 从单次 `if` 修正改成循环修正：

```c
static float NormalizeYawError(float error)
{
    while (error > 180.0f) {
        error -= 360.0f;
    }
    while (error < -180.0f) {
        error += 360.0f;
    }
    return error;
}
```

这样 `base_yaw + 90`、`base_yaw - 90` 跨过 `180/-180` 边界时也不会出问题。

### 3. 修改 `AssignmentChoose/assignment.c`：任务一内部锁定起始 yaw

位置：`assignment.c` 中 `int stageFlag = 0;` 后面，`assignment1()` 前面。

新增：

```c
static uint8_t task1_yaw_locked = 0;
static float task1_base_yaw = 0.0f;

static float Task1TargetYaw(float offset)
{
    float target = task1_base_yaw + offset;
    RangeLimite(&target, 180.0f);
    return target;
}
```

说明：`offset` 是从任务一起始方向开始计算的理论累计转角。左转 90 后是 `+90`，右转回来是 `0`，再右转 90 是 `-90`。

然后把 `assignment1()` 开头改成：

```c
void assignment1(void)
{
  if (!task1_yaw_locked)
  {
    task1_base_yaw = wit_data.yaw;
    task1_yaw_locked = 1;
  }

  if (stageFlag == 0 && turnCompleted == 0)
  {
    Left_Turn(90);
    if (turnCompleted == 1)stageFlag++;
  }
```

也就是只在第一次进入任务一时记录 `wit_data.yaw`，后面不要再刷新它。

### 4. 修改 `assignment1()` 三段直行调用

位置：`AssignmentChoose/assignment.c` 的 `assignment1()`。

把第一段直行：

```c
DistanceControl(650, 1);
```

改为：

```c
DistanceControlWithYaw(650, 1, Task1TargetYaw(90.0f));
```

把第二段直行：

```c
DistanceControl(800, 1);
```

改为：

```c
DistanceControlWithYaw(800, 1, Task1TargetYaw(0.0f));
```

把第三段直行：

```c
DistanceControl(1000, 1);
```

改为：

```c
DistanceControlWithYaw(1000, 1, Task1TargetYaw(-90.0f));
```

修改后的三段直行逻辑大概是：

```c
else if (stageFlag == 1)
{
  DistanceControlWithYaw(650, 1, Task1TargetYaw(90.0f));
  if (encoderFlag == 1)
  {
    stageFlag = 2;
    stop();
  }
}
else if (stageFlag == 3)
{
  DistanceControlWithYaw(800, 1, Task1TargetYaw(0.0f));
  if (encoderFlag == 2)
  {
    stageFlag = 4;
    stop();
  }
}
else if (stageFlag == 5)
{
  DistanceControlWithYaw(1000, 1, Task1TargetYaw(-90.0f));
  if (encoderFlag == 3)
  {
    stageFlag = 6;
    stop();
  }
}
```

### 5. 修改 `zeroparameter()`：重新开始任务时允许重新锁定 yaw

位置：`AssignmentChoose/assignment.c` 的 `zeroparameter()`。

在函数末尾加入：

```c
task1_yaw_locked = 0;
task1_base_yaw = 0.0f;
```

完整建议：

```c
void zeroparameter(void)
{
    CrossingFlag = 0;
    LeftTurnFlag = 0;
    RightTurnFlag = 0;
    turnCompleted = 0;
    assignmentFlag = 0;
    TurnOverFlag = 0;

    task1_yaw_locked = 0;
    task1_base_yaw = 0.0f;
}
```

如果你后面还会重置 `stageFlag` 和 `encoderFlag`，也建议一并在这里清掉，否则重新执行任务一时状态机可能不会从第 0 阶段开始。

## 云台 UART 接收函数建议

你现在在 SysConfig 里新增的云台串口实例名是：

```c
UART_Gimbal
```

生成出来的宏名会是：

```c
UART_Gimbal_INST
UART_Gimbal_INST_IRQHandler
UART_Gimbal_INST_INT_IRQN
```

当前 `UART/uart_gimbal.h` 和 `UART/uart_gimbal.c` 还是从 K230 代码复制来的，里面有 `K230_Init()`、`UART_Camera_INST_IRQHandler()`、`Camera_buffer`、`CameraData` 等旧名字。建议不要继续沿用这些名字，直接替换成独立的云台模块。

### 1. SysConfig 建议

如果你采用下面这种“RX 中断逐字节接收”的方式，建议云台 UART 只保留 RX 中断：

```c
UART3.enabledInterrupts = ["RX"];
```

并取消云台 UART 的 RX DMA：

```c
UART3.enabledDMARXTriggers
UART3.DMA_CHANNEL_RX
```

原因：如果同时开 RX DMA 和 RX 中断，DMA 可能先把 `RXDATA` 搬走，导致中断函数里读不到预期字节。云台数据量通常不大，用 RX 中断收包更直观。

如果你暂时不需要向云台发送数据，也建议先不要开 TX interrupt。阻塞发送 `DL_UART_Main_transmitDataBlocking()` 不需要 TX 中断。

### 2. 替换 `UART/uart_gimbal.h`

位置：`UART/uart_gimbal.h`。

建议把整个文件从 K230 声明替换为：

```c
#ifndef UART_GIMBAL_H_
#define UART_GIMBAL_H_

#include <stdint.h>
#include <stdbool.h>

#define GIMBAL_RX_BUFFER_SIZE 64

void Gimbal_Init(void);
void Gimbal_Process(void);
bool Gimbal_HasNewFrame(void);
void Gimbal_SendByte(uint8_t data);
void Gimbal_SendString(const char *str, uint16_t max_len);

void UART_Gimbal_INST_IRQHandler(void);

#endif
```

这里不再出现任何 `K230`、`Camera`、`assignmentFlag` 相关名字。

### 3. 替换 `UART/uart_gimbal.c`

位置：`UART/uart_gimbal.c`。

建议把整个文件替换为：

```c
#include "uart_gimbal.h"
#include "global.h"
#include "ti_msp_dl_config.h"
#include <stdlib.h>
#include <string.h>

static char gimbal_rx_buffer[GIMBAL_RX_BUFFER_SIZE];
static char gimbal_frame_buffer[GIMBAL_RX_BUFFER_SIZE];
static volatile uint8_t gimbal_rx_index = 0;
static volatile uint8_t gimbal_receiving = 0;
static volatile uint8_t gimbal_frame_ready = 0;

static void Gimbal_PushByte(uint8_t data)
{
    if (data == '$') {
        gimbal_rx_index = 0;
        gimbal_receiving = 1;
        return;
    }

    if (data == '#') {
        if (gimbal_receiving) {
            gimbal_rx_buffer[gimbal_rx_index] = '\0';
            memcpy(gimbal_frame_buffer, gimbal_rx_buffer, gimbal_rx_index + 1);
            gimbal_frame_ready = 1;
        }
        gimbal_receiving = 0;
        gimbal_rx_index = 0;
        return;
    }

    if (gimbal_receiving) {
        if (gimbal_rx_index < (GIMBAL_RX_BUFFER_SIZE - 1)) {
            gimbal_rx_buffer[gimbal_rx_index++] = (char)data;
        } else {
            gimbal_receiving = 0;
            gimbal_rx_index = 0;
        }
    }
}

void Gimbal_Init(void)
{
    gimbal_rx_index = 0;
    gimbal_receiving = 0;
    gimbal_frame_ready = 0;

    NVIC_ClearPendingIRQ(UART_Gimbal_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_Gimbal_INST_INT_IRQN);
}

bool Gimbal_HasNewFrame(void)
{
    return gimbal_frame_ready != 0;
}

void Gimbal_Process(void)
{
    char frame[GIMBAL_RX_BUFFER_SIZE];
    float value[4] = {0};
    uint8_t count = 0;

    if (!gimbal_frame_ready) {
        return;
    }

    __disable_irq();
    memcpy(frame, gimbal_frame_buffer, GIMBAL_RX_BUFFER_SIZE);
    gimbal_frame_ready = 0;
    __enable_irq();

    char *token = strtok(frame, ",");
    while (token != NULL && count < 4) {
        value[count++] = strtof(token, NULL);
        token = strtok(NULL, ",");
    }

    if (count >= 2) {
        Yuntai.yaw = value[0];
        Yuntai.pitch = value[1];
    }

    if (count >= 4) {
        Yuntai.x = value[2];
        Yuntai.y = value[3];
    }
}

void Gimbal_SendByte(uint8_t data)
{
    DL_UART_Main_transmitDataBlocking(UART_Gimbal_INST, data);
}

void Gimbal_SendString(const char *str, uint16_t max_len)
{
    uint16_t count = 0;

    if (str == NULL || max_len == 0) {
        return;
    }

    while (*str != '\0' && count < max_len) {
        Gimbal_SendByte((uint8_t)*str);
        str++;
        count++;
    }
}

void UART_Gimbal_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_Gimbal_INST)) {
    case DL_UART_MAIN_IIDX_RX:
        Gimbal_PushByte(DL_UART_Main_receiveData(UART_Gimbal_INST));
        break;

    default:
        break;
    }
}
```

这版默认云台发送数据格式为：

```text
$yaw,pitch,x,y#
```

例如：

```text
$12.5,-3.0,100,80#
```

如果云台只发两个角度，也可以只发：

```text
$12.5,-3.0#
```

此时只更新 `Yuntai.yaw` 和 `Yuntai.pitch`。

### 4. 修改 `main.c`：接入云台串口，删除旧 K230 引用

位置：`main.c` include 区域。

把：

```c
#include "K230.h"
```

改为：

```c
#include "uart_gimbal.h"
```

位置：初始化区域，建议在 `WIT_Init();` 后面加入：

```c
Gimbal_Init();
```

位置：`while (1)` 主循环里，建议加入：

```c
Gimbal_Process();
```

建议放在任务调度前后都可以，例如：

```c
while (1)
{
    Gimbal_Process();

    ...

    assignment_function[assignmentFlag]();
}
```

如果当前工程里已经没有 `UART_AI` 这个 SysConfig 实例，下面这段也建议删除，否则会出现宏未定义或无用中断：

```c
NVIC_ClearPendingIRQ(UART_AI_INST_INT_IRQN);
NVIC_EnableIRQ(UART_AI_INST_INT_IRQN);
```

以及文件末尾这个旧的空处理函数：

```c
void UART_AI_INST_IRQHandler(void)
{
 switch (DL_UART_Main_getPendingInterrupt(UART_AI_INST)) {
  case DL_UART_MAIN_IIDX_RX:
    getdata = DL_UART_Main_receiveData(UART_AI_INST);
    DL_GPIO_togglePins(LED_PORT,LED_USER_LED_PIN);
    break;

  default:
    break;
  }
}
```

如果你确实还要保留 AI 调参串口，就不要删 `UART_AI`；但从当前 SysConfig 搜索结果看，只有 `UART_WIT`、`UART_BT`、`UART_Gimbal`，没有 `UART_AI`。

### 5. 修改 `global.h`：删除 K230 全局变量声明

位置：`global.h` 中这段：

```c
//K230通信部分
extern char Camera_buffer[20];
extern volatile char ReceiveData;
extern uint8_t BufferIndex;
extern int ReceivingFlag;//接收标志
extern int ProcessFlag;//处理标志
extern int dataCount;
extern int Final_data[20];
extern int CameraData[20];
extern int num_of_data;
```

建议整段删除。

云台接收状态不建议继续放在 `global.h` 里，而是像上面的 `uart_gimbal.c` 一样做成 `static` 内部状态。对外只暴露：

```c
Gimbal_Init();
Gimbal_Process();
Gimbal_HasNewFrame();
Gimbal_SendString();
```

真正需要给其他模块使用的数据，继续放在已有的：

```c
extern struct Data Yuntai;
```

### 6. 清理 `UART/uart_gimbal.c` 里的旧 K230 信息

下面这些都应删除或被上面的新代码替换：

```c
#include "K230.h"
#include "assignment.h"

char Camera_buffer[20];
volatile char ReceiveData;
uint8_t BufferIndex = 0;
int ReceivingFlag = 0;
int ProcessFlag = 0;
int dataCount = 0;
int Final_data[20];
int CameraData[20];
int num_of_data = 0;

K230_Init()
UART_Camera_INST_IRQHandler()
Process()
GetCameraData()
assignmentFlag = CameraData[0];
```

云台串口不应该再修改 `assignmentFlag`。任务选择归任务选择模块，云台数据只负责更新 `Yuntai`。

### 7. 编译前检查清单

1. `UART/uart_gimbal.h` 的头文件保护不再是 `__K230_`。
2. `UART/uart_gimbal.c` 不再 include `K230.h`。
3. `main.c` 不再 include `K230.h`。
4. SysConfig 里 `UART_Gimbal` 只开 RX interrupt，不开 RX DMA 和 TX interrupt。
5. `UART_Gimbal_INST_IRQHandler` 最终会由宏展开成 `UART3_IRQHandler`，和生成文件里的中断向量匹配。
6. 主循环周期性调用 `Gimbal_Process()`，不要在串口中断里直接做 `strtok/strtof` 这类解析工作。

## 关于 `DistanceControlWithYaw()` 倒车距离控制的建议

### 结论

不建议只靠在 `DistanceControlWithYaw()` 初始化时把 `startEncoder_L`、`encoderLeftCount`、`encoderRightCount` 清零来解决倒车问题。

这个办法短期看起来能避开“倒车时编码器变成负数”的现象，但它不是最稳的方案，原因有三个：

1. `encoderLeftCount` 和 `encoderRightCount` 是中断里持续更新的全局累计值，距离控制函数直接清零它们，会影响其他依赖编码器的模块。
2. 速度采样中断里还有 `Last_Left_count` 和 `Last_Right_count`。如果只清当前编码器计数，不同时清这两个历史计数，下一次 `Current_Speed_Left/Right` 会突然出现一个很大的跳变，速度 PID 可能猛冲一下。
3. 你当前倒车出问题的关键不是“起点不是 0”，而是 `avg_delta` 在倒车时可能是负数，导致距离 PID 的误差一直变大：`target - (-delta)`，所以 PID 没法随着接近目标而减速。

更推荐的思路是：不要强行改全局编码器累计值，而是在本次距离控制内部把编码器增量转换成“已经走过的正距离”。也就是：

```c
left_delta = encoderLeftCount - startEncoder_L;
right_delta = encoderRightCount - startEncoder_R;

left_travel = abs(left_delta);
right_travel = abs(right_delta);
avg_travel = (left_travel + right_travel) / 2;
```

这样前进和倒车都用同一套距离 PID：目标距离永远是正数，已经行驶距离也永远是正数。

### 对你提出的清零方案的评价

如果你确实想在每段运动开始时清零编码器，也必须这样做才相对安全：

```c
__disable_irq();
encoderLeftCount = 0;
encoderRightCount = 0;
Last_Left_count = 0;
Last_Right_count = 0;
__enable_irq();

startEncoder_L = 0;
startEncoder_R = 0;
```

也就是说，不能只清 `encoderLeftCount` 和 `encoderRightCount`，还要同步清速度采样使用的 `Last_Left_count`、`Last_Right_count`。否则速度反馈会被污染。

但是我仍然更建议不要清全局编码器，而是保留累计计数，只记录本段运动的起点，再算相对增量。这样模块之间互相影响最小。

### 当前函数里还有两个明显问题

第一，你已经算出了 PID 解算后的：

```c
target_speed_L
target_speed_R
```

但最后真正执行的是：

```c
SpeedControl(150 + yaw_correction, 150 - yaw_correction, dir);
```

这等于放弃了距离 PID 的 `base_speed`，所以小车不会在接近目标时自然减速。为了实现“PID + 编码器距离控制”，应该改成：

```c
SpeedControl(target_speed_L, target_speed_R, dir);
```

第二，倒车时 yaw 纠偏方向通常要反过来。因为同样的“左轮快、右轮慢”，前进和倒车造成的车身转向方向是相反的。所以建议：

```c
if (dir == 0) {
    yaw_correction = -yaw_correction;
}
```

如果实车测试发现纠偏方向反了，再把左右两侧的 `+ yaw_correction` 和 `- yaw_correction` 对调即可。

### 推荐标准代码

下面这版不清零全局编码器，而是在函数内部使用相对增量，并用绝对行驶量解决倒车距离判断问题。

```c
static int32_t AbsInt32(int32_t value)
{
    return value >= 0 ? value : -value;
}

int DistanceControlWithYaw(int target_distance, int dir, float target_yaw)
{
    static float startYaw = 0.0f;

    if (!speedControlTick) {
        return 0;
    }
    speedControlTick = 0;

    if (!AbsoluateEncoder) {
        AbsoluateEncoder = (int32_t)((float)target_distance /
                           (2.0f * Pi * Radios) * codePerCircle);

        __disable_irq();
        startEncoder_L = encoderLeftCount;
        startEncoder_R = encoderRightCount;
        __enable_irq();

        startYaw = target_yaw;
        Reset_PID(&DistPID_L);
        Reset_PID(&DistPID_R);
    }

    int32_t current_left;
    int32_t current_right;

    __disable_irq();
    current_left = encoderLeftCount;
    current_right = encoderRightCount;
    __enable_irq();

    int32_t left_delta = current_left - startEncoder_L;
    int32_t right_delta = current_right - startEncoder_R;

    int32_t left_travel = AbsInt32(left_delta);
    int32_t right_travel = AbsInt32(right_delta);
    int32_t avg_travel = (left_travel + right_travel) / 2;

    if (avg_travel >= AbsoluateEncoder) {
        SpeedControl(0, 0, dir);
        AbsoluateEncoder = 0;
        startEncoder_L = 0;
        startEncoder_R = 0;
        Reset_PID(&DistPID_L);
        Reset_PID(&DistPID_R);
        encoderFlag++;
        return 1;
    }

    DistPID_L.Error = AbsoluateEncoder - avg_travel;
    DistPID_L.Integral += DistPID_L.Error;
    DistPID_L.Integral = LimitInt(DistPID_L.Integral, -3000, 3000);

    int base_speed = (int)(DistPID_L.Kp * DistPID_L.Error +
                           DistPID_L.Ki * DistPID_L.Integral +
                           DistPID_L.Kd * (DistPID_L.Error - DistPID_L.Last_Error));
    DistPID_L.Last_Error = DistPID_L.Error;

    base_speed = LimitInt(base_speed, 0, DISTANCE_SPEED_LIMIT);

    float yaw_error = NormalizeYawError(wit_data.yaw - startYaw);
    int yaw_correction = (int)(YAW_CORRECTION_KP * yaw_error);
    yaw_correction = LimitInt(yaw_correction,
                              -YAW_CORRECTION_LIMIT,
                              YAW_CORRECTION_LIMIT);

    if (dir == 0) {
        yaw_correction = -yaw_correction;
    }

    int target_speed_L = base_speed + yaw_correction;
    int target_speed_R = base_speed - yaw_correction;

    target_speed_L = LimitInt(target_speed_L, 0, DISTANCE_SPEED_LIMIT);
    target_speed_R = LimitInt(target_speed_R, 0, DISTANCE_SPEED_LIMIT);

    SpeedControl(target_speed_L, target_speed_R, dir);

    return 0;
}
```

### 如果倒车速度 PID 仍然异常

上面的代码解决的是“距离 PID 倒车时误差方向错误”的问题。但你的 `SpeedControl()` 现在仍然直接使用：

```c
Current_Speed_Left
Current_Speed_Right
```

如果倒车时这两个速度反馈是负数，而目标速度是正数，速度 PID 也可能不舒服。更标准的处理是让 `SpeedControl()` 根据 `dir` 把速度反馈统一成正值：

```c
void SpeedControl(int target_speed_L, int target_speed_R, int dir)
{
    int feedback_L = Current_Speed_Left;
    int feedback_R = Current_Speed_Right;

    if (dir == 0) {
        feedback_L = -feedback_L;
        feedback_R = -feedback_R;
    }

    Delta_PWM_L = Speed(&SpeedPID_L, feedback_L, target_speed_L);
    Delta_PWM_R = Speed(&SpeedPID_R, feedback_R, target_speed_R);

    if (Delta_PWM_L > 300)  Delta_PWM_L = 300;
    if (Delta_PWM_L < -300) Delta_PWM_L = -300;

    if (Delta_PWM_R > 300)  Delta_PWM_R = 300;
    if (Delta_PWM_R < -300) Delta_PWM_R = -300;

    Left_Control(dir, target_speed_L + Delta_PWM_L);
    Right_Control(dir, target_speed_R + Delta_PWM_R);
}
```

注意：这段成立的前提是“前进时编码器速度为正，倒车时编码器速度为负”。如果实车上某一侧相反，要先统一编码器方向。

### 最小验证步骤

1. 把车架空，前进低速跑，确认 `encoderLeftCount` 和 `encoderRightCount` 都朝同一方向变化。
2. 再倒车低速跑，确认两者都朝相反方向变化。
3. 打印 `left_delta`、`right_delta`、`avg_travel`，确认倒车时 `avg_travel` 仍然从 0 增加到目标编码器值。
4. 先把 `YAW_CORRECTION_KP` 调小，例如 `1.0f`，确认距离控制正常后再逐渐加大。
5. 如果倒车纠偏越修越偏，把 `yaw_correction` 的符号或左右轮加减关系对调。
