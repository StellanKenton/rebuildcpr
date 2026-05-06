# 0x05 自检信息上报实现方案

## 目标

依据 `CPR反馈器通讯协议251208.xlsx` 中 `0x05 自检信息上报` 完成功能设计：

- 开机自检阶段先运行 2 秒故障采样。
- 自检 2 秒完成后再开机进入后续系统流程。
- 系统启动后，该模块放在 `system` 背景任务中每 100 ms 执行一次故障检测。
- 该模块只负责故障检测、故障触发、故障清除，以及保存当前故障和 2 秒窗口内出现过的故障。
- `0x05` 上报发生在 `wireless` 任务中，本模块不处理发送、不管理 2 秒发送节拍。
- 2 秒窗口内出现过的所有故障都需要被缓存；即使故障只出现一瞬间，也要保留给 `wireless` 任务上报。

本文只生成实现说明，不直接修改程序代码。

## 协议格式

文档位置：

- Sheet: `通信协议`
- 段落: `0x05 自检信息上报`
- 方向: `MCU -> HU`

帧格式：

| 字节 | 含义 |
|---|---|
| byte0 | `0xFA` |
| byte1 | `0xFC` |
| byte2 | `0x01` |
| byte3 | `0x05` |
| byte4 | `Len_H` |
| byte5 | `Len_L` |
| byte6 | CPR 模块自检结果 |
| byte7 | 电源模块自检结果 |
| byte8 | 音频模块自检结果 |
| byte9 | 通讯模块自检结果 |
| byte10 | 存储模块自检结果 |

按文档，`0x05` payload 长度为 5 字节，即 `Len = 0x0005`。当前 [cprsensor_protocol.h](/Users/rumi/Space/code/rebuildcpr/User/manager/iotmanager/cprsensor_protocol.h:207) 里的 `stCprsensorProtocolSelfCheckReplyPayload` 额外包含 `timestampBe[4]`，与 xlsx 不一致。实现时建议以 xlsx 为准，新增 5 字节 payload 结构或发送时只打包 5 字节，避免上位机按协议解析失败。

## 故障位定义

模块字节通用规则：

- `BIT0 = 1` 表示该模块自检通过。
- 若该模块存在任一故障位，`BIT0` 应清 0，并置对应故障位。
- 保留位保持 0。

| Payload 字节 | 模块 | Bit | 描述 | 故障 ID | 等级 |
|---|---|---:|---|---|---|
| byte6 | CPR 模块 | BIT0 | 自检通过 | / | / |
| byte6 | CPR 模块 | BIT1 | 加速度计初始化失败 | E01 | H |
| byte6 | CPR 模块 | BIT2 | RTC 自检错误 | E02 | M |
| byte7 | 电源模块 | BIT0 | 自检通过 | / | / |
| byte7 | 电源模块 | BIT1 | 3.3V 电源过高 | E11 | H |
| byte7 | 电源模块 | BIT2 | 3.3V 电源过低 | E12 | H |
| byte7 | 电源模块 | BIT3 | 5V 电源过高 | E13 | H |
| byte7 | 电源模块 | BIT4 | 5V 电源过低 | E14 | H |
| byte7 | 电源模块 | BIT5 | DC 电压输入过高 | E15 | H |
| byte8 | 音频模块 | BIT0 | 自检通过 | / | / |
| byte8 | 音频模块 | BIT1 | 音频模块通信失败 | E21 | M |
| byte8 | 音频模块 | BIT2 | 音频模块歌曲数目异常 | E22 | L |
| byte9 | 通讯模块 | BIT0 | 自检通过 | / | / |
| byte9 | 通讯模块 | BIT1 | 无线模块初始化失败 | E31 | M |
| byte10 | 存储模块 | BIT0 | 自检通过 | / | / |
| byte10 | 存储模块 | BIT1 | 存储模块初始化失败 | E41 | H |

## 建议新增模块

建议新增 `User/manager/selfcheck/selfcheck_fault.c/.h`，让 `selfcheck` 继续只保存初始化结果，`selfcheck_fault` 负责：

- 将各模块运行状态转换为协议 bit。
- 维护当前故障状态。
- 维护 2 秒窗口内出现过的故障锁存。
- 提供故障触发、故障清除接口，供其他模块主动更新故障。
- 生成 5 字节 `0x05` payload。
- 提供 payload 读取接口给 `wireless` 任务；本模块不主动上报。

建议接口：

```c
typedef struct stSelfCheckFaultPayload {
    uint8_t cpr;
    uint8_t power;
    uint8_t audio;
    uint8_t wireless;
    uint8_t memory;
} stSelfCheckFaultPayload;

void selfCheckFaultInit(void);
void selfCheckFaultResetWindow(void);
void selfCheckFaultProcess100ms(void);
bool selfCheckFaultRunStartupWindow(uint32_t durationMs);
void selfCheckFaultSetBits(const stSelfCheckFaultPayload *faultBits);
void selfCheckFaultClearBits(const stSelfCheckFaultPayload *faultBits);
void selfCheckFaultGetCurrentPayload(stSelfCheckFaultPayload *payload);
void selfCheckFaultGetWindowPayload(stSelfCheckFaultPayload *payload);
void selfCheckFaultConsumeWindowPayload(stSelfCheckFaultPayload *payload);
```

## 采样与锁存逻辑

核心原则：采样周期 100 ms，模块内部维护两个状态：

- `currentFault`：当前仍存在的故障。
- `windowFault`：最近一个 2 秒上报窗口内出现过的故障，瞬时故障也会被锁存。

每次采样得到新的检测结果后：

- 新出现的故障置入 `currentFault` 和 `windowFault`。
- 已恢复的故障从 `currentFault` 清除。
- 已恢复的故障不从 `windowFault` 清除，直到 `wireless` 任务消费窗口。
- 需要防抖的故障先经过防抖确认，再更新 `currentFault` 和 `windowFault`。

`wireless` 任务每 2 秒读取一次 `windowFault | currentFault` 并上报。上报动作完成后调用 `selfCheckFaultConsumeWindowPayload()` 消费窗口。本模块消费窗口时应把 `windowFault` 重置为当时的 `currentFault`，避免持续故障在跨窗口时丢失。

建议伪代码：

```c
static stSelfCheckFaultPayload gCurrentFault;
static stSelfCheckFaultPayload gWindowFault;
static uint32_t gLastSampleTick;

void selfCheckFaultProcess100ms(void)
{
    uint32_t now = repRtosGetTickMs();

    if ((uint32_t)(now - gLastSampleTick) < 100U) {
        return;
    }
    gLastSampleTick = now;

    gCurrentFault = selfCheckFaultCollectCurrent();
    selfCheckFaultOrFaultBits(&gWindowFault, &gCurrentFault);
}

void selfCheckFaultConsumeWindowPayload(stSelfCheckFaultPayload *payload)
{
    repRtosEnterCritical();
    if (payload != NULL) {
        *payload = selfCheckFaultBuildPayload(selfCheckFaultMerge(&gWindowFault, &gCurrentFault));
    }
    gWindowFault = gCurrentFault;
    repRtosExitCritical();
}
```

`selfCheckFaultCollectCurrent()` 内部只处理故障位，不直接置 `BIT0`。输出 payload 时再补通过位：

```c
static uint8_t selfCheckFaultFinalizeModuleByte(uint8_t faultBits)
{
    return (faultBits == 0U) ? 0x01U : faultBits;
}
```

注意：`currentFault` 和 `windowFault` 都只保存故障位，不保存 `BIT0` 通过位。对外读取 payload 时按每个模块是否有故障决定是否置 `BIT0`。

## 防抖策略

每个故障项如果存在采样抖动、状态瞬间跳变或边界阈值抖动，建议做防抖处理。防抖只影响 `currentFault` 的触发和清除；一旦故障被确认触发，必须立即 OR 到 `windowFault`，保证 2 秒窗口内出现过的故障不会丢失。

建议给每个需要防抖的故障维护独立状态：

```c
typedef struct stSelfCheckFaultDebounce {
    uint8_t activeCount;
    uint8_t clearCount;
    bool active;
} stSelfCheckFaultDebounce;
```

建议规则：

- 触发防抖：连续 N 次采样检测到故障后，才置为 active。
- 清除防抖：连续 M 次采样检测为正常后，才清除 active。
- 采样周期为 100 ms，N/M 按故障类型设置。
- 故障一旦 active，本次采样就写入 `currentFault` 和 `windowFault`。
- 故障清除只清 `currentFault`，不清 `windowFault`。

建议默认参数：

| 故障类型 | 触发防抖 | 清除防抖 | 说明 |
|---|---:|---:|---|
| 加速度计初始化失败 E01 | 1 次 | 3 次 | 初始化结果稳定，恢复需要连续正常确认 |
| RTC 自检错误 E02 | 1 次 | 1 次 | 时间判断稳定，不需要延迟 |
| 电源电压过高/过低 E11~E15 | 3 次 | 5 次 | ADC 和电源边界容易抖动 |
| 音频通信失败 E21 | 3 次 | 5 次 | 避免瞬时通信失败直接进入当前故障 |
| 音频歌曲数异常 E22 | 1 次 | 3 次 | 查询结果相对稳定 |
| 无线初始化失败 E31 | 3 次 | 5 次 | 无线状态机启动/切换期间可能短暂异常 |
| 存储初始化失败 E41 | 1 次 | 3 次 | 初始化结果稳定，恢复需要连续正常确认 |

电压类故障建议同时做迟滞，避免数值在阈值附近反复触发/清除。例如 3.3V 过高可以用 `> 3600 mV` 触发，恢复阈值用 `< 3550 mV`；3.3V 过低可以用 `< 3000 mV` 触发，恢复阈值用 `> 3050 mV`。5V 和 DC 同理预留 50~100 mV 迟滞，具体值按硬件确认。

## 各模块故障来源

建议第一版先复用现有状态，缺失的检测项用明确 TODO 标注，不要随意推断。

| 模块 | 故障位 | 建议来源 |
|---|---:|---|
| CPR | BIT1 加速度计初始化失败 | `sensorGetInitStatus()` 中 `accReady == false` 或 `sensorIsReady() == false` 的加速度计部分 |
| CPR | BIT2 RTC 自检错误 | 读取 RTC 当前时间；若 RTC 时间早于 `2026-05-01 00:00:00`，置 E02 |
| 电源 | BIT1/2 3.3V 过高/过低 | `powerGetManager()->voltage.v3v3Mv`，大于3.6V或低于3.0V |
| 电源 | BIT3/4 5V 过高/过低 | `powerGetManager()->voltage.v5v0Mv`，大于5.5V或低于4.5V |
| 电源 | BIT5 DC 输入过高 | `powerGetManager()->voltage.dcMv`，大于8.0V |
| 音频 | BIT1 通信失败 | `audioGetStatus()->state == AUDIO_STATE_FAULT` 或 `moduleReady == false` |
| 音频 | BIT2 歌曲数异常 | 建议在 `audioInit()` 查询歌曲数后和当前不同语言的音频数量对比要大于等于当前的不同语言的音频数 |
| 通讯 | BIT1 无线初始化失败 | `wirelessInit()` 结果或 `wireless` 状态中模块不可用状态 |
| 存储 | BIT1 存储初始化失败 | `selfCheckGetSummary()->flashReady == false` 或 `memoryInit()` 结果 |

电源阈值建议集中定义在 `selfcheck_fault.c`，并在注释中注明来源待确认。例如：

```c
#define SELF_CHECK_3V3_HIGH_MV 3600U
#define SELF_CHECK_3V3_LOW_MV  3000U
#define SELF_CHECK_5V_HIGH_MV  5500U
#define SELF_CHECK_5V_LOW_MV   4500U
#define SELF_CHECK_DC_HIGH_MV  6000U
```

这些值只是占位建议，真正实现前需要和硬件/产品确认。

### RTC 自检判断

RTC 自检错误对应 CPR 模块 `byte6 BIT2`，故障 ID 为 `E02`。判断规则：

- 读取 RTC 当前年月日时分秒。
- 将 RTC 时间与 `2026-05-01 00:00:00` 比较。
- 若 RTC 时间小于 `2026-05-01 00:00:00`，认为 RTC 时间异常，置 `byte6 BIT2`。
- 若 RTC 读取失败，也应按 RTC 自检错误处理，置 `byte6 BIT2`。

建议在 `selfcheck_fault.c` 中封装为独立函数：

```c
static bool selfCheckFaultIsRtcTimeValid(void)
{
    /* TODO: 使用项目 RTC 读取接口获取当前年月日时分秒。 */
    if (!rtcReadDateTime(&dateTime)) {
        return false;
    }

    if (dateTime.year < 2026U) {
        return false;
    }
    if ((dateTime.year == 2026U) && (dateTime.month < 5U)) {
        return false;
    }
    if ((dateTime.year == 2026U) && (dateTime.month == 5U) && (dateTime.day < 1U)) {
        return false;
    }

    return true;
}
```

实际实现时应替换伪代码里的 `rtcReadDateTime()` 和 `dateTime` 类型，使用当前工程已有 RTC/HAL 接口。

## 与 wireless 任务的边界

`0x05` 组帧、2 秒发送节拍、链路选择和发送重试全部放在 `wireless` 任务或其调用的协议管理模块中处理。`selfCheckFault` 不包含任何发送接口。

建议 `wireless` 侧新增读取和消费流程：

```c
static void wirelessServiceSelfCheckReport(void)
{
    if ((uint32_t)(now - lastReportTick) < 2000U) {
        return;
    }

    selfCheckFaultConsumeWindowPayload(&payload);
    protcolMgrSendSelfCheckReport((const uint8_t *)&payload, 5U);
    lastReportTick = now;
}
```

协议发送接口可以放在 `protcolmgr`，例如：

```c
bool protcolMgrSendSelfCheckReport(const uint8_t payload[5]);
```

内部使用现有 `cprsensorProtocolPackFrame()`，命令字使用 `CPRSENSOR_PROTOCOL_CMD_SELF_CHECK`。这属于 wireless/protocol 边界内的内容，不放入 `selfCheckFault`。

## 系统接入点

### 开机自检阶段

当前 [sysmgr.c](/Users/rumi/Space/code/rebuildcpr/User/system/sysmgr.c:210) 中 `systemSelfCheckMode()` 直接将 `lSelfCheckCompleted = true`，需要改为真实 2 秒窗口：

1. 首次进入 `eSYSTEM_SELF_CHECK_MODE` 时调用 `selfCheckFaultResetWindow()`，记录开始 tick。
2. 在该模式内每 100 ms 调用一次 `selfCheckFaultProcess100ms()` 或专用 startup 采样函数。
3. 满 2 秒后调用 `selfCheckCommit()`。
4. 创建 worker tasks，并进入 standby/normal 流程。

自检阶段只要求“先自检 2 秒再开机”，不在该模块里上报 `0x05`。

### 系统启动后

当前 [systask.c](/Users/rumi/Space/code/rebuildcpr/User/system/systask.c:205) 的 `systemTaskEntry()` 每 10 ms 循环一次。不要直接把整个 system task 改成 100 ms，否则会影响 watchdog、ADC background、power LED/debug 等现有节拍。建议在 `systemTaskEntry()` 中调用：

```c
selfCheckFaultProcess100ms();
```

并由 `selfCheckFaultProcess100ms()` 内部用 tick 保证 100 ms 执行一次。

建议只在系统完成自检并进入运行态后做周期检测：

```c
if ((systemGetMode() == eSYSTEM_STANDBY_MODE) || (systemGetMode() == eSYSTEM_NORMAL_MODE)) {
    selfCheckFaultProcess100ms();
}
```

## 验收点

- 本模块不发送 `0x05`，只提供可用于 `0x05` 的 5 字节 payload。
- 无故障时读取 payload，5 个模块字节均为 `0x01`。
- 任一模块有故障时读取 payload，该模块 `BIT0 = 0`，对应故障 bit 置 1。
- 开机自检模式至少采样 2 秒后才完成。
- 自检完成后不由本模块上报，直接进入后续开机流程。
- 系统运行后采样周期为 100 ms。
- 当前故障恢复后会从当前故障状态清除。
- 2 秒内短暂出现又恢复的故障仍保留在窗口故障状态中，直到 `wireless` 消费窗口。
- `wireless` 消费窗口后，已恢复的故障不会继续出现在下一个窗口。
- 持续存在的故障在消费窗口后仍保留为当前故障，并会进入下一个窗口。
