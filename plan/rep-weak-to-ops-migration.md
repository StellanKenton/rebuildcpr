# rep 目录 weak 到 ops 迁移计划

## 1. 背景

1. 平台绑定点是隐式的，是否被工程侧覆盖只能靠链接结果判断。
2. 缺少实现时容易静默退回 stub，运行期才暴露问题。
3. core 与项目侧的契约分散在多个 `xxxPlatform*` 函数上，不利于统一管理。
4. 后续新增平台能力时，容易继续复制 weak 模式，导致接口形态越来越散。

本计划的目标是把 `rep/` 中这些 weak 定义统一迁移为显式 `ops/provider` 形式。

说明：

- 本计划不包含 `rep/lib/` 下的汇编弱符号。
- 本计划优先描述迁移顺序、约束和验收方法，不要求一次性完成全部代码改动。

## 2. 目标

迁移完成后，`rep/` 中相关目录应满足以下目标：

1. core 不再依赖 weak hook。
2. core 只通过静态 `ops` 表访问项目侧能力。
3. 项目侧统一在 `User/port/` 提供 `xxxPortGetOps()` 入口。
4. 缺失必需 `ops` 或缺失必需成员时，core 明确失败返回，不再静默使用默认 stub。
5. 目录文档中的契约描述同步改为 `ops/provider` 形式。

## 3. 统一设计约束

### 3.1 总体形态

统一采用以下结构：

1. `rep/` 侧头文件定义 `stXxxOps`。
2. `User/port/xxx_port.h` 声明 `const stXxxOps *xxxPortGetOps(void);`
3. `User/port/xxx_port.c` 提供长期有效的静态 `ops` 表。
4. `rep/` core 内部通过 `static const stXxxOps *xxxGetOps(void)` 获取并消费 `ops`。

参考模板：

- `rep/comm/frameparser/framepareser.c`
- `User/port/frameparser_port.h`
- `User/port/frameparser_port.c`

### 3.2 命名建议

建议统一命名，避免不同目录再次发散：

1. `driver`：`stDrvXxxOps` + `drvXxxPortGetOps()`
2. `module`：`stXxxOps` + `xxxPortGetOps()`
3. `service`：`stXxxOps` + `xxxPortGetOps()`
4. `tools`：`stXxxOps` + `xxxPortGetOps()`

### 3.3 设计边界

1. `ops` 只放 core 真正需要的最小能力。
2. 默认配置加载函数也收敛进 `ops`，例如 `loadDefaultCfg(device, cfg)`。
3. 可选能力允许成员为 `NULL`，但必需能力必须在文档中标明。
4. 不在 core 中继续保留 `xxxPlatformDelayMs()`、`xxxGetPlatformInterface()` 这类分散 provider。

## 4. 迁移范围

### 4.1 driver

1. `rep/driver/drvgpio/drvgpio.c`
2. `rep/driver/drvadc/drvadc.c`
3. `rep/driver/drviic/drviic.c`
4. `rep/driver/drvspi/drvspi.c`
5. `rep/driver/drvuart/drvuart.c`
6. `rep/driver/drvusb/drvusb.c`
7. `rep/driver/drvanlogiic/drvanlogiic.c`
8. `rep/driver/drvmcuflash/drvmcuflash.c`

### 4.2 service 与 tools

1. `rep/tools/ringbuffer/ringbuffer.c`
2. `rep/sys/log/log.c`
3. `rep/sys/log/console.c`
4. `rep/tools/trace/trace.c`
5. `rep/sys/update/update_debug.c`

### 4.3 module

1. `rep/module/lis2hh12/lis2hh12.c`
2. `rep/module/lsm6/lsm6.c`
3. `rep/module/mpu6050/mpu6050.c`
4. `rep/module/pca9535/pca9535.c`
5. `rep/module/tm1651/tm1651.c`
6. `rep/module/gd25qxxx/gd25qxxx.c`
7. `rep/module/w25qxxx/w25qxxx.c`
8. `rep/module/sdcard/sdcard.c`
9. `rep/module/fc41d/fc41d.c`
10. `rep/module/ec800m/ec800m.c`
11. `rep/module/esp32c5/esp32c5.c`
12. `rep/module/wt2003hx/wt2003hx.c`

## 5. 分阶段实施计划

### 阶段 0：恢复构建基线

目标：先保证当前工程中已改为 `ops` 的目录真正接入编译，避免在 weak 迁移前基线已损坏。

当前需要优先处理：

1. `frameparser` 已改成 `frmPsrPortGetOps()` 形态。
2. 当前链接失败显示 `frmPsrPortGetOps` 未定义。
3. 需要先确认 `User/port/frameparser_port.c` 已加入 CMake 和 Keil 编译入口。

验收标准：

1. `Firmware: Build` 不再因为 `frmPsrPortGetOps` 缺失而链接失败。
2. 这一阶段不要求顺带处理其他 warning。

### 阶段 1：建立样板目录

目标：先完成一个最小目录的 weak 到 ops 全链路迁移，固定代码骨架和文档模板。

建议样板优先级：

1. `rep/tools/ringbuffer`
2. `rep/sys/log`
3. `rep/module/lis2hh12`

建议优先选 `lis2hh12` 作为模块样板，因为它同时包含：

1. 默认配置加载
2. 接口 provider
3. 配置校验
4. 延时与重试时序

这一阶段要产出：

1. `stXxxOps` 定义
2. `xxxPortGetOps()` 入口
3. `User/port/xxx_port.h/.c`
4. core 内部统一 `getOps()` 访问
5. 对应 md 的契约更新

验收标准：

1. 目标目录内 weak 定义清零。
2. 功能调用路径只经过 `ops`。
3. 构建通过。

### 阶段 2：批量迁移 driver

目标：优先清理 driver 层 weak，因为这批目录本身就接近 BSP 接口表模式，迁移成本最低。

建议顺序：

1. `drvgpio`
2. `drvadc`
3. `drviic`
4. `drvspi`
5. `drvuart`
6. `drvusb`
7. `drvanlogiic`
8. `drvmcuflash`

实施要点：

1. 尽量复用现有 `stDrvXxxBspInterface`，不要同时大改公共语义。
2. 只把 weak provider 改为显式 `drvXxxPortGetOps()`。
3. 先让调用入口收敛，再考虑后续命名统一。
4. 对应 md 的契约更新

验收标准：

1. `rep/driver/` 中 weak 清零。
2. 依赖 driver 的 module 与 service 仍可正常编译。

### 阶段 3：迁移 service 与 tools

目标：清理基础设施层 weak，统一公共基础能力的绑定方式。

建议顺序：

1. `ringbuffer`
2. `log`
3. `console`
4. `trace`
5. `update_debug`

实施要点：

1. `ringbuffer` 的临界区、屏障接口很适合收敛成小型 `ops` 表。
2. `log` 与 `console` 需要区分必需成员和可选成员。
3. `update_debug` 这类 debug 钩子可以保留为可选 `ops` 成员。
4. 对应 md 的契约更新
验收标准：

1. `rep/sys/` 与 `rep/tools/` 中目标 weak 清零。
2. 缺失可选成员时行为明确退化，不影响主流程。

### 阶段 4：按模块族迁移 module

目标：按相似模式分批改造，避免同时处理多种风格。

建议按三族推进。

第一族：IIC 传感器与扩展器

1. `lis2hh12`
2. `lsm6`
3. `mpu6050`
4. `pca9535`
5. `tm1651`

建议公共 `ops` 成员：

1. `loadDefaultCfg`
2. `getBusInterface`
3. `isValidAssemble` 或 `isValidCfg`
4. `getLinkId`
5. `delayMs`
6. 需要时增加 `getResetDelayMs`、`resetWrite`

第二族：SPI Flash / 存储

1. `gd25qxxx`
2. `w25qxxx`
3. `sdcard`

第三族：UART / transport / control

1. `fc41d`
2. `ec800m`
3. `esp32c5`
4. `wt2003hx`

这一族建议直接做 transport/control 合并 `ops`，不要继续拆成多个分散 provider。

验收标准：

1. 每完成一族，先编译验证，再进入下一族。
2. 完成后 `rep/module/` 中目标 weak 清零。

### 阶段 5：文档与编译入口清尾

目标：确保代码、文档和编译入口三者一致。

每个目录迁移后至少同步以下内容：

1. 叶子目录 md 中的 weak hook 描述改成 `ops/provider` 契约。
2. 若原有 `*_assembly.h` 已不再合适，评估是否改名、收敛或仅保留兼容声明。
3. `User/port/*.c` 必须加入 CMake 与 Keil 编译入口。
4. 工程内搜索 `weak`，确认只剩文档历史描述或 `lib / asm` 例外。

验收标准：

1. `rep/` 除 `lib / asm` 外不再有实际 weak 定义。
2. 文档中不再把 weak 描述为当前有效机制。

## 6. 每一步的固定操作顺序

后续每个目录建议严格按以下顺序执行：

1. 在头文件中定义 `stXxxOps`。
2. 在 `User/port/` 新增 `xxx_port.h/.c`。
3. 在 core 中新增 `static const stXxxOps *xxxGetOps(void)`。
4. 把原 weak 调用点替换为 `ops` 访问。
5. 删除 weak 定义。
6. 更新该目录 md。
7. 接入编译入口。
8. 运行一次构建验证。

这样做的原因是：

1. 可以始终保持编译面清晰。
2. 一旦失败，容易定位在接口定义、编译入口还是实现缺失。
3. 避免出现“代码改完了但 port 文件没进工程”的重复问题。

## 7. 风险与控制点

### 7.1 主要风险

1. 新增 `User/port/*.c` 后未加入编译入口，导致链接失败。
2. 把必需成员误设为可选，运行时退化为静默失败。
3. 一次修改过多目录，导致问题难以回溯。
4. 文档未同步，后续维护者继续按 weak 方式扩展。

### 7.2 控制措施

1. 每批只改一个小族或一个层级。
2. 每个目录完成后立即构建，不累计多批再验证。
3. 每个 `ops` 结构体都写明必需成员与可选成员。
4. 每个目录迁移时同步更新 md，不留到最后统一补文档。

## 8. 里程碑建议

建议按以下里程碑推进：

1. 里程碑 A：修复 `frameparser_port.c` 编译接入，恢复构建基线。
2. 里程碑 B：完成一个样板目录的 weak 到 ops 迁移。
3. 里程碑 C：完成全部 driver 迁移。
4. 里程碑 D：完成 service 与 tools 迁移。
5. 里程碑 E：完成全部 module 迁移。
6. 里程碑 F：完成文档与工程入口清尾，`rep/` 除 `lib / asm` 外 weak 清零。

## 9. 建议的下一步

如果后续按本计划分步执行，建议顺序如下：

1. 先处理 `frameparser_port.c` 编译接入问题。
2. 以 `lis2hh12` 为第一个模块样板完成 weak 到 ops 迁移。
3. 样板稳定后，整批清理 `rep/driver/`。
4. 再推进 `service/tools`。
5. 最后按模块族收尾 `module/`。