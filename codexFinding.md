

## assignment1 状态机改成 switch 的建议

你的 `assignment1()` 现在已经是按主循环反复调用推进的状态机，所以外层 `stageFlag` 可以改成 `switch (stageFlag)`，但不要在任何 `case` 里加 `while` 或等待循环。每次进入 `assignment1()` 只执行当前阶段的一小段逻辑，然后马上 `break` 返回，这样不会卡住其他任务。

另外，`task1_yaw_locked` 当前是 yaw 初始化标志变量，不是函数。建议保留它的变量含义，把 `assignment1()` 开头那段“只锁定一次初始 yaw”的代码抽成 `TaskYawInitialization()` 函数，函数内容不变，只是换成函数封装。

### 1. 在头文件增加函数声明

位置：`AssignmentChoose/assignment.h`

```c
void TaskYawInitialization(void);
```

### 2. 在 assignment.c 增加函数定义

位置：`AssignmentChoose/assignment.c`，建议放在 `Task1TargetYaw()` 后面、`assignment1()` 前面。

```c
void TaskYawInitialization(void)
{
  if (!task1_yaw_locked)
  {
    task1_base_yaw = wit_data.yaw;
    task1_yaw_locked = 1;
  }
}
```

### 3. assignment1 开头改为调用初始化函数

把原来的：

```c
if (!task1_yaw_locked)
{
  task1_base_yaw = wit_data.yaw;
  task1_yaw_locked = 1;
}
```

替换为：

```c
TaskYawInitialization();
```

### 4. assignment1 外层状态机改为 switch

推荐写法如下。重点是每个 `case` 都有 `break`，并且没有 `while`，所以不会阻塞主循环里其他任务。

```c
void assignment1(void)
{
  TaskYawInitialization();

  switch (stageFlag)
  {
    case 0:
      if (turnCompleted == 0)
      {
        Left_Turn(90);
      }
      if (turnCompleted == 1) stageFlag++;
      break;

    case 1:
      DistanceControlWithYaw(590, 1, Task1TargetYaw(90.0f));
      if (encoderFlag == 1)
      {
        stageFlag = 2;
        stop();
      }
      break;

    case 2:
      Right_Turn(90);
      if (turnCompleted == 2) stageFlag++;
      break;

    case 3:
      DistanceControlWithYaw(200, 0, Task1TargetYaw(0.0f));
      if (encoderFlag == 2)
      {
        Host_Send('0', "000000", '0');
        stageFlag++;
        stop();
      }
      break;

    case 4:
      DistanceControlWithYaw(980, 1, Task1TargetYaw(0.0f));
      if (encoderFlag == 3)
      {
        Host_Send('1', "000000", '0');
        stageFlag++;
        stop();
      }
      break;

    case 5:
      Right_Turn(90);
      if (turnCompleted == 3) stageFlag++;
      break;

    case 6:
      DistanceControlWithYaw(760, 1, Task1TargetYaw(-90.0f));
      if (encoderFlag == 4)
      {
        stageFlag++;
        stop();
      }
      break;

    case 7:
      Right_Turn(90);
      if (turnCompleted == 4) stageFlag++;
      break;

    case 8:
      DistanceControlWithYaw(280, 1, Task1TargetYaw(-180.0f));
      if (encoderFlag == 5)
      {
        Host_Send('2', "000000", '0');
        stageFlag++;
        stop();
      }
      break;

    default:
      break;
  }
}
```

### 5. 验证重点

1. 编译确认 `TaskYawInitialization()` 的声明、定义、调用一致。
2. 确认 `assignment1()` 每次被主循环调用时只执行当前 `stageFlag` 对应的一段逻辑。
3. 串口或 OLED 观察 `stageFlag` 是否能按 0 到 9 推进。
4. 检查转弯阶段仍由 `turnCompleted` 推进，直行阶段仍由 `encoderFlag` 推进。
5. 确认 `Host_Send()` 和 `stop()` 的触发阶段没有改变。
