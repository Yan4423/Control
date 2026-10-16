# 第三方来源与署名保留

本次交接只整理仓库，不改变已有第三方版权或许可，不替整个项目选择新的MIT/GPL等许可证。

| 范围 | 来源/现有标记 | 仓库内证据 |
|---|---|---|
| 项目集成与遥控、电机测试演进 | Yan4423 | [AUTHORS.md](AUTHORS.md)、Git历史 |
| 初始工程骨架 | 既有README记录为RoboMaster C板 `14.PWM_SNAIL` 示例衍生 | 原始Git提交 `d037e80` 的README |
| `applications/`、`bsp/` | 保留的DJI/RoboMaster旧遥控示例 | `applications/remote_control.c` 中2019 DJI与RM标记；当前不加入构建 |
| `Drivers/CMSIS/` | ARM/ST CMSIS | [CMSIS许可](Drivers/CMSIS/LICENSE.txt)、[STM32F4设备许可](Drivers/CMSIS/Device/ST/STM32F4xx/LICENSE.txt) |
| `Drivers/STM32F4xx_HAL_Driver/` | STMicroelectronics HAL | [HAL许可](Drivers/STM32F4xx_HAL_Driver/LICENSE.txt)与逐文件头 |
| `Src/`、`Inc/`、启动文件等 | 原示例与CubeMX生成文件混合 | 逐文件头；`.ioc` 记录STM32Cube FW_F4 V1.28.3 |

当前部分旧DJI示例缺少独立许可文件，保留的中文注释也存在旧编码显示问题。本次没有推测、替换这些许可或批量改写第三方文件。如果后续计划改为公开发布，维护者应先补核原始示例的来源和适用许可。私有仓库的协作权限不等于对第三方代码的重新授权。
