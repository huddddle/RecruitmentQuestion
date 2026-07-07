# UART DMA配置完成步骤

## 已完成的工作

✅ 1. 创建了DMA发送驱动文件
   - `uart_dma_tx.h` - 驱动头文件
   - `uart_dma_tx.c` - 驱动实现

✅ 2. 更新了蓝牙模块接口
   - `bluetooth.h` - 添加了DMA发送函数声明
   - `bluetooth.c` - 实现了DMA发送函数

✅ 3. 添加了中断处理
   - `INTERRUPT/interrupt.c` - 添加了UART_BT中断处理函数

✅ 4. 更新了SysConfig配置
   - `mspm0-modules.syscfg` - 为UART_BT添加了FIFO和DMA配置

## 需要完成的步骤

### 步骤1：重新生成配置文件

**重要**：修改了`.syscfg`文件后，需要重新生成配置代码。

#### 方法A：使用Code Composer Studio (CCS)
1. 在CCS中打开项目
2. 双击打开 `mspm0-modules.syscfg` 文件
3. 等待SysConfig工具加载
4. 点击保存按钮（或按Ctrl+S）
5. SysConfig会自动重新生成 `ti_msp_dl_config.c` 和 `ti_msp_dl_config.h`

#### 方法B：使用命令行
```bash
cd BalanceCar
# 使用TI SysConfig CLI工具重新生成
sysconfig -o Debug -s "path/to/mspm0_sdk" mspm0-modules.syscfg
```

### 步骤2：验证生成的配置

重新生成后，检查 `Debug/ti_msp_dl_config.h` 文件，应该包含以下新定义：

```c
/* Defines for DMA_CH2 */
#define DMA_CH2_CHAN_ID                                                      (2)
#define UART_BT_INST_DMA_TRIGGER_TX                          (DMA_UART1_TX_TRIG)
```

### 步骤3：添加源文件到项目

确保以下文件已添加到项目编译列表：

- `Drivers/Bluetooth/uart_dma_tx.c`
- `Drivers/Bluetooth/bluetooth.c`（已有，已更新）
- `INTERRUPT/interrupt.c`（已有，已更新）

#### 在CCS中添加文件：
1. 右键点击项目 → Properties
2. Build → ARM Compiler → Include Options
3. 确认包含路径：`${PROJECT_ROOT}/Drivers/Bluetooth`

### 步骤4：在main.c中初始化

在你的 `main.c` 文件中，确保调用初始化函数：

```c
#include "bluetooth.h"

int main(void)
{
    // 系统初始化
    SYSCFG_DL_init();
    
    // 初始化蓝牙DMA发送（必须在SYSCFG_DL_init之后）
    Bluetooth_Init();
    
    // 其他初始化...
    
    while (1) {
        // 主循环
    }
}
```

### 步骤5：在控制循环中使用

参考以下示例在你的控制代码中使用DMA发送：

```c
// 在你的控制循环中
void BalanceControl(void)
{
    static uint32_t send_counter = 0;
    
    // 读取角度传感器
    float angle = GetCurrentAngle();
    
    // PID控制
    float kp = 1.5f, ki = 0.1f, kd = 0.5f;
    ControlMotor(angle, kp, ki, kd);
    
    // 每100次控制循环发送一次数据（降低发送频率）
    if (++send_counter >= 100) {
        send_counter = 0;
        
        // 非阻塞发送，不影响控制循环
        Bluetooth_SendAnglePID_DMA(angle, kp, ki, kd);
    }
}
```

## 编译和测试

### 编译项目

1. 在CCS中：Project → Build Project
2. 检查编译输出，确保没有错误

### 常见编译错误及解决方法

#### 错误1：`DMA_CH2_CHAN_ID` 未定义
**原因**：SysConfig未重新生成配置文件
**解决**：执行步骤1，重新生成配置

#### 错误2：`UART_DMA_TX_IRQHandler` 未定义
**原因**：`uart_dma_tx.c` 未添加到项目
**解决**：执行步骤3，添加源文件到项目

#### 错误3：链接错误 - 多重定义
**原因**：可能有重复的函数定义
**解决**：检查是否有其他地方定义了相同的函数

### 测试步骤

1. **编译通过**
   - 确保项目编译无错误和警告

2. **下载到板子**
   - 使用调试器下载程序到MSPM0G3507

3. **连接蓝牙模块**
   - 确保蓝牙模块连接到PA8(TX)和PA9(RX)
   - 波特率设置为9600

4. **打开串口助手**
   - 连接蓝牙模块
   - 波特率：9600
   - 数据位：8
   - 停止位：1
   - 校验：无

5. **观察输出**
   - 应该看到类似以下格式的数据：
   ```
   Angle=45.50,Kp=1.500,Ki=0.100,Kd=0.500
   Angle=45.52,Kp=1.500,Ki=0.100,Kd=0.500
   ...
   ```

## 性能验证

### 验证CPU占用

使用示波器或逻辑分析仪观察：
- GPIO翻转频率（在控制循环中翻转GPIO）
- 发送数据时GPIO频率应该基本不变（说明CPU占用很低）

### 验证发送完整性

- 长时间运行，检查数据是否有丢失
- 调整发送频率，找到最佳平衡点

## 故障排查清单

- [ ] SysConfig配置已保存并重新生成
- [ ] `ti_msp_dl_config.h` 包含DMA_CH2相关定义
- [ ] `uart_dma_tx.c` 已添加到项目
- [ ] `Bluetooth_Init()` 在main函数中调用
- [ ] UART_BT中断处理函数已添加
- [ ] 蓝牙模块硬件连接正确
- [ ] 波特率配置正确（9600）

## 下一步优化建议

1. **动态调整发送频率**
   - 根据控制性能动态调整数据发送频率

2. **添加数据缓冲队列**
   - 如果需要更高的发送频率，可以实现环形缓冲区

3. **添加错误统计**
   - 统计发送失败次数，用于性能分析

4. **支持多种数据格式**
   - 添加二进制数据发送，减少数据量

## 技术支持

如果遇到问题，请检查：
1. 硬件连接是否正确
2. 配置文件是否重新生成
3. 编译输出中的错误信息
4. 使用示波器检查UART TX引脚是否有输出

参考文档：
- `README_UART_DMA.md` - 详细使用说明
- `uart_dma_example.c` - 代码示例
