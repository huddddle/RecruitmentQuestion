# 循迹与角度(ADC)驱动模块 使用与移植手册 

## 1. 原理介绍

本系统针对双轮平衡车/智能车移植了以下两项关键外围传感器驱动：
- **I2C循迹模块 (TrackingModuleIIC)**:
    - 硬件使用了 PCA9555 IO 扩展器，这颗芯片通过 I2C 接口将 12 路红外光电反射传感器的状态汇集起来传输给 MCU。
    - MCU端采用硬件 I2C1 与其实时通信。内部会解算 I2C 获取的 16位 原始数据（gTrackRawState12），转化为 12 位的 0/1 状态数组 TrkI2C_x[12]。
    - 并且引入了位置权重（gTrackWeights），能够直接反馈出车体的横向偏差 TrkI2C_Tracking_Sum。

- **角度传感器模块 (ADC_Angle)**:
    - 原生使用了 ADC0_CH1 (对应引脚 PA15)。
    - 为了避免原工程中定时器（Timer）触发频率冲突和复杂的 DMA 搬运配置引起的阻塞死机问题，**新版驱动重构为“非阻塞纯软件连续获取模式”**。
    - MCU直接向ADC下发指令 DL_ADC12_startConversion(ADC12_Angle_INST)，硬件在后台会自动连续转换，主程序可以随时读取最新的结果 DL_ADC12_getMemResult(...) 并换算为物理角度（0~333.3°）。

---

## 2. 如何使用驱动

### 2.1 引入所需头文件
在你的应用代码（如 main.c 或 任务函数）顶部引入驱动头文件：
\\\c
#include "trackingiic.h"
#include "adc_angle.h"
\\\

### 2.2 外设初始化 (必须在 SysCfg_init 初始化后调用)
在进入主循环 while(1) 之前，需要激活外设：
\\\c
// 初始化循迹 (大部分 I2C 与引脚初始化在 Syscfg 已经完成)
// 只需初始化角度 ADC
ADC_Angle_Init();
\\\

### 2.3 在主循环中获取实时数据
这两个模块现在都被设计为**非阻塞的快速读取方式**：

\\\c
while (1) {
    // 1. 获取循迹数据: 更新底层的数据数组和误差值。
    // 该函数执行非常快，如果 I2C 断开连接返回 false。
    trackSensorUpdate();
    
    // 你可以直接访问以下全局变量进行控制业务：
    // TrkI2C_x[0] ~ TrkI2C_x[11]     : 获取从左到右 12 个探头是黑线(1) 还是白底(0)
    // TrkI2C_Tracking_Sum            : 直接获取加权计算后的位置偏差
    // TrkI2C_IrSensorNumber          : 当前踩上黑线的探头总数

    // 2. 获取角度数据
    // 该函数不涉及等待转换，直接读取邮箱内的最新值并浮点运算
    float currentAngle = ADC_Angle_GetDegree();
    
    // ... 你的 PID 或 其他控制逻辑 ...
}
\\\

---

## 3. 防阻死与避坑指南

### 防卡死说明及注意点：
1. **I2C 拔插 / 接触不良 防卡死：**
   	rackingiic.c 内部的 I2C 轮询带有超时检测 (TRACK_SENSOR_I2C_TIMEOUT_LOOPS)。如果循迹模块排线松动或者断开，它不会死循环卡死整个单片机跑飞，而是会让 	rackSensorUpdate() 返回失败，并将 gTrackSensorOnline 置为 alse。
   **建议**: 在你的行车逻辑中加入判断，如果 gTrackSensorOnline == false，应当停车并提示硬件掉线。
2. **ADC 读取阻塞死机：**
   原版的代码由于绑定了外部 TIMER_Angle 与 DMA_CH0，如果这些外设意外停掉或者修改了通道，MCU 会在等待 DMA 信号时彻底假死。
   新的 dc_angle.c 已经彻底移除了 DMA 依赖，转为最安全的 DL_ADC12_getMemResult 内存直读。这是纯净的数据读拿操作，**绝不会卡死主程序**。
3. **命名冲突：**
   若以后要结合其他传感器，切记引入的数组名字。新版循迹特意将原来的 x[12] 更名为了 TrkI2C_x[12] ，这防止它和 global.h 里用来调参或存放别的数值的变量冲突。使用者一定要用 TrkI2C_x 来写逻辑。
