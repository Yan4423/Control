# 代码地图与维护

## 当前构建路径

入口为 `Src/main.c`。CMake源码清单在 `cmake/stm32cubemx/CMakeLists.txt`，不是扫描所有.c文件。

| 文件/函数 | 作用 |
|---|---|
| `main.c / wbus_start_receive` | USART3 ReceiveToIdle DMA，关闭半传输中断 |
| `wbus_store_frame / wbus_decode_channels` | 25字节帧、0x0F头/0x00尾、16个11-bit通道和故障标志 |
| `drive_control_update` | 每10ms读取输入、SA状态机、左右差速混控、限幅与渐变 |
| `drive_slew` | 每次±5，换向先把目标临时设0 |
| `drive_write_motor` | 按各自定时器ARR+1算CCR，先清另一个方向，再写有效方向 |
| `drive_force_stop` | 清TIM1_CH1–4与TIM8_CH1–2并重置命令 |
| `left_encoder_*` | PF0/PF1双边沿正交计数，100ms速度采样 |
| `debug_uart_send_snapshot` | 输出17字段CSV |
| `Src/usart.c`、`dma.c`、`stm32f4xx_it.c` | 串口、DMA配置和IRQ转发 |
| `Src/tim.c` | TIM1/TIM8生成初始化、GPIO复用 |
| `tools/serial_dashboard.ps1` | Windows读取与显示；不发送控制指令 |
| `applications/`、`bsp/` | 旧DJI参考文件，不加入当前构建；不是RF206S输入入口 |

## 关键参数

均在 `Src/main.c` 的USER CODE宏定义区：

| 宏 | 当前值 | 修改含义 |
|---|---|---|
| DRIVE_CONTROL_PERIOD_MS | 10 | 控制周期 |
| DRIVE_COMMAND_FULL_SCALE | 1000 | 归一化满指令 |
| DRIVE_SLEW_STEP | 5 | 每周期最大指令变化；0到满量程约1000/5×10ms=2s |
| DRIVE_MAX_DUTY_PERCENT | 50 | 电机输出上限，不是速度闭环目标 |
| DRIVE_LEFT_MOTOR_POLARITY / RIGHT | +1 / -1 | 镜像电机极性；前推分别使用C3/C6 |
| DRIVE_STICK_CENTER / DEADZONE / SPAN | 1024 / 80 / 670 | 遥控中心、死区、标定幅度 |
| DRIVE_THROTTLE/TURN/ENABLE_CHANNEL | 2 / 0 / 4 | 从0起的数组索引，对应CH3/CH1/CH5 |
| WBUS_LINK_TIMEOUT_MS | 100 | 有效帧超时阈值 |
| DEBUG_UART_PERIOD_MS | 100 | 遥测周期 |

## CubeMX与手工修改

1. `.ioc` 与 `tim.c` 的TIM8仍是PSC=167、ARR=19999的50Hz舵机设置；`main.c` 初始化后、启动PWM前将TIM8改为PSC=0、ARR=8399、CCR=0，才用于20kHz电机输出。保持该启动顺序。
2. 某些外设GPIO和USART6的手工调整已经存在。重生成前后至少对比TIM1/TIM8引脚、上拉、初始脉冲、USART6、PF0/PF1 EXTI、USART3/DMA中断，以及主函数初始化调用列表。
3. HSE=12MHz、SYSCLK=168MHz、APB2定时器时钟168MHz，因此168MHz/8400=20kHz。若改系统时钟，重新计算两组PWM，不能只修改ARR后继续使用旧百分比。
4. 只在分支上重生成，检查Git diff，干净构建，然后在断开电机动力时测量。不要以“CubeMX没有报错”代替外设功能验证。

## 已知限制及优先待办

| 优先级 | 事项 | 当前事实/建议验收 |
|---|---|---|
| P0 | 中位解锁互锁 | 目前SA允许带油门解锁。需要时由接手人新增并测试，避免默认为已有 |
| P0 | HAL错误处理/看门狗 | `Error_Handler` 为空，没有统一的故障锁止；没有配置独立看门狗。GPIO/PWM清零不能保证CPU失控时停机 |
| P0 | 失联/Failsafe验收 | 100ms是软件阈值，实际停止延迟还受10ms循环、串口阻塞与接收机输出策略影响，需测量而非承诺硬实时 |
| P1 | W.BUS并发审查 | `now` 在关闭中断前获取；若其间新帧时间戳更新，无符号差可能短暂被判为超时。此项为代码审查风险，未实机复现；UART错误回调与控制写入也需检查 |
| P1 | 双编码器和速度闭环 | 右侧尚无实现；左侧每圈计数、方向未完成统一标定 |
| P1 | CubeMX与运行配置统一 | 将TIM8电机配置和手工外设调整纳入可重生成流程，需回归验证 |
| P1 | 电流/换向保护 | 没有电流检测/限流/堵转保护；软件过零不保证机械静止 |
| P2 | 蜂鸣器 | 独立补丁未并入，见存档说明 |
| P2 | 历史示例 | `applications/`、`bsp/`存在旧中文编码；只作为参考，清理前保留来源与许可 |

本次交接保持遥控算法不变，没有顺带实现这些待办。安全或控制功能修改应另做提交，并在 [交接验收表](HANDOVER.md#接手验收记录待接手人填写) 留测试证据。

## 日常提交流程

接手人在自己的克隆中配置自己的作者信息，新建分支、修改、构建、记录测试并发PR。保留Yan4423原作者和第三方标记；不要重写已有历史，也不要共享原作者账户。涉及引脚、PWM频率、通道映射或CSV顺序时，同步更新接线表、协议和仪表。
