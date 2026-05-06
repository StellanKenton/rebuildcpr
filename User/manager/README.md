# Manager Layer

这是 `User/manager/` 的项目侧业务入口文档。

## 1. 目录边界

`User/manager` 负责当前产品业务编排，包括传感、无线、存储、电源、算法、音频提示等服务。

| 目录 | 职责 |
| --- | --- |
| `audio/` | CPR 场景音频业务状态机，调用 WT2003HX port 播放文件 |
| `cpralg/` | CPR 算法数据管理 |
| `iotmanager/` | BLE/WiFi 协议解析与回复 |
| `memory/` | 当前产品存储服务 |
| `power/` | 电源采样、关机请求和电量 LED |
| `sensor/` | 传感器采样管理 |
| `wireless/` | FC41D 无线模块业务管理 |

## 2. 分层规则

- 具体器件协议放在 `rep/module`。
- 板级 UART/GPIO 绑定放在 `User/port`。
- 任务创建和启动顺序放在 `User/system`。
- 本目录只放产品业务策略和跨模块编排。
