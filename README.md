# RF206S 履带车双电机控制器

基于 RoboMaster 开发板 C 型（STM32F407）和 RF206S 接收机的双履带遥控程序。接收机通过 W.BUS 输入油门、转向和 SA 使能信号，开发板通过两组 20 kHz PWM 控制两块 BTS7960 电机驱动。

## 当前功能

- 左摇杆控制前进/后退，右摇杆控制差速转向。
- SA 必须先处于禁止侧，再拨到允许侧才能解锁电机。
- W.BUS 失联、丢帧、Failsafe 或 SA 禁止时，四路电机 PWM 立即归零。
- 最大输出占空比限制为 50%。
- 每 10 ms 只增加 5 个指令单位，约 2 秒从 0 平滑爬升到最大指令。
- 换向时先逐步降到 0，再启用相反方向 PWM。
- USART6 以 115200 波特率输出遥控状态、左右命令、PWM 和左编码器数据，可配合 `tools` 目录中的串口仪表查看。

## PWM 接线

| 车辆侧 | C 板 PWM | MCU 引脚/定时器 | BTS7960 |
|---|---|---|---|
| 左电机方向 1 | C3 | PE13 / TIM1_CH3 | RPWM |
| 左电机方向 2 | C4 | PE14 / TIM1_CH4 | LPWM |
| 右电机方向 1 | C5 | PC6 / TIM8_CH1 | RPWM |
| 右电机方向 2 | C6 | PI6 / TIM8_CH2 | LPWM |

当前车辆前进方向使用左侧 C3、右侧 C6；这是由 `Src/main.c` 中的电机极性设置决定的。两块 BTS7960 的 VCC、R_EN、L_EN 接 5 V，逻辑 GND、C 板 GND 与电池 B- 必须共地。电机电源 B+/B- 接 12 V，并使用合适的支路保险。

## 遥控器与安全逻辑

- W.BUS：USART3 RX，100000 baud、9-bit（含偶校验位）、Even parity、2 stop bits。
- 油门通道：W.BUS channel 2。
- 转向通道：W.BUS channel 0。
- SA 使能：W.BUS channel 4。
- 有效帧超时：100 ms。
- 摇杆死区：中心值 1024，死区 ±80。

上电后即使 SA 已在允许侧也不会直接驱动。必须先将 SA 放到禁止侧，让程序记录安全状态，再拨到允许侧解锁。

## 左编码器

左编码器 A/B 相接 PF0/PF1，使用 EXTI 双边沿中断进行 x4 正交计数。当前串口输出位置、每秒计数和非法跳变计数；换算 RPM 前需要确认每圈编码器计数。

## 编译

需要 CMake、Ninja、GNU Arm Embedded Toolchain 和 STM32CubeMX 生成的 HAL 驱动。

```powershell
cmake --preset Debug
cmake --build --preset Debug
```

输出位于 `build/Debug/`：

- `rf206s_motor_test.elf`
- `rf206s_motor_test.hex`
- `rf206s_motor_test.bin`

## 烧录

连接 ST-Link，保持电机 12 V 断开，然后运行：

```powershell
STM32_Programmer_CLI -c port=SWD mode=UR reset=HWrst freq=1000 `
  -w build/Debug/rf206s_motor_test.elf -v -rst
```

首次带电测试必须架空履带、摇杆回中、SA 位于禁止侧，并准备随时断开 12 V。

## 目录

- `Src/`、`Inc/`：应用和中断代码。
- `Drivers/`：STM32 HAL/CMSIS。
- `cmake/`、`CMakeLists.txt`、`CMakePresets.json`：构建配置。
- `rf206s_motor_test.ioc`：STM32CubeMX 工程。
- `tools/`：串口仪表及辅助脚本。

