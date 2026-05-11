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

当前 CPR 纠正提示策略为：`audio` 每 15 秒判断一次，但判断对象不是“最近 3 个 15 秒窗口”，而是 `cpralgmgr` 维护的最近 3 次按压记录；当同一类快慢或深浅越界在最近 3 次按压里至少出现 2 次时，才触发对应语音。深浅与快慢分开判断，因此同一周期内若同时满足例如“过浅”和“过慢”，会连续播报两条语音。若按压中断达到 3 秒及以上，则清空这组按压历史并重新计时，恢复按压后不会立即触发快慢/深浅告警。

为避免 WT2003HX 在物理播放尚未结束时提前上报 `STOP` 导致前一条提示被截断，普通提示语音在业务层还会增加一个最小播放保护窗口，再允许切到下一条提示。

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
| `audioInit()` | audio task | 启动时等待 memory ready；若 `/setting/*` 缺失则保留默认语言/音量/节拍，再读取已存在配置并初始化 WT2003HX，最后设置单曲/DAC/音量 |
| `audioProcess()` | audio task 周期调用 | 处理串口回复、业务事件和播放队列；不再周期轮询设置文件 |
| `audioApplyLanguageSetting()` | iotmanager | 协议收到语言设置后立即同步 audio 状态，必要时插入切换语言提示 |
| `audioApplyVolumeSetting()` | iotmanager | 协议收到音量设置后立即同步 audio 状态，并在模块已就绪时直接下发音量 |
| `audioApplyMetronomeSetting()` | iotmanager | 协议收到节拍设置后立即同步 audio 状态并重置节拍计时 |
| `audioEnqueueNotice()` | 其他 manager 可选调用 | 投递普通语音提示，并清掉待播滴声；普通提示优先于滴声 |
| `audioEnqueueDidi()` | 其他 manager 可选调用 | 仅在无普通提示、无普通语音播放时投递节拍器滴声；普通语音忙时本次滴声直接跳过 |

## 4. 音频资源

文件名为两字符，不带扩展名。语言前缀：中文 `Z`、英文 `E`、德文 `D`、法文 `F`、意大利文 `I`；资源索引 `0-9` 对应开始 CPR、滴声、过深、过浅、过慢、过快、低电、低电关机、切换语言、按压良好。
