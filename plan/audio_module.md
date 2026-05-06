# 音频模块说明

本文档按当前代码整理音频模块的硬件连接、串口/DMA 配置、通信协议、初始化流程、音频资源映射和播放时机。


## 1. 硬件引脚

| 功能 | 宏定义 | MCU 引脚 | 当前 GPIO/复用配置 | 说明 |
|---|---|---:|---|---|
| 音频模块使能 | `EN_AUDIO_Pin` | PA1 | GPIO 推挽输出，默认低电平 | `BspAudio_Set(AUDIO_ENABLE)` 输出低电平使能；`AUDIO_DISABLE` 输出高电平关闭 |
| 音频串口发送 | `RX_AUDIO_Pin` | PA2 | `USART2_TX`, AF_PP | 名称按音频模块侧理解：MCU TX 接音频 RX |
| 音频串口接收 | `TX_AUDIO_Pin` | PA3 | `USART2_RX`, input no pull | 名称按音频模块侧理解：MCU RX 接音频 TX |

使能电平：

- `AUDIO_ENABLE`: PA1 输出 `GPIO_PIN_RESET`。
- `AUDIO_DISABLE`: PA1 输出 `GPIO_PIN_SET`。

## 2. 串口和 DMA

当前驱动使用 `USART2`：

```c
#define UART_AUDIO huart2
```

| 项目 | 当前代码值 |
|---|---|
| UART 句柄 | `huart2` |
| 外设实例 | `USART2` |
| 波特率 | `115200` |
| 数据格式 | 8 data bits, 1 stop bit, no parity |
| 硬件流控 | 无 |
| TX DMA | `DMA1_Channel7`, memory-to-peripheral, normal, byte, low priority |
| RX DMA | `DMA1_Channel6`, peripheral-to-memory, normal, byte, low priority |


## 3. 通信协议

音频模块使用 UART 异步串口命令帧。

帧格式：

| 字段 | 长度 | 当前代码 |
|---|---:|---|
| 起始码 | 1 | `0x7E` |
| 长度 | 1 | `0x03 + 参数长度` |
| 命令码 | 1 | 见命令表 |
| 参数 | N | 可为空 |
| 累加和 | 1 | 从长度字段到最后一个参数逐字节累加 |
| 结束码 | 1 | `0xEF` |

发送时的总字节数为 `长度 + 2`，其中额外 2 字节是起始码和结束码。

示例：按文件名播放 `Z1`。

```text
7E 05 A1 5A 31 31 EF
```

计算说明：

- 长度 `0x05 = 0x03 + 2`
- 命令 `0xA1`
- 参数 `"Z1" = 0x5A 0x31`
- 校验和 `0x05 + 0xA1 + 0x5A + 0x31 = 0x131`，取低 8 位为 `0x31`

当前接收解析会检查起始码、长度和结束码，并根据命令码更新 `Audio_Info`；代码未校验接收帧的累加和。

### 3.1 命令表

| 命令宏 | 值 | 方向 | 参数 | 用途 |
|---|---:|---|---|---|
| `CMD_EXTFLASH_INDEX_PLAY` | `0xA0` | MCU -> audio | 2 字节索引 | 指定片外 Flash 索引播放 |
| `CMD_EXTFLASH_NAME_PLAY` | `0xA1` | MCU -> audio | 文件名，不带后缀，最长 8 字节 | 指定片外 Flash 根目录文件名播放 |
| `CMD_PLAY_STOP` | `0xAA` | MCU -> audio | 无 | 停止播放 |
| `CMD_PLAY_PAUSE` | `0xAB` | MCU -> audio | 无 | 暂停/继续播放 |
| `CMD_VOLUM_SET` | `0xAE` | MCU -> audio | 1 字节音量，0-31 | 设置音量 |
| `CMD_PLAY_MODE` | `0xAF` | MCU -> audio | 1 字节模式 | 设置播放模式 |
| `CMD_OUTPUT_MODE_SWICTH` | `0xB6` | MCU -> audio | 1 字节输出方式 | 设置 SPK/DAC 输出 |
| `CMD_CHECK_VERSION` | `0xC0` | MCU -> audio | 无 | 查询软件版本 |
| `CMD_CHECK_VOLUM_SET` | `0xC1` | MCU -> audio | 无 | 查询当前音量 |
| `CMD_CHECK_STATE` | `0xC2` | MCU -> audio | 无 | 查询播放状态 |
| `CMD_CHECK_MUSIC_NUM` | `0xC3` | MCU -> audio | 无 | 查询音乐总数 |
| `CMD_CHECK_CONNECT_STATE` | `0xCA` | MCU -> audio | 无 | 查询外设连接状态 |

### 3.2 当前使用的模块状态

| 状态/配置 | 值 | 说明 |
|---|---:|---|
| `SINGLE_MODE` | 0 | 单曲不循环 |
| `SINGLE_CYCLE` | 1 | 单曲循环 |
| `ALL_CYCLE` | 2 | 全部循环 |
| `RANDOM` | 3 | 随机 |
| `SPK_MODE` | 0 | SPK 输出 |
| `DAC_MODE` | 1 | DAC 输出 |
| `PLAY_STATE` | 1 | 播放状态 |
| `STOP_STATE` | 2 | 停止状态 |
| `PAUSE_STATE` | 3 | 暂停状态 |

初始化代码注释写“设置为单曲循环模式”，但实际发送的是 `SINGLE_MODE = 0`。当前以代码值为准，即单曲不循环。

## 4. 初始化流程

音频任务在 `AudioTask()` 中启动：

1. 调用 `Audio_Modle_Init()`。
2. 之后每 20 ms 调用一次 `App_Audio_Handle()`。


流程
1. `BspAudio_Set(AUDIO_DISABLE)`：关闭音频模块，延时约 1050 ms。
2. `Drv_Audio_Init()`：初始化 USART2、DMA 空闲接收和环形缓冲。
3. `BspAudio_Set(AUDIO_ENABLE)`：打开音频模块，延时约 1050 ms。
4. 依次查询：
   - `CMD_CHECK_VERSION`
   - `CMD_CHECK_MUSIC_NUM`
   - `CMD_CHECK_STATE`
   - `CMD_CHECK_CONNECT_STATE`
5. 设置播放模式：发送 `CMD_PLAY_MODE`，参数 `SINGLE_MODE`。
6. 设置输出模式：发送 `CMD_OUTPUT_MODE_SWICTH`，参数 `DAC_MODE`。
7. 延时约 300 ms。
8. 如果版本为空，置位 `g_Err.Audio.bits.Communication_Err`。
9. 如果歌曲数为 0，置位 `g_Err.Audio.bits.Song_Num_Err`。

如果任意查询/设置命令在 1000 ms 内收不到相同命令码的回复，`Audio_Init_Check()` 返回通信错误。

## 5. 运行状态机

音频状态机：

| 状态 | 行为 |
|---|---|
| `AUDIO_PLAY_START` | 播放 CPR 开始提示音，然后进入 `AUDIO_PLAY_DIDI` |
| `AUDIO_PLAY_DIDI` | 优先读取 `Play_Audio` 普通语音队列；如果没有普通语音，就读取 `Play_DiDI` 滴声队列 |
| `AUDIO_PLAY_NOTICE` | 播放普通语音，等待播放结束后回到 `AUDIO_PLAY_DIDI`，并丢弃之前的滴声队列项避免积压 |

普通语音和滴声播放前都会轮询 `CMD_CHECK_STATE`，等待模块不处于播放状态后再发播放命令。

## 6. 语言、音量和音频文件映射

### 6.1 语言

| 枚举 | 值 | 当前文件名前缀 |
|---|---:|---|
| `AUDIO_ZH` | `0x01` | `Z` |
| `AUDIO_EN` | `0x02` | `E` |
| `AUDIO_DE` | `0x03` | `D` |
| `AUDIO_FR` | `0x04` | `F` |
| `AUDIO_IT` | `0x05` | `I` |
| `AUDIO_Default` | `0xFF` | 未初始化 |

`VoiceName[6][10][3]` 中第 0 行也填了 `Z0` 到 `Z9`，但正常语言枚举从 1 开始，第 0 行通常不应被使用。

### 6.2 音量

无线端/存储端保存的是 0-3 档，实际下发给音频模块的是 0-31 级。

| 外部档位 | 实际音量 |
|---:|---:|
| 0 | 0 |
| 1 | 20 |
| 2 | 26 |
| 3 | 31 |
| 其它 | 31 |

音量更新流程：

1. 无线命令 `E_CMD_VOLUME` 更新 `Audio_Volume_Rev`。
2. `App_Audio_Handle()` 中 `Update_Play_Setting()` 检测到变化。
3. 发送 `CMD_VOLUM_SET` 设置音量。
4. 发送 `CMD_CHECK_VOLUM_SET` 查询确认。
5. 存储任务 `Memory_Save_Volume()` 会把语言和音量保存到 Flash。

### 6.3 当前音频资源

音频文件通过两字符文件名播放，例如中文 `Z0`、英文 `E0`，播放命令使用 `CMD_EXTFLASH_NAME_PLAY`。文件名不带扩展名。

| 枚举 | 索引 | 中文 | 英文 | 德文 | 法文 | 意大利文 | 当前代码用途 |
|---|---:|---|---|---|---|---|---|
| `ADUIO_STRAT_CPR` | 0 | `Z0` | `E0` | `D0` | `F0` | `I0` | CPR 开始提示 |
| `ADUIO_DIDI` | 1 | `Z1` | `E1` | `D1` | `F1` | `I1` | CPR 节拍器滴声 |
| `ADUIO_PRESS_DEEP` | 2 | `Z2` | `E2` | `D2` | `F2` | `I2` | 按压过深 |
| `ADUIO_PRESS_SWALLOW` | 3 | `Z3` | `E3` | `D3` | `F3` | `I3` | 按压过浅 |
| `ADUIO_PRESS_SLOW` | 4 | `Z4` | `E4` | `D4` | `F4` | `I4` | 按压过慢 |
| `ADUIO_PRESS_FAST` | 5 | `Z5` | `E5` | `D5` | `F5` | `I5` | 按压过快 |
| `ADUIO_LOW_BATTERY` | 6 | `Z6` | `E6` | `D6` | `F6` | `I6` | 低电量提示 |
| `ADUIO_BATTERY_DEAD` | 7 | `Z7` | `E7` | `D7` | `F7` | `I7` | 低电关机提示 |
| `ADUIO_CHANGE_LANGUAGE` | 8 | `Z8` | `E8` | `D8` | `F8` | `I8` | 切换语言提示 |
| `ADUIO_PRESS_WELL` | 9 | `Z9` | `E9` | `D9` | `F9` | `I9` | 代码会入队，但当前 `Play_Notice_Audio()` 没有处理该 case |
| `ADUIO_RELEASE_BAD` | 10 | 无映射 | 无映射 | 无映射 | 无映射 | 无映射 | 当前未见播放入口；`VoiceName` 只有 0-9 共 10 项 |

注意：`ADUIO_PRESS_WELL` 已在 CPR 反馈中入队，但 `Play_Notice_Audio()` 的 `switch` 没有该分支；`ADUIO_RELEASE_BAD` 超出当前 `VoiceName[][10]` 的第二维范围，也未见播放入口。这两项属于当前代码现状，后续若要启用需补齐映射和播放分支。

## 7. 播放时机

### 7.1 开始 CPR

音频状态机第一次进入 `AUDIO_PLAY_START` 时播放：

- 队列：不走队列，直接播放。
- 音频：`ADUIO_STRAT_CPR`，即当前语言的 `?0`。
- 之后进入滴声/提示状态。

### 7.2 节拍器滴声

`CPR_Metronome()` 按节拍周期投递 `ADUIO_DIDI` 到 `Play_DiDI` 队列。

播放音频：

- `ADUIO_DIDI`，即当前语言的 `?1`。

`CPR_Metronome_Freq` 会跟随 `CPR_Metronome_Freq_Recv` 更新，外部可通过无线命令修改节拍频率。

### 7.3 CPR 质量提示

CPR 报警逻辑以 15 秒为一个窗口，对最近 3 次统计值判断；3 次中至少 2 次满足条件时投递语音。

| 条件 | 队列 | 音频 |
|---|---|---|
| 最近 3 次中至少 2 次深度高于 `Depth_High_Limit` | `Play_Audio` | `ADUIO_PRESS_DEEP`, `?2` |
| 最近 3 次中至少 2 次深度低于 `Depth_Low_Limit` | `Play_Audio` | `ADUIO_PRESS_SWALLOW`, `?3` |
| 最近 3 次中至少 2 次频率高于 `Freq_High_Limit` | `Play_Audio` | `ADUIO_PRESS_FAST`, `?5` |
| 最近 3 次中至少 2 次频率低于 `Freq_Low_Limit` | `Play_Audio` | `ADUIO_PRESS_SLOW`, `?4` |
| 最近 3 次中至少 2 次深度和频率都在上下限内 | `Play_Audio` | `ADUIO_PRESS_WELL`, `?9`，但当前播放函数未处理该 case |

普通语音优先级高于滴声；播放完普通语音后会丢弃一个滴声队列项。

### 7.4 低电量和低电关机

电源任务在低电模式 `E_LOW_POWER_MODE` 下投递语音。

| 条件 | 队列 | 音频 | 后续动作 |
|---|---|---|---|
| `su8_LowBattry_Tick == 0` | `Play_Audio` | `ADUIO_LOW_BATTERY`, `?6` | 低电提示周期重置为 60000 ms |
| `BatValue == 0` 且 `DCIN <= 100` 持续 5000 ms | `Play_Audio` | `ADUIO_BATTERY_DEAD`, `?7` | 延时 5000 ms 后执行 `App_Power_ShutDown()` |

### 7.5 切换语言

无线命令 `E_CMD_LANGUAGE` 会：

1. 设置 `Audio_Language_Rev = Recv_Data[0]`。
2. 设置 `Audio_Change_Language_Flag = 1`。
3. 音频任务中 `Update_Play_Setting()` 检测到标志后投递 `ADUIO_CHANGE_LANGUAGE`。

播放音频：

- `ADUIO_CHANGE_LANGUAGE`，即新语言下的 `?8`。

语言和音量由 `Memory_Read_VolumeSetting()` 从 Flash 读取；读取失败时默认 `AUDIO_ZH` 和音量档位 3。

- `CprSensor.ioc`/注释中的音频串口波特率与当前 `usart.c` 不一致：IOC/注释为 9600，代码为 115200。
- 初始化注释写单曲循环，但代码发送 `SINGLE_MODE = 0`，按枚举含义是单曲不循环。
- 接收帧未校验累加和，只检查头、长度和尾。
- `ADUIO_PRESS_WELL` 会被 CPR 反馈逻辑入队，但普通语音播放函数未处理该枚举。
- `ADUIO_RELEASE_BAD` 没有文件名映射和播放入口。
- `Play_Notice_Audio()` 的默认分支不会清空 `SendData`，如果遇到未处理枚举，可能沿用上一次播放参数。
