# 任务函数代码逻辑梳理报告

本报告只梳理代码和列出疑点，未修改任务源码。分析范围主要是 `AssignmentChoose/assignment.c` 中的 `assignment0` 到 `assignment7`，并结合 `main.c`、`SPEED/speed.c`、`TURN/turn.c`、`Drivers/Hostcom/hostcom.c` 理解状态推进逻辑。

## 一、整体运行方式

1. `main.c` 中定义了任务函数表：
   - `assignment_function[0] = assignment0`
   - `assignment_function[1] = assignment1`
   - ...
   - `assignment_function[7] = assignment7`

2. 开机后先调用 `AssignmentChoose()`：
   - 左键递增 `assignmentFlag`，范围是 `0~7`。
   - 右键确认任务编号。
   - 第二阶段用左键选择 `shapeFlag`，当前只有 `head/tail` 两个字符串，范围是 `0~1`。
   - 选择完成后蜂鸣/LED提示，并通过 `Host_Send((char)('a' + assignmentFlag - 1), "000000", '0')` 告知上位机当前任务。

3. 主循环中反复执行：
   - 更新巡线传感器。
   - OLED 显示 `stageFlag`、`assignmentFlag`、传感器数量。
   - 调用 `assignment_function[assignmentFlag]()`。

4. 任务函数本质上都是状态机：
   - `stageFlag` 决定当前阶段。
   - 距离阶段主要靠 `DistanceControlWithYaw()` 完成后递增 `encoderFlag`。
   - 转向阶段主要靠 `Left_Turn()` / `Right_Turn()` 完成后递增 `turnCompleted`。
   - 每次主循环只推进当前阶段的一小步，符合非阻塞状态机思路。

## 二、共用状态和关键函数

1. `TaskYawInitialization()`
   - 每个任务开始时调用。
   - 第一次调用时把当前 `wit_data.yaw` 锁定到 `task1_base_yaw`。
   - 后续 `Task1TargetYaw(offset)` 都以这个初始角作为基准计算目标航向。
   - `zeroparameter()` 会把 `task1_yaw_locked` 清零，使下次任务可重新锁定初始 yaw。

2. `Task1TargetYaw(offset)`
   - 返回 `task1_base_yaw + offset`。
   - 通过 `RangeLimite(&target, 180.0f)` 把角度限制到约 `[-180, 180]`。

3. `DistanceControlWithYaw(target_distance, dir, target_yaw, basic_speed, Distance_pid)`
   - 当内部全局 `AbsoluateEncoder == 0` 时初始化本段距离目标、起始编码器、起始 yaw。
   - 每到一个速度控制 tick 才运行一次，否则返回 `0`。
   - 到达目标距离后：
     - 调 `SpeedControl(0, 0, dir)` 停车。
     - 清空本段距离控制状态。
     - `encoderFlag++`。
     - 返回 `1`。
   - 未到达时返回 `0`。

4. `Left_Turn()` / `Right_Turn()`
   - 内部用静态 `turn_step` 做非阻塞转向状态机。
   - 完成一次转向后 `turnCompleted++`。

## 三、各任务函数流程

### assignment0

只调用 `trackSensorUpdate()`，相当于空闲/传感器刷新任务，不控制小车运动。

### assignment1

`assignment1()` 先锁定初始 yaw，然后按 `stageFlag` 执行：

| 阶段 | 动作 | 完成条件 | 完成后 |
| --- | --- | --- | --- |
| 0 | 前进 850，目标 yaw 为初始 yaw | `encoderFlag == 1` | `stageFlag = 1`，停车 |
| 1 | 右转 90 度 | `turnCompleted == 1` | `stageFlag++` |
| 2 | 前进 140，目标 yaw 为初始 yaw - 90 | `encoderFlag == 2` | `stageFlag++`，停车 |
| 3 | 右转 90 度 | `turnCompleted == 2` | `stageFlag++` |
| 4 | 前进 380，目标 yaw 为初始 yaw - 180 | `encoderFlag == 3` | 发送 `Host_Send('0', ...)`，停车，延时 5 秒，进入下一阶段 |
| 5 | 前进 570，目标 yaw 为初始 yaw - 180 | `encoderFlag == 4` | `stageFlag++`，停车 |
| 6 | 右转 90 度 | `turnCompleted == 3` | `stageFlag++` |
| 7 | 前进 650，目标 yaw 为初始 yaw + 90 | `encoderFlag == 5` | `stageFlag++`，停车 |
| 8 | 右转 90 度 | `turnCompleted == 4` | `stageFlag++` |
| 9 | 后退 230，目标 yaw 为初始 yaw - 180 | `encoderFlag == 6` | 发送 `Host_Send('1', ...)`，停车，进入下一阶段 |
| 10 | 前进 830，目标 yaw 为初始 yaw | `encoderFlag == 7` | `stageFlag++`，停车 |

注意：当前 `case 9` 后面缺少 `break`，详见“发现的问题”。

### assignment2

`assignment2()` 当前逻辑与 `assignment1()` 基本完全一致，也有相同的 `case 9` 漏 `break` 风险。

### assignment3

`assignment3()` 先锁定初始 yaw，然后执行另一条路线：

| 阶段 | 动作 | 完成条件 | 完成后 |
| --- | --- | --- | --- |
| 0 | 左转 90 度 | `turnCompleted == 1` | `stageFlag++` |
| 1 | 前进 560，目标 yaw 为初始 yaw + 90 | `encoderFlag == 1` | `stageFlag = 2`，停车 |
| 2 | 右转 90 度 | `turnCompleted == 2` | `stageFlag++` |
| 3 | 后退 200，目标 yaw 为初始 yaw | `encoderFlag == 2` | 发送 `Host_Send('0', ...)`，停车 |
| 4 | 前进 980，目标 yaw 为初始 yaw | `encoderFlag == 3` | 发送 `Host_Send('1', ...)`，停车 |
| 5 | 右转 90 度 | `turnCompleted == 3` | `stageFlag++`，停车 |
| 6 | 前进 760，目标 yaw 为初始 yaw - 90 | `encoderFlag == 4` | `stageFlag++`，停车 |
| 7 | 右转 90 度 | `turnCompleted == 4` | `stageFlag++` |
| 8 | 前进 280，目标 yaw 为初始 yaw - 180 | `encoderFlag == 5` | 发送 `Host_Send('2', ...)`，停车，任务结束 |

### assignment4

`assignment4()` 当前逻辑与 `assignment3()` 基本一致。差异是 `assignment3` 的 `case 5` 在转向完成后调用了 `stop()`，`assignment4` 的对应阶段没有显式 `stop()`；不过 `Right_Turn()` 完成阶段内部本身会把左右电机置 0。

### assignment5

`assignment5()` 前半段大体沿用 `assignment1/2` 的路线，但加入了上位机等待和往返逻辑：

| 阶段 | 动作 | 完成条件 | 完成后 |
| --- | --- | --- | --- |
| 0 | 前进 850，目标 yaw 为初始 yaw | `encoderFlag == 1` | 进入阶段 1，停车 |
| 1 | 右转 90 度 | `turnCompleted == 1` | 进入阶段 2 |
| 2 | 前进 140，目标 yaw 为初始 yaw - 90 | `encoderFlag == 2` | 进入阶段 3，停车 |
| 3 | 右转 90 度 | `turnCompleted == 2` | 进入阶段 4 |
| 4 | 前进 380，目标 yaw 为初始 yaw - 180 | `encoderFlag == 3` | 发送 `Host_Send('0', ...)`，停车，等待上位机变量 `'6'`，再进入阶段 5 |
| 5 | 前进 570，目标 yaw 为初始 yaw - 180 | `encoderFlag == 4` | 进入阶段 6，停车 |
| 6 | 右转 90 度 | `turnCompleted == 3` | 进入阶段 7 |
| 7 | 前进 650，目标 yaw 为初始 yaw + 90 | `encoderFlag == 5` | 进入阶段 8，停车 |
| 8 | 右转 90 度 | `turnCompleted == 4` | 进入阶段 9 |
| 9 | 后退 230，目标 yaw 为初始 yaw - 180 | `DistanceControlWithYaw()` 返回 `1` | 发送 `Host_Send('1', ...)`，进入阶段 10 |
| 10 | 若收到上位机 `'5'` 则延时 4 秒；然后前进 830，目标 yaw 为初始 yaw | 距离完成 | 进入阶段 11，停车 |
| 11 | 若收到上位机 `'5'` 则延时 4 秒；然后后退 830，目标 yaw 为初始 yaw - 180 | 距离完成 | `stageFlag--` 回到阶段 10，停车 |

阶段 10 和 11 会形成前进/后退循环，除非外部改变 `stageFlag` 或 `assignmentFlag`。

### assignment6 / assignment7

两个函数完全一致：

- 左右电机都以方向 `1`、速度 `550` 持续运行。
- 没有距离完成判断，也没有主动停车阶段。

## 四、发现的问题和风险

1. `assignment1()` 的 `case 9` 缺少 `break`
   - 位置：`AssignmentChoose/assignment.c:183` 到 `AssignmentChoose/assignment.c:190`。
   - 影响：执行阶段 9 时会直接贯穿到 `case 10`。因为 `DistanceControlWithYaw()` 内部只有一套共享距离控制状态，阶段 9 的后退距离和阶段 10 的前进距离可能在同一次调用中互相干扰，导致方向、目标距离、`encoderFlag` 推进都异常。

2. `assignment2()` 的 `case 9` 同样缺少 `break`
   - 位置：`AssignmentChoose/assignment.c:276` 到 `AssignmentChoose/assignment.c:283`。
   - 影响同上。

3. `assignment5()` 阶段 4 等待上位机 `'6'` 的判断顺序可能导致死等
   - 位置：`AssignmentChoose/assignment.c:498`。
   - 当前条件是 `while (!(g_Host_Var1 == '6' && Host_Receive_Process()))`。
   - C 语言 `&&` 会短路求值：如果 `g_Host_Var1` 当前不是 `'6'`，`Host_Receive_Process()` 不会被调用，新收到的数据也不会被解析到 `g_Host_Var1`。因此如果进入循环时 `g_Host_Var1` 不是 `'6'`，很可能永远等不到 `'6'`。

4. `assignment5()` 阶段 4 使用阻塞 `while`
   - 位置：`AssignmentChoose/assignment.c:498` 到 `AssignmentChoose/assignment.c:500`。
   - 影响：阻塞期间主循环不再刷新 OLED、不再执行任务状态机，也不会运行主循环里的传感器刷新逻辑。虽然中断仍可运行，且车已经 `stop()`，但这和其他阶段的非阻塞状态机风格不一致。

5. `AssignmentChoose()` 在选择任务 0 时会上报一个可疑字符
   - 位置：`AssignmentChoose/assignment.c:85`。
   - 当 `assignmentFlag == 0` 时，`'a' + assignmentFlag - 1` 等于字符 `` ` ``，不是 `a~g`。
   - 如果任务 0 是合法空闲任务，上位机可能收到意料之外的任务码；如果任务 0 不应该被选择，选择范围可能需要调整。

6. `zeroparameter()` 清零不完整，可能影响任务重跑
   - 位置：`AssignmentChoose/assignment.c:596` 起。
   - 当前清了 `CrossingFlag`、转向相关标志、`assignmentFlag`、yaw 锁定，但没有清 `stageFlag`、`encoderFlag`、`AbsoluateEncoder`。
   - 如果这个函数未来用于任务复位/重新选题，任务可能不会从阶段 0 重新开始，距离控制也可能继承上一次残留状态。

7. 转向完成判断没有做角度环绕误差归一化
   - 位置：`TURN/turn.c:43` 附近、`TURN/turn.c:105` 附近。
   - 当前判断类似 `fabsf(wit_data.yaw - Target_Dir) < 3.0f`。
   - 如果目标角在 `-180/180` 边界附近，比如当前 yaw 是 `179`，目标 yaw 是 `-179`，直接相减会得到约 `358`，实际角度误差只有约 `2`。这种情况下可能出现转向完成判断失败。

8. `assignment1/assignment2`、`assignment3/assignment4` 大量重复
   - 这不是立即运行错误，但维护风险较高。
   - 例如 `assignment1` 和 `assignment2` 的漏 `break` 是成对出现的；后续调距离或发上位机命令时，也容易只改了一份。

## 五、建议的修改方向

这些只是建议，未改源码，需你确认后再动：

1. 给 `assignment1()` 和 `assignment2()` 的 `case 9` 末尾补 `break`。
2. 把 `assignment5()` 阶段 4 的等待条件改成先调用 `Host_Receive_Process()`，再判断 `g_Host_Var1`。
3. 如果希望保持全局非阻塞风格，把 `assignment5()` 阶段 4 的等待上位机响应拆成单独阶段，而不是 `while` 死等。
4. 明确任务 0 是否允许被选择；如果允许，给上位机发送专门的空闲码；如果不允许，把选择范围改为 `1~7`。
5. 如果 `zeroparameter()` 用于重新开始任务，建议同时清 `stageFlag`、`encoderFlag`、`AbsoluateEncoder`，并视情况清距离/转向内部状态。
6. 转向判断建议改为归一化后的角度误差判断，避免 `-180/180` 边界问题。

