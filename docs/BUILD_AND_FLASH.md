# 从新电脑编译与烧录

## 工具与克隆

主构建路径为CMake/GNU Arm，不需要Keil。编译不需要连接C板，也不需要重新运行CubeMX；HAL/CMSIS已在仓库中。

准备Git、CMake（至少3.22）、Ninja、`arm-none-eabi-gcc` / `objcopy`，烧录另需STM32CubeProgrammer与ST-Link驱动。把各自安装目录的bin加入自己电脑的PATH。可用STM32工具包或独立安装，具体验证版本见 [验证记录](VALIDATION.md)。

```powershell
git clone https://github.com/Yan4423/RF206S-Tracked-Vehicle-Control.git
cd RF206S-Tracked-Vehicle-Control
powershell.exe -NoProfile -File tools/check_environment.ps1
cmake --preset Debug
cmake --build --preset Debug
```

环境检查只读、不烧录。私有仓库需要接手人自己的GitHub访问权限。编辑器继承启动时的PATH，安装工具后需重启VS Code/终端。

## 输出与版本

Debug输出在 `build/Debug/`；Release可通过以下命令构建：

```powershell
cmake --preset Release
cmake --build --preset Release
```

每种配置输出 `rf206s_motor_test.elf`、`.hex`、`.bin`、`.map` 与编译数据库。不要把旧机器的build目录复制到新电脑；CMakeCache包含绝对路径。调试符号使ELF可能很大，实际烧录容量以链接报告为准。

记录交付给实车的确切固件：

```powershell
git rev-parse HEAD
Get-FileHash build/Debug/rf206s_motor_test.bin -Algorithm SHA256
```

## ST-Link烧录（人工执行）

先断开电机12V，C板逻辑供电，确认ST-Link接SWDIO、SWCLK、GND及目标电压参考；使用硬件复位连接时接NRST。目标电压参考不是向3.3V信号脚强灌电源，供电接法按C板/ST-Link实际型号确认。

```powershell
STM32_Programmer_CLI -l
STM32_Programmer_CLI -c port=SWD mode=UR reset=HWrst freq=1000 -w build/Debug/rf206s_motor_test.elf -v -rst
```

必须看到下载完成和 `Download verified successfully`，再记录提交和校验值。失败时旧固件可能仍在板内，不能根据“已编译”推断“已更新”。

- `No debug probe detected`：检查ST-Link USB、驱动、线缆、供电和是否被其他调试器占用。
- `Unable to get core ID`：检查板端SWD/GND/NRST与目标电压，复位后重试；必要时把 `freq=1000` 降到 `freq=100`。
- 路径或盘符错误：在本地克隆根目录执行，路径有空格时加引号，不依赖原作者F盘。
- 未连接探针时不要反复擦除或修改芯片保护设置；本项目不需要改读保护。

## VS Code

仓库任务包含Configure Debug、Build Debug与手动Flash。它们调用PATH内的工具，不写死用户名。Flash任务依赖Build，采用Under Reset/1000kHz；只有人工选择Flash任务才会烧录。默认构建和环境检查不驱动硬件。

## CubeMX重新生成前

`.ioc` 记录STM32F407IGHx、UFBGA176、12MHz HSE与168MHz系统时钟，固件包FW_F4 V1.28.3。它包含旧TIM8设置；**运行中的20kHz来自 `main.c` 启动后的覆盖**。此外部分生成文件有手工修订。重生成前创建分支，完整读 [维护说明](DEVELOPMENT.md#cubemx与手工修改)，逐文件比较后再编译、测波形。
