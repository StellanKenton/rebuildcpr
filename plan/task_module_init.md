# 模块任务内初始化迁移建议

## 背景

当前系统初始化主要集中在 `User/system/sysmgr.c` 的 `systemModuleInit()` 中，`powerInit()`、`sensorInit()`、`memoryInit()`、`wirelessInit()` 等模块会在系统初始化模式下串行执行。这样实现简单，但存在几个问题：

- 单个模块初始化耗时或失败重试会阻塞系统模式切换。
- 上电自检、后台服务、看门狗刷新都依赖系统主任务继续运行。
- 部分模块本身已经有独立任务，初始化和运行逻辑分离在不同位置，职责不够清晰。

目标是把适合异步启动的业务模块初始化移动到各自任务中执行，让 `sysmgr` 只负责系统级启动、基础依赖和模式调度。

## 总体原则

1. BSP 和最小系统基础能力仍同步初始化。
2. 业务模块由自己的任务负责 `init + process`。
3. 模块之间通过 ready/fault 状态同步，不通过隐式阻塞同步。
4. 上电自检读取模块状态，不直接承担模块初始化。
5. 初始化失败不能卡死任务，必须有重试间隔或故障状态。

## 建议保留在 `sysmgr` 的初始化

这些初始化属于系统基础能力或很早就要使用，建议继续保留在 `systemInitMode()` / `systemModuleInit()`：

- `systemInitBsp()`
- `drvGpioInit()`
- `selfCheckInit()`
- `selfCheckFaultInit()`
- `pca9535PortInit()`
- `tm1651PortInit()`

说明：

- `pca9535PortInit()` 和 `tm1651PortInit()` 会影响 LED、显示和自检反馈，早期初始化更稳。
- `vfsInit()` 不放在 `sysmgr`，由 memory task 负责初始化。这样存储相关的 VFS 服务、littlefs port 初始化、挂载和 flash 检查都收敛在 memory 模块内。

## 建议迁移到任务内的模块

### Power

当前 `powerTaskEntry()` 只 delay，`powerProcess()` 仍在 `systemTaskEntry()` 中执行。建议先把 power 的运行职责迁移到 power task。

建议结构：

```c
static void powerTaskEntry(void *argument)
{
    (void)argument;

    for (;;) {
        if (!powerIsReady()) {
            (void)powerInit();
        } else {
            powerProcess();
        }

        (void)repRtosDelayMs(PowerTaskInterval);
    }
}
```

迁移后 `systemTaskEntry()` 中可以保留 `powerLedProcess()`，因为 LED 显示与系统模式有关；也可以后续统一移入 power task，但要保证 `pca9535PortInit()` 已完成。

### Sensor

`sensorInit()` 会创建队列、初始化 force ADC 和 LIS2HH12。它适合放入 `sensorTaskEntry()`。

sensor task 需要实时采集加速度和压力数据，是 CPR 算法输入链路的上游任务，因此优先级应保持较高。迁移初始化时不能因为高优先级而让初始化失败重试持续占用 CPU。

建议结构：

```c
static void sensorTaskEntry(void *argument)
{
    (void)argument;

    for (;;) {
        if (!sensorIsReady()) {
            (void)sensorInit();
        } else {
            sensorProcess();
        }

        (void)repRtosDelayMs(SensorTaskInterval);
    }
}
```

注意：

- `cprAlgMgrProcess()` 依赖 `sensorReadSample()`，但当前 `sensorReadSample()` 已能处理队列未创建场景，适合异步初始化。
- `SensorTaskPriority` 较高是合理的，因为采样路径需要实时性。
- `sensorInit()` 失败重试时必须 delay，避免高优先级 sensor task 在外设异常时持续占用 CPU。

### Memory

`memoryInit()` 应负责完整的存储初始化链路，包括 `vfsInit()`、`vfsLittlefsPortInit()` 和 `vfsMount()`。这些步骤可能涉及外部 flash 和文件系统挂载，适合放入 `memoryTaskEntry()`。

建议结构：

```c
static void memoryTaskEntry(void *argument)
{
    (void)argument;

    for (;;) {
        if (!memoryIsReady()) {
            (void)memoryInit();
        } else {
            memoryProcess();
        }

        (void)repRtosDelayMs(MemoryTaskInterval);
    }
}
```

注意：

- `vfsInit()` 由 memory task 调用，不再由 `sysmgr` 提前调用。
- `memoryInit()` 当前被多个 public API 懒加载调用，迁移后仍需要保持幂等。
- 自检应读取 `memoryIsReady()` 或 memory status，不要由 `sysmgr` 直接阻塞等待挂载完成。

### Wireless

`wirelessInit()` 会加载存储配置、配置 FC41D 并启动 BLE/Wi-Fi。它适合放入 `wirelessTaskEntry()`，但需要考虑 memory 依赖。

建议策略：

- 如果 `memoryIsReady()`，加载存储配置。
- 如果 memory 超时未 ready，使用默认无线配置启动。
- wireless task 内部继续周期性调用 `wirelessProcess()`，在 memory ready 后可重新加载配置。

建议结构：

```c
static void wirelessTaskEntry(void *argument)
{
    (void)argument;

    for (;;) {
        if (*wirelessGetStatus() == eWIRELESS_STATE_INIT) {
            (void)wirelessInit();
        } else {
            wirelessProcess();
        }

        (void)repRtosDelayMs(WirelessTaskInterval);
    }
}
```

注意：

- `wirelessProcess()` 当前已经会执行配置加载、配置检查和启动目标模式，所以可以逐步弱化 `wirelessInit()` 的一次性职责。
- 如果无线初始化失败，不应阻塞系统进入降级运行。

### Audio

`audioInit()` 已经放在 `audioTaskEntry()` 中，这个方向是正确的。它内部有多个 `1000ms` 等待，不能放回系统主初始化。

建议补充：

- `audioTaskEntry()` 不要只启动时初始化一次，应该允许故障后周期性重试。
- `audioProcess()` 当前已经在未 ready 时调用 `audioInit()`，基本满足要求。

### CPR Algorithm

`cprAlgMgrInit()` 已经放在 `cprAlgTaskEntry()` 中，方向正确。

注意：

- CPR 算法依赖 sensor sample queue。
- 初始化可以先完成，但 process 阶段需要容忍 sensor 尚未 ready。

## 系统模式建议调整

当前 worker tasks 在 `systemSelfCheckMode()` 自检完成后才创建：

```c
if(lSelfCheckCompleted) {
    (void)systaskCreateWorkerTasks();
    systemSetMode(eSYSTEM_STANDBY_MODE);
}
```

如果模块初始化迁移到任务内，建议改为在 `systemInitMode()` 完成基础初始化后创建 worker tasks：

```c
if (!systaskCreateWorkerTasks()) {
    LOG_E(SYSTEM_LOG_TAG, "worker task create failed");
    return;
}

gSystemInitModeCompleted = true;
systemSetMode(eSYSTEM_POWERUP_SELFCHECK_MODE);
```

推荐启动流程：

```text
INIT_MODE
  BSP init
  basic module init
  create worker tasks
  switch POWERUP_SELFCHECK_MODE

WORKER TASKS
  module init
  update ready/fault state
  process loop

SELF_CHECK_MODE
  wait startup window
  collect module states
  commit selfcheck
  switch STANDBY_MODE
```

## 自检配合方式

建议每个模块暴露明确状态：

```c
typedef enum {
    MODULE_INIT_PENDING = 0,
    MODULE_INIT_READY,
    MODULE_INIT_FAULT,
} eModuleInitState;
```

或者沿用各模块已有接口：

- `sensorIsReady()`
- `memoryIsReady()`
- `wirelessGetStatus()`
- `power` 可补充 `powerIsReady()`
- `audio` 可补充 `audioIsReady()`

自检逻辑应读取这些状态，并按启动窗口超时判断结果：

- 启动窗口内 ready：记录 pass。
- 启动窗口结束仍 pending/fault：记录 fail。
- 非关键模块失败：允许降级进入 standby/normal。
- 关键模块失败：进入故障或阻止特定功能运行。

## 推荐迁移顺序

1. 让 `powerTaskEntry()` 真正执行 `powerInit()` 和 `powerProcess()`。
2. 从 `systemTaskEntry()` 移除 `powerProcess()`，保留 `drvAdcBackground()`、`powerLedProcess()` 和看门狗刷新。
3. 将 `sensorInit()` 从 `systemModuleInit()` 移入 `sensorTaskEntry()`。
4. 将 `vfsInit()` 和 `memoryInit()` 从 `systemModuleInit()` 移入 `memoryTaskEntry()`。
5. 将 `wirelessInit()` 从 `systemModuleInit()` 移入 `wirelessTaskEntry()`，处理 memory 未 ready 的默认配置启动。
6. 把 `systaskCreateWorkerTasks()` 前移到 `systemInitMode()` 基础初始化完成之后。
7. 调整 `selfCheckFaultRunStartupWindow()` 中各模块结果来源，改为读取模块状态。

## 风险点

- sensor task 优先级高是为了保证实时采样，但初始化失败后频繁重试可能影响低优先级任务和看门狗刷新。
- `wirelessInit()` 依赖存储配置，memory 未 ready 时要有默认配置策略。
- `vfsInit()` 和 `memoryInit()` 当前可能被多个 API 懒加载调用，迁移时要保持幂等和线程安全。
- `sensorInit()` 创建队列失败后重复创建，需要确认 RTOS queue create 失败场景不会泄漏资源。
- 自检窗口开始时间要在 worker tasks 创建之后，否则会统计到无意义的 pending 状态。

## 建议验收点

- 上电后系统不再因为 wireless/memory/audio 单模块失败卡在 init mode。
- `systemTaskEntry()` 周期稳定，能持续刷新 IWDG。
- sensor 未 ready 时 CPR 算法任务不崩溃、不阻塞。
- memory 未 ready 时 wireless 可以按默认配置启动或明确进入降级状态。
- 自检日志能区分 `pending`、`ready`、`fault`，便于定位启动慢和初始化失败。
