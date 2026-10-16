# RF206S 履带车控制器

RoboMaster 开发板 C 型（STM32F407IGHx）通过 RF206S / W.BUS 接收遥控指令，以两组 PWM 控制 BTS7960 双履带电机。仓库所有者与原项目作者为 **[Yan4423](https://github.com/Yan4423)**；本次交接保留所有权、管理权限和原提交历史。

> 本仓库提供可构建的实验固件，不等于整车验收记录。当前板内固件版本需要接手人重新核验。

## 给接手人的阅读顺序

1. [接线与接口](docs/WIRING.md)：C3–C6、W.BUS、串口、编码器与供电。
2. [编译和烧录](docs/BUILD_AND_FLASH.md)：从全新电脑克隆开始，不依赖原作者的磁盘路径。
3. [遥控操作与排障](docs/OPERATION.md)：解锁、试车、仪表、常见故障。
4. [代码地图与维护](docs/DEVELOPMENT.md)：参数、控制流程、CubeMX注意事项、后续工作。
5. [串口数据协议](docs/TELEMETRY.md)、[作者与仓库权限](AUTHORS.md)、[第三方来源](THIRD_PARTY_NOTICES.md)。

## 当前主线行为

| 项目 | 当前代码 |
|---|---|
| 输入 | RF206S W.BUS，USART3 RX / PC11，100000 baud，8E2在线路上 / HAL 9-bit含校验 |
| 遥控通道 | 油门 CH3、转向 CH1、SA CH5（代码数组索引分别为2、0、4） |
| 左电机 | C3 / PE13 / TIM1_CH3 → RPWM；C4 / PE14 / TIM1_CH4 → LPWM |
| 右电机 | C5 / PC6 / TIM8_CH1 → RPWM；C6 / PI6 / TIM8_CH2 → LPWM |
| 前推对应电气输出 | C3、C6有效，C4、C5为0；实际车辆前进方向仍需架空复核 |
| PWM | 20kHz，最大50%，CCR满指令为4200 / 周期8400 |
| 渐变 | 每10ms最多变化5/1000，0到最大指令约2秒；换向先降到0 |
| 解锁 | 先收到SA禁止，再拨允许；**没有摇杆回中才能解锁的校验** |
| 停机 | SA禁止或接收机故障标志在控制循环处理时清零；有效帧超时阈值100ms |
| 调试 | USART6 TX / PG14，115200 8N1，每100ms输出CSV |
| 编码器 | 仅左侧PF0/PF1的x4计数；没有右侧解码或速度闭环 |

## 快速开始

工具加入PATH后，在仓库根目录运行：

```powershell
cmake --preset Debug
cmake --build --preset Debug
powershell.exe -NoProfile -File tools/check_environment.ps1
```

已附带HAL/CMSIS源码，无需额外下载遥控器库。输出位于 `build/Debug/rf206s_motor_test.{elf,hex,bin}`。烧录与实车接电步骤见文档；本次仓库整理不会自动烧录或驱动电机。

## 交付状态

- 保留首次提交 `d037e80` 的遥控算法、约2秒渐变与电机极性。
- 修正串口仪表的C3–C6标记、PWM纵轴与时间轴；去除IDE中原电脑的固定路径。
- 原工作目录中未提交的蜂鸣器功能保存在 [待合并补丁](archive/local-unmerged/README.md)，没有自动并入主线。
- [本次构建验证](docs/VALIDATION.md) 记录编译器版本、输出校验值和验证边界；本次未进行实机验收。

## 目录

| 路径 | 用途 |
|---|---|
| `Src/`、`Inc/` | 实际应用、外设初始化与中断 |
| `Drivers/` | STM32 HAL/CMSIS与原许可 |
| `cmake/`、根目录CMake文件 | 显式源码清单与交叉编译 |
| `rf206s_motor_test.ioc` | CubeMX配置，包含历史设置，重生成前读维护文档 |
| `tools/` | Windows串口仪表、环境检查 |
| `applications/`、`bsp/` | 保留的DJI旧示例；**当前CMake不编译它们** |
| `docs/`、`archive/` | 交接资料和未合并改动存档 |

接手人以自己的GitHub身份提交后续修改。仓库继续由Yan4423持有；访问权限交接见 [AUTHORS.md](AUTHORS.md)。
