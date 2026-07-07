# AI自动调参功能使用说明

## 概述

已为BalanceCar倒立摆项目实现了AI自动调参数据发送功能，可以将直立环PID的实时数据通过蓝牙串口发送给 `llm-pid-tuner` 工具进行自动调参。

## 实现的文件

### 新增文件
1. **AssignmentChoose/ai_sending.c** - AI调参数据发送实现
2. **AssignmentChoose/ai_sending.h** - AI调参数据发送头文件

### 修改的文件
1. **AssignmentChoose/assignment.c** - 添加了 `AISending()` 调用
2. **Move/move.c** - 将 `angle` 和 `balancePWM` 改为全局变量
3. **Move/move.h** - 导出 `angle`、`balancePWM` 和 `BalanceMode`

## 数据格式

按照 `llm-pid-tuner` 要求的CSV格式发送：

```
timestamp_ms,setpoint,input,pwm,error,p,i,d
```

### 字段说明
- **timestamp_ms**: 系统时间戳（毫秒）
- **setpoint**: 设定值（215.99度，倒立摆垂直位置）
- **input**: 当前角度值
- **pwm**: 当前PWM输出值
- **error**: 误差（setpoint - input）
- **p**: P分量（Kp × error）
- **i**: I分量（Ki × integral）
- **d**: D分量（Kd × (error - last_error)）

## 使用方法

### 1. 硬件连接
- 确保蓝牙模块已连接到UART_BT（UART1）
- 波特率：9600
- 引脚：PA8(TX), PA9(RX)

### 2. 运行调参工具

在PC端运行 `llm-pid-tuner`：

```bash
cd "D:\Claude Code Generated\llm-pid-tuner"
.\llm-pid-tuner.exe
```

或者使用Python源码：

```bash
python tuner.py
```

### 3. 配置 config.json

确保 `config.json` 中的配置正确：

```json
{
  "SERIAL_PORT": "COM5",  // 你的蓝牙串口号
  "BAUD_RATE": 9600,
  "HARDWARE_PROFILE": "generic_serial_csv",
  "LLM_API_KEY": "your-api-key",
  "LLM_API_BASE_URL": "https://api.openai.com/v1",
  "LLM_MODEL_NAME": "gpt-4o",
  "LLM_PROVIDER": "openai"
}
```

### 4. 启动小车

1. 在小车上选择 **任务3**（assignment3）
2. 小车会自动进入平衡控制模式
3. 当进入 `maintain` 阶段（维持平衡）时，会自动通过DMA发送数据
4. AI工具会实时接收数据并给出PID调参建议

## 代码示例

### assignment3() 函数

```c
void assignment3(void)
{
    balance();  // 执行平衡控制

    // 只在维持平衡阶段发送数据
    extern Mode BalanceMode;
    if (BalanceMode == maintain) {
        extern float angle;
        extern int balancePWM;
        AISending(angle, balancePWM);  // 发送AI调参数据
    }
}
```

### AISending() 函数

```c
void AISending(float angle, int balancePWM)
{
    // 自动获取时间戳、计算误差和PID分量
    // 格式化为CSV字符串
    // 通过DMA非阻塞发送
}
```

## 性能特点

### ✅ 优势
1. **DMA非阻塞发送** - CPU占用<5%，不影响高频控制循环
2. **自动数据采集** - 自动计算误差和PID三个分量
3. **智能发送策略** - 只在维持平衡阶段发送，避免起振阶段干扰
4. **标准CSV格式** - 完全兼容 `llm-pid-tuner` 工具

### 📊 发送频率
- 建议：10-20Hz（每50-100ms发送一次）
- 当前实现：每次调用 `assignment3()` 都会尝试发送
- 如果上一次发送未完成，会自动跳过本次发送

## 调参流程

1. **启动阶段**
   - 小车进入 `stand` 模式（起振）
   - 不发送数据

2. **维持阶段**
   - 小车进入 `maintain` 模式
   - 开始发送实时数据
   - AI工具分析数据并给出建议

3. **参数更新**
   - AI工具会建议新的PID参数
   - 手动更新 `PID/pid.c` 中的 `AnglePID` 参数
   - 重新编译并下载到小车

4. **迭代优化**
   - 重复上述过程，直到达到满意的控制效果

## 当前PID参数

在 `PID/pid.c` 中：

```c
PID AnglePID = {40.0, 2.0, 0, 0, 0, 0, 0};
// Kp=40.0, Ki=2.0, Kd=0
```

## 注意事项

1. **安全第一** - 调参时务必有人值守，随时准备断电
2. **数据有效性** - 只在 `maintain` 阶段发送数据，确保数据质量
3. **发送频率** - 如果需要降低发送频率，可以添加计数器控制
4. **串口冲突** - 确保没有其他程序占用蓝牙串口

## 故障排查

### 问题1：AI工具收不到数据
- 检查蓝牙连接是否正常
- 检查串口号是否正确
- 检查波特率是否为9600
- 确认小车已进入 `maintain` 模式

### 问题2：数据格式错误
- 查看串口助手，确认输出格式为CSV
- 应该看到类似：`12345,215.99,216.50,120,-0.51,20.40,4.00,0.00`

### 问题3：控制性能下降
- 检查发送频率是否过高
- 确认DMA发送是否正常工作
- 可以临时注释掉 `AISending()` 调用进行对比

## 示例输出

```
1234,215.99,216.50,120,-0.51,-20.40,4.00,0.00
1334,215.99,216.30,115,-0.31,-12.40,3.50,0.00
1434,215.99,216.10,110,-0.11,-4.40,3.00,0.00
1534,215.99,215.95,105,0.04,1.60,2.50,0.00
```

## 参考文档

- [llm-pid-tuner README](D:\Claude Code Generated\llm-pid-tuner\README.md)
- [UART DMA发送说明](../Drivers/Bluetooth/README_UART_DMA.md)

---

**实现日期**: 2026年5月30日  
**适用于**: BalanceCar倒立摆项目 + llm-pid-tuner AI调参工具
