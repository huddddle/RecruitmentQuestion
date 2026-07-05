# RecruitmentQuestion

基于 TI MSPM0G3507 的智能小车控制工程，使用 Code Composer Studio / Theia 工程结构组织代码。项目包含任务选择、巡线行驶、角度转向、速度闭环、距离控制、OLED 显示、蓝牙通信、K230 串口通信、IMU 姿态读取和超声波测距等模块。

## 硬件与工具链

- 主控：TI MSPM0G3507
- IDE：TI Code Composer Studio / CCS Theia
- 编译器：TI Arm Clang `TICLANG_4.0.3.LTS`
- SDK：MSPM0 SDK `2.5.0.05`
- SysConfig：`1.24.0`
- 调试器：XDS110，目标配置文件为 `targetConfigs/MSPM0G3507.ccxml`

以上版本来自当前 `.cproject` 工程配置。若本地版本不同，导入工程后需要检查 SDK、SysConfig 和编译器路径。

## 目录结构

```text
.
├── main.c / main.h                 # 程序入口与主循环
├── global.h                        # 全局状态与共享结构体声明
├── mspm0-modules.syscfg            # TI SysConfig 外设配置
├── AssignmentChoose/               # 任务选择与任务流程
├── Move/                           # 巡线、停车、前进/后退等运动逻辑
├── TURN/                           # 基于航向角的非阻塞转向控制
├── SPEED/                          # 编码器测速、速度 PID、距离控制
├── PID/                            # PID 基础算法
├── SERVO/                          # 舵机控制
├── UART/                           # K230 串口通信
├── INTERRUPT/                      # 中断与系统节拍相关代码
├── Drivers/
│   ├── Motor/                      # 电机驱动
│   ├── Bluetooth/                  # 蓝牙串口通信
│   ├── WIT/                        # WIT 姿态传感器
│   ├── MPU6050/                    # MPU6050 / DMP 驱动
│   ├── Ultrasonic_GPIO/            # GPIO 超声波测距
│   ├── Ultrasonic_Capture/         # 捕获方式超声波测距
│   ├── OLED_*                      # 多种 OLED I2C/SPI 驱动
│   └── MSPM0/                      # 时钟、中断等底层封装
├── UserLib/Chassis/                # C++ 底盘控制库实验封装
└── targetConfigs/                  # 调试目标配置
```

`Debug/` 目录为 IDE 生成的编译产物和索引缓存，不作为源码提交。

## 功能概览

- 通过按键和 OLED 选择任务编号与图形类型。
- 使用红外巡线模块进行基础循迹和路口判断。
- 使用 WIT / IMU 航向角实现左转、右转和掉头控制。
- 通过编码器读取车轮速度，并使用增量式 PID 进行速度闭环。
- 支持按目标编码器距离进行距离闭环控制。
- 支持 OLED 状态显示、蓝牙调试输出、蜂鸣器和 LED 提示。
- 预留 K230 视觉数据串口解析接口。

## 导入与编译

1. 安装 Code Composer Studio / CCS Theia，并安装 MSPM0 SDK 与 SysConfig。
2. 打开 IDE，选择导入已有工程。
3. 选择本仓库根目录 `RecruitmentQuestion`。
4. 确认工程使用的芯片为 `MSPM0G3507`，调试连接为 XDS110。
5. 打开 `mspm0-modules.syscfg`，确认 SysConfig 能正常加载并生成配置。
6. 执行 Build，生成的文件会输出到 `Debug/` 目录。
7. 连接开发板后使用 `targetConfigs/MSPM0G3507.ccxml` 下载和调试。

## 运行流程

程序入口在 `main.c`：

1. 初始化 SysConfig、系统节拍、OLED、测速、超声波等模块。
2. 初始化电机输出并进入任务选择流程 `AssignmentChoose()`。
3. 主循环中刷新 OLED 状态，包括转向完成次数、转向标志和当前 yaw。
4. 根据 `assignmentFlag` 调用对应任务函数执行巡线、转向、停车等动作。

## 开发说明

- 新增外设时优先在 `mspm0-modules.syscfg` 中配置引脚和外设实例。
- 新增模块时建议按现有目录风格拆分 `.c/.h`，并在 `.cproject` 中加入 include path。
- 转向逻辑应保持非阻塞风格，避免在主循环中长时间 `delay`。
- 调试构建产物、对象文件、map/out 文件和 clangd 缓存已由 `.gitignore` 排除。

## 相关文档

- `TURN/TURN_REFACTOR.md`：转向模块重构说明。
- `UserLib/Chassis/CHASSIS_USAGE.md`：底盘控制库实验封装使用说明。
