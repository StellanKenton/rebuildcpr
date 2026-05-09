---
doc_role: manager-spec
layer: manager
module: audio
status: active
portability: project-bound
public_headers:
    - audio.h
core_files:
    - audio.c
port_files: []
debug_files: []
depends_on:
    - ../../../rep/module/wt2003hx/wt2003hx.md
forbidden_depends_on:
    - STM32 HAL UART
    - direct GPIO
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
    - audio.h
    - audio.c
read_next:
    - audio.h
---

# Audio Manager 说明

这是当前目录的权威入口文档。

## 1. 业务目标

`audio` manager 负责把 CPR 产品事件转换为音频播放请求：开机后播放 CPR 开始提示、按节拍播放滴声、根据 CPR 质量播放纠正提示、处理低电量提示，并响应无线协议下发的语言、音量和节拍设置。

## 2. 模块/业务拆分

| 归属 | 功能 |
| --- | --- |
| `rep/module/wt2003hx` | 串口帧协议、命令组帧、校验和、回复解析、模块状态缓存 |
| `User/port/wt2003hx_port.*` | `DRVUART_AUDIO`、`DRVGPIO_EN_AUDIO`、使能电平、tick/delay 绑定 |
| `User/manager/audio` | 音频资源映射、播放队列、CPR 节拍、低电提示、语言/音量更新 |
| `User/system/systask.*` | 创建并周期调度 audio 任务 |

## 3. 公共函数使用契约表

| 函数 | 调用方 | 说明 |
| --- | --- | --- |
| `audioInit()` | audio task | 启动时等待 memory ready 且 `/setting/volume` 可读；随后从 `/setting/*` 读取语言/音量/节拍配置，再初始化 WT2003HX 并设置单曲/DAC/音量 |
| `audioProcess()` | audio task 周期调用 | 处理串口回复、业务事件和播放队列；不再周期轮询设置文件 |
| `audioApplyLanguageSetting()` | iotmanager | 协议收到语言设置后立即同步 audio 状态，必要时插入切换语言提示 |
| `audioApplyVolumeSetting()` | iotmanager | 协议收到音量设置后立即同步 audio 状态，并在模块已就绪时直接下发音量 |
| `audioApplyMetronomeSetting()` | iotmanager | 协议收到节拍设置后立即同步 audio 状态并重置节拍计时 |
| `audioEnqueueNotice()` | 其他 manager 可选调用 | 投递普通语音提示，并清掉待播滴声；普通提示优先于滴声 |
| `audioEnqueueDidi()` | 其他 manager 可选调用 | 仅在无普通提示、无普通语音播放时投递节拍器滴声；普通语音忙时本次滴声直接跳过 |

## 4. 音频资源

文件名为两字符，不带扩展名。语言前缀：中文 `Z`、英文 `E`、德文 `D`、法文 `F`、意大利文 `I`；资源索引 `0-9` 对应开始 CPR、滴声、过深、过浅、过慢、过快、低电、低电关机、切换语言、按压良好。
