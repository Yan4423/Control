# 交接构建与检查记录

日期：2026-10-16。固件源码基线：`d037e800503e81e816a171748f429875acb1e2d8`。本次整理没有修改 `Src/`、`Inc/`、`Drivers/`、定时器/时钟、CMake源码清单或遥控参数。

## 环境

Windows；CMake 4.3.1；Ninja 1.13.2；GNU Tools for STM32 14.3.1（14.3.rel1）。烧录工具已在PATH定位，本次不连接探针、不执行下载。

## 结果

| 检查 | 结果 |
|---|---|
| 从独立克隆首次Debug配置/构建 | 通过，无编译警告；RAM 17616B，FLASH 25128B |
| 从独立克隆首次Release配置/构建 | 通过，无编译警告；RAM 17616B，FLASH 14960B |
| 环境检查 | Git、CMake、Ninja、GCC、objcopy、Programmer可在PATH定位 |
| PowerShell/JSON配置 | 语法解析通过 |
| 仪表离线检查 | 禁止/前进/后退映射、PWM独立坐标轴、秒时间轴、无效行过滤、断流状态、板复位、14字段兼容、200点窗口通过 |
| 蜂鸣器存档 | `git apply --check`通过，未应用到主线 |
| 文档相对链接、机器路径与常见密钥格式 | 检查通过；许可/作者信息保留 |
| 烧录、接收机实测、PWM针脚测量、车辆负载验收 | **本次未执行，不作为已通过项** |

## BIN校验值

```text
Debug   SHA256 14FF832B1F895BEE7355BDF7381F1D0CD813635F650F20F49F859F93CB4246ED
Release SHA256 61FBBE5EF239B82B4ED8E2561E92D8042D47134CC4AD1CBC50D3069B4A30B43A
```

以上为本次工具链构建产物。后续工具链或源码改变可能改变哈希；板内实际固件是否匹配需要接手人单独验证。ELF含调试路径，另一个目录构建的ELF哈希不一定相同。

蜂鸣器补丁SHA256：`A7A3F89DA17F7B43C94C4591F7338334D6FD4E66DDD27F246354B781B5AFB256`。

## 可复做的无硬件检查

```powershell
powershell.exe -NoProfile -File tools/check_environment.ps1
powershell.exe -NoProfile -STA -File tools/test_dashboard_offline.ps1
```

离线仪表测试使用模拟CSV，不打开COM、不烧录，会短暂建立屏幕外测试窗并生成 `build/dashboard-offline-preview.png`，不能用它代替USB接线或实机验收。构建命令见 [编译文档](BUILD_AND_FLASH.md)。
