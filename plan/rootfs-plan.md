# VFS 规划

## 1. 目标

目标是在当前 MCU 工程里做一个简化版、类 Linux 使用方式的 filesystem 抽象层，后续统一放在 `rep/sys/vfs/` 下。

这个抽象层不直接绑定某一种文件系统库，也不直接绑定某一个硬件外设，而是拆成下面三层：

1. `vfs core`
   负责统一路径语义、挂载点管理、文件/目录基础操作、错误码和生命周期。
2. `filesystem backend`
   负责把 littlefs、fatfs 这类具体库适配成统一的 `vfs` 接口。
3. `block device / partition binding`
   负责把 GD25Qxxx、W25Qxxx、SD 卡、内部 flash 分区等底层介质绑定给具体 backend。

这样后续使用时，可以按项目需要自由组合：

- `vfs + littlefs backend + 外置 flash 分区`
- `vfs + fatfs backend + SD 卡`
- `vfs + fatfs backend + 某个兼容块设备`

## 2. 当前现状

当前项目已经有一部分文件系统能力，但还是项目侧实现，不是可复用抽象：

- `User/manager/memory/` 当前直接绑定 `littlefs + GD25Qxxx`。
- 现有 API 已经覆盖了 `mkdir`、`ls`、`cat`、`tee`、`mv`、`rm`、空间查询等基本能力。
- 现有 console/debug 能力也是围绕这个单一 littlefs 实例展开。

这说明业务侧需求已经成立，但当前实现还存在三个限制：

1. 文件 API 和底层库耦合，未来接入 fatfs 时会重复造一套接口。
2. 文件系统实例和硬件介质耦合，无法自然复用到别的 flash 或块设备。
3. 挂载点和路径空间还是“单实例内存区”思路，不是统一 namespace。

所以后续方向不是继续扩展 `User/manager/memory`，而是把其中可复用的语义上提到 `rep/sys/vfs/`，项目绑定部分留在 `User/`。

## 3. 设计原则

结合当前仓库规则和 MCU 资源约束，`vfs` 需要遵守下面原则：

1. 先做“统一抽象”，不追求 Linux 完整语义。
2. 不做复杂 vnode/inode/cache 体系，避免资源浪费。
3. 不默认依赖动态内存；优先使用固定表和调用方缓冲区。
4. `rep/sys/vfs/` 只放可复用 core 和 backend 适配，不放具体板级绑定。
5. 具体 flash、SPI、SDIO、分区映射继续放在 `User/port/` 或项目侧 manager。
6. littlefs 和 fatfs 的差异由 backend 吸收，不暴露到业务 API。
7. 本次任务只覆盖“文件与目录基础能力”，不做权限、用户、组、链接、设备节点。

## 4. VFS 的边界

### 4.1 VFS 负责什么

- 统一绝对路径语义，例如 `/mem/log.txt`、`/cfg/net.json`。
- 维护挂载点，例如 `/mem`、`/sd`、`/cfg`。
- 负责路径分发：把 `/mem/a.txt` 路由到 littlefs，把 `/sd/b.bin` 路由到 fatfs。
- 对上提供统一 API：`mount`、`unmount`、`format`、`mkdir`、`remove`、`rename`、`stat`、`open/read/write/close`、`opendir/readdir`、`sync`。
- 统一错误码、状态机、只读属性、容量查询。

### 4.2 VFS 不负责什么

- 不直接管理 SPI、QSPI、SDIO、USB MSC 等外设细节。
- 不直接实现 littlefs/fatfs 源码本体。
- 不负责分区探测策略和产品级存储布局决策。
- 不负责系统启动顺序编排，这些仍然属于 `User/system/` 或 `User/manager/`。

## 5. 建议的三层抽象

### 5.1 层一：统一 VFS Core

这是最终给业务使用的一层，建议提供下面几类能力：

#### a. 挂载管理

- `vfsInit()`
- `vfsRegisterMount()`
- `vfsMount()`
- `vfsUnmount()`
- `vfsFormat()`
- `vfsIsReady()`

#### b. 路径与节点操作

- `vfsExists()`
- `vfsMkdir()`
- `vfsRemove()`
- `vfsRename()`
- `vfsStat()`
- `vfsGetSpaceInfo()`

#### c. 文件操作

- `vfsOpen()`
- `vfsClose()`
- `vfsRead()`
- `vfsWrite()`
- `vfsSeek()`
- `vfsTell()`
- `vfsSyncFile()`

#### d. 目录操作

- `vfsDirOpen()`
- `vfsDirRead()`
- `vfsDirClose()`

#### e. 辅助能力

- 路径规范化
- 挂载点查找
- 错误码映射
- 只读与格式化保护

### 5.2 层二：Filesystem Backend 适配层

这一层不对业务暴露具体库，而是定义统一的 backend 操作表，例如：

- `mount`
- `unmount`
- `format`
- `stat`
- `mkdir`
- `remove`
- `rename`
- `fileOpen/fileClose/fileRead/fileWrite/fileSeek/fileSync`
- `dirOpen/dirRead/dirClose`
- `getSpaceInfo`

建议每个 backend 都只实现一份 `vfs backend ops`，这样：

- littlefs backend 负责把 `lfs_*` 映射成 `vfs` 统一语义。
- fatfs backend 负责把 `f_*` 映射成 `vfs` 统一语义。

### 5.3 层三：Block Device / Partition 绑定层

backend 下层不要直接认识 GD25Qxxx 或 SD 卡驱动，而应只认识统一块设备接口。

建议块设备接口只保留最小能力：

- `read`
- `prog/write`
- `erase`
- `sync`
- `getInfo`

其中 `getInfo` 至少给出：

- 逻辑起始地址或分区偏移
- 总容量
- 擦除粒度
- 编程粒度
- 读写对齐要求
- 是否可擦除
- 是否只读

说明：

- littlefs 常常需要显式 `erase`。
- fatfs 更偏块设备语义，可能主要依赖 sector read/write/ioctl。
- 所以块设备抽象可以分成“通用信息 + 按 backend 需要调用的最小动作”，不要一开始做过度统一。

## 6. 一次性落地的目录与文件规划

本次任务不是分阶段试点，而是按一次性完整交付来规划。实现时应直接把 core、backend、项目绑定、兼容层、调试层和文档一起补齐，避免再经历“先 littlefs、后 fatfs、再回头补接口”的二次返工。

建议直接按下面方式创建和修改文件：

```text
rep/sys/vfs/
    vfs.md
    vfs_cfg.h
    vfs_types.h
    vfs_backend.h
    vfs_blockdev.h
    vfs_path.h
    vfs_path.c
    vfs_mount.h
    vfs_mount.c
    vfs.h
    vfs.c
    vfs_copy.h
    vfs_copy.c
    vfs_littlefs.h
    vfs_littlefs.c
    vfs_fatfs.h
    vfs_fatfs.c
    vfs_debug.h
    vfs_debug.c

User/port/
    vfs_littlefs_port.h
    vfs_littlefs_port.c
    vfs_fatfs_port.h
    vfs_fatfs_port.c

User/manager/memory/
    memory.h                // 修改为 vfs 兼容层声明
    memory.c                // 修改为 vfs 兼容层实现
    memory_debug.h          // 修改为 vfs debug 适配入口声明
    memory_debug.c          // 修改为 vfs debug 适配入口实现

User/system/
    sysmgr.c               // 修改挂载初始化接入
    system_debug.c         // 修改调试命令注册接入
```

建议职责如下：

- `vfs.md`
  作为 `rep/sys/vfs/` 的目录权威文档，写明边界、依赖、初始化顺序、路径语义和项目侧接入方式。
- `vfs_cfg.h`
  放编译期配置，例如最大挂载点数、最大打开文件数、最大打开目录数、最大路径长度、跨卷复制缓冲区大小。
- `vfs_types.h`
  统一错误码、节点类型、文件句柄、目录句柄、挂载配置等公共类型。
- `vfs_backend.h`
  backend 操作表定义。
- `vfs_blockdev.h`
  块设备/分区抽象定义，明确 backend 可见的最小能力和设备能力描述。
- `vfs_path.h/.c`
  负责路径规范化、最长挂载点匹配、绝对路径校验、尾随 `/` 处理和路径拆分。
- `vfs_mount.h/.c`
  负责挂载点表、挂载状态机、挂载查找、只读保护、格式化保护。
- `vfs.h/.c`
  作为对外统一 API 入口，负责文件/目录/空间查询能力和对 `path`、`mount`、`backend` 子模块的编排。
- `vfs_copy.h/.c`
  实现跨挂载点 `copy/move` 的公共搬运逻辑、失败清理逻辑和分块复制逻辑。
- `vfs_littlefs.h/.c`
  littlefs 适配层，吸收 `lfs_*` 生命周期、错误码和 raw flash 差异。
- `vfs_fatfs.h/.c`
  fatfs 适配层，吸收 `FRESULT`、卷号、目录项和 sector 设备差异。
- `vfs_debug.h/.c`
  可裁剪 console/debug 辅助能力，统一实现 `mkdir/ls/cd/pwd/cat/tee/mv/rm/df/cp` 这类命令底座。
- `User/port/vfs_littlefs_port.h/.c`
  负责把外置 flash 分区绑定为 littlefs backend 可用的 block device。
- `User/port/vfs_fatfs_port.h/.c`
  负责把项目侧 SD 卡或其他 sector 设备绑定为 fatfs backend 可用的磁盘接口。
- `User/manager/memory/memory.h/.c`
  保留项目原有对外 API，但内部改为通过 `vfs` 调用 `/mem` 挂载点。
- `User/manager/memory/memory_debug.h/.c`
  保留现有调试入口，但内部改为注册 `vfs_debug` 的命令。
- `User/system/sysmgr.c`
  负责产品启动时的 `vfs` 初始化、挂载注册和自动挂载调用。
- `User/system/system_debug.c`
  负责调试命令注册与项目菜单整合。

## 7. VFS 对外语义建议

### 7.1 路径模型

建议先支持绝对路径，统一使用 `/`：

- `/mem`
- `/mem/log.txt`
- `/cfg/net.json`
- `/sd/update.bin`

本次实现建议：

- 支持绝对路径。
- 可以支持当前工作目录，但不要作为本次实现的强依赖。
- 不支持 `.`、`..` 的复杂相对路径也可以接受，但建议 core 内部保留规范化能力，便于以后扩展。
- 需要明确 `/` 是否允许枚举、尾随 `/` 是否等价、重复 `/` 是否折叠、挂载点匹配必须采用最长前缀规则。

### 7.2 挂载点模型

挂载点建议是固定表，不使用动态链表。

每个挂载点至少包含：

- 挂载路径，例如 `/mem`
- backend 类型，例如 littlefs/fatfs
- backend 实例上下文
- 块设备或分区配置
- 是否自动挂载
- 是否只读
- 是否已挂载

### 7.3 句柄模型

考虑 MCU 资源限制，文件句柄与目录句柄都建议采用固定数量表：

- 最大挂载点数编译期配置
- 最大打开文件数编译期配置
- 最大打开目录数编译期配置
- 最大路径长度编译期配置

本次实现不要引入复杂引用计数和多级缓存。

### 7.4 跨挂载点复制与移动语义

`vfs` 后续需要明确区分三类操作：

1. 同挂载点、同卷内的 `rename/move`
2. 跨挂载点的 `copy`
3. 跨挂载点的 `move`

建议统一语义如下：

- 同挂载点、同 backend、同卷内的 `rename/move`，优先走 backend 原生重命名能力。
- 跨挂载点的 `copy`，统一由 `vfs` 层执行“打开源文件 -> 分块读取 -> 打开目标文件 -> 分块写入 -> 关闭文件”。
- 跨挂载点的 `move`，不承诺原子重命名，默认按 `copy + delete` 处理。

这里的“跨挂载点”包括但不限于：

- `littlefs -> fatfs`
- `fatfs -> littlefs`
- 两个不同的 fatfs 实例之间
- 两个不同的 SD 卡之间
- 同一张 SD 卡上的两个不同逻辑卷之间
- 两个不同的外置 flash 分区之间

重要说明：

- 即使两边都是 fatfs，只要它们不是同一个卷，复制本质上仍然是上层 `read + write` 数据搬运，不是底层直接复制。
- 即使两边都是 SD 卡，也不意味着 fatfs 能替 `vfs` 自动完成跨卷文件搬移。
- 只有同卷内的 `rename` 才可能退化为轻量目录项修改，这一类操作成本最低。

因此建议 `vfs` 后续直接提供独立的复制接口，而不是把所有跨挂载点行为塞进 `rename`：

- `vfsCopy(srcPath, dstPath)`
- `vfsMove(srcPath, dstPath)`

并明确规定：

- `vfsRename()` 只保证同挂载点内语义稳定。
- `vfsMove()` 可以跨挂载点，但跨挂载点时不保证原子性。
- `vfsCopy()` 是跨 backend、跨设备的标准能力。

### 7.5 元数据兼容边界

跨 backend 或跨卷复制时，默认只保证“文件内容 + 基本目录结构”可迁移，不保证所有元数据完全保真。

本次实现只统一下面这些最小公共语义：

- 文件内容
- 目录创建
- 文件大小
- 文件/目录类型
- 是否存在

下面这些属性建议视为“可选保留”，不要在本次实现作为强 contract：

- 时间戳
- 文件属性位
- 长文件名相关差异
- 编码页差异
- 特定 backend 私有属性

这样可以降低 littlefs 和 fatfs 之间、fatfs 不同卷之间做复制时的语义冲突。

## 8. 两种 backend 的职责划分

### 8.1 LittleFS Backend

适用场景：

- 外置 NOR flash
- 需要掉电一致性
- 目录和小文件较多
- 需要 wear leveling

建议 backend 吸收的差异：

- `lfs_t`、`lfs_file_t`、`lfs_dir_t` 生命周期管理
- `LFS_ERR_*` 到 `vfs` 错误码映射
- littlefs 的 block 配置与 cache/lookahead 配置
- 格式化、空白分区首次挂载恢复策略

当前项目已有可迁移基础：

- `User/manager/memory/` 中的 littlefs config
- GD25Qxxx 外置 flash 绑定
- 空白区自动格式化逻辑

### 8.2 FatFS Backend

适用场景：

- 兼容性优先
- 需要与 PC、U 盘、SD 卡生态对接
- 更偏 sector/block 设备

建议 backend 吸收的差异：

- `FRESULT` 到 `vfs` 错误码映射
- `FIL`、`DIR`、`FILINFO` 生命周期管理
- 长文件名和编码配置差异
- 挂载盘符/逻辑驱动号与 vfs 路径挂载点的映射

注意：

- fatfs 往往要求底层提供 sector 读写与 ioctl。
- 这和 littlefs 的 raw flash 语义不一样，所以 backend 下层不宜硬凑成一个完全同形接口。

## 9. 建议的统一错误码

建议 `vfs` 定义自己的错误码，不直接向上暴露 `LFS_ERR_*` 或 `FRESULT`：

- `eVFS_OK`
- `eVFS_INVALID_PARAM`
- `eVFS_NOT_READY`
- `eVFS_NOT_FOUND`
- `eVFS_ALREADY_EXISTS`
- `eVFS_NOT_DIR`
- `eVFS_IS_DIR`
- `eVFS_NO_SPACE`
- `eVFS_IO`
- `eVFS_CORRUPT`
- `eVFS_UNSUPPORTED`
- `eVFS_BUSY`

这样业务层不会感知 backend 差异。

## 10. 一次性完成的任务定义

本次任务按一次性完整交付定义，不接受“先搭 contract、后补 backend、再补兼容层”的拆分方式。提交时必须让 `vfs core`、`littlefs backend`、`fatfs backend`、项目绑定、兼容层、调试层和文档同时到位。

### 10.1 必须一次性交付的能力

- 统一 `vfs` 公共 API、公共类型、错误码和挂载点模型。
- 路径规范化、最长挂载点匹配、节点查询、目录遍历、文件读写、空间查询。
- 同挂载点内 `rename`，以及跨挂载点 `copy/move`。
- littlefs backend 与 GD25Qxxx 项目绑定。
- fatfs backend 与项目侧磁盘绑定接口。
- `User/manager/memory` 兼容层迁移到 `vfs`，对外接口名保持不变。
- `vfs_debug` 调试命令以及 `memory_debug` 到 `vfs_debug` 的接线迁移。
- 启动初始化和调试注册接入。
- `rep/sys/vfs/vfs.md` 与上层目录说明同步更新。

### 10.2 一次性实现时必须明确的 contract

- 并发边界：明确哪些 API 允许 task 调用、哪些不允许 ISR 调用，以及是否允许跨任务共享句柄。
- 生命周期：明确 `mount/unmount/format` 与打开中的文件、目录句柄之间的关系。
- 路径规则：明确 `/`、尾随 `/`、重复 `/`、最长前缀匹配、非法路径字符和超长路径处理。
- 覆盖语义：明确 `copy/move/write` 在目标已存在时的行为。
- 失败清理：明确跨挂载点 `copy/move` 中途失败后的目标文件处理策略。
- 只读约束：明确只读挂载点下哪些操作应直接失败。
- 元数据边界：明确哪些属性必须保留，哪些属性允许降级或忽略。

### 10.3 一次性实施任务清单

1. 新建 `rep/sys/vfs/` 目录和权威文档，先把路径模型、挂载模型、句柄模型、错误码模型写死。
2. 实现 `vfs_cfg/types/backend/blockdev/path/mount/core/copy` 这一组公共文件，形成稳定的统一 API。
3. 实现 `vfs_littlefs.*`，把 littlefs 生命周期、错误码和 raw flash 语义完整吸收在 backend 内部。
4. 实现 `vfs_fatfs.*`，把 fatfs 生命周期、盘符映射和 sector 设备语义完整吸收在 backend 内部。
5. 在 `User/port/` 新建 littlefs 与 fatfs 的项目绑定文件，分别对接 GD25Qxxx 和项目侧磁盘设备。
6. 修改 `User/manager/memory/memory.h` 与 `User/manager/memory/memory.c`，保留现有 API，内部全部改走 `/mem` 挂载点。
7. 修改 `User/manager/memory/memory_debug.h` 与 `User/manager/memory/memory_debug.c`，把现有控制台命令切到 `vfs_debug`。
8. 修改 `User/system/sysmgr.c`，在系统初始化中补 `vfsInit()`、挂载注册和自动挂载。
9. 修改 `User/system/system_debug.c`，把调试命令注册切到新的 `vfs_debug` 入口。
10. 更新 `rep/rule/map.md`，补齐 `rep/sys/vfs/` 的目录定位和边界说明；如最终归到 `service` 风格，也要同步修正文档边界。
11. 做完整联调，至少覆盖 `/mem` 单挂载点、`/mem` 与 `/sd` 双挂载点、跨挂载点复制/移动、格式化和异常失败路径。

### 10.4 完成判定

满足下面条件后，才算这次任务完成：

- 仓库中已经存在并接线完毕 `rep/sys/vfs/`、`User/port/`、`User/manager/memory/`、`User/system/` 的对应文件。
- `memory.*` 仍保持现有对外函数名，但内部不再直接持有 littlefs 私有对象。
- 调试命令已经通过 `vfs_debug` 提供，而不是继续直接操作 littlefs 实例。
- 工程至少存在 `/mem` 和 `/sd` 两类挂载接入点，其中 `/mem` 可直接在当前项目落地，`/sd` 的项目绑定接口也必须在本次实现中就位。
- 文档已经说明 API 语义、并发边界、失败语义、文件职责和目录边界。

## 11. 与现有代码的迁移建议

当前最值得复用的不是 `memory.*` 的全部实现，而是其中这些经验：

1. littlefs 挂载参数已经在这个板子上验证过。
2. 外置 GD25Qxxx 的容量探测和区域裁剪策略已经跑通。
3. console 调试命令说明“类 Linux 使用方式”对当前项目是有价值的。

本次实现完成后，代码职责应按下面方式拆分：

- 保留在 `User/manager/memory/`：
  - 当前产品兼容 API
  - 当前产品对 `/mem` 的命名和调用适配
  - 当前产品调试入口对 `vfs_debug` 的转接

- 上提到 `rep/sys/vfs/`：
  - 通用路径分发
  - 通用文件/目录 API
  - littlefs backend 适配
  - fatfs backend 适配
  - 通用错误码和状态机
  - 跨挂载点复制与移动逻辑

- 留在 `User/port/`：
  - GD25Qxxx/W25Qxxx/SD 卡等块设备绑定
  - project-specific 分区信息
  - 平台锁、延时、外设初始化前置条件

## 12. 需要明确避免的几个坑

1. 不要把 `vfs` 做成“只支持 littlefs，fatfs 再补一套兼容层”。这样会再次回到双轨 API。
2. 不要一开始照搬 Linux VFS 全套对象模型。对当前 MCU 资源和项目体量来说过重。
3. 不要把具体 flash 型号、SPI bus、CS 引脚、分区地址写死在 `rep/sys/vfs/` core。
4. 不要让业务层直接接触 `lfs_t`、`FIL`、`DIR` 这类 backend 私有对象。
5. 不要把调试 console 命令和 core 强耦合，调试能力应可裁剪。

## 13. 本次实现必须同步补的文档

这次实现在写代码时必须同步完成下面文档工作，不能留到后续补：

1. 新增 `rep/sys/vfs/vfs.md` 作为该目录权威入口文档。
2. 更新上层入口文档，至少补 `rep/rule/map.md` 对 `vfs/` 的说明。
3. 如果最后把 `vfs` 定位成 `sys` 风格能力，也要同步补充与 `rep/sys/sys.md` 的边界说明，避免目录语义漂移。

## 14. 一次性交付时的内部实现顺序

虽然任务按一次性交付要求推进，但实现时仍建议按下面顺序在同一批改动中完成，以降低返工：

1. 先定 `vfs` 的路径、挂载、错误码、并发和失败语义，再写头文件。
2. 再实现 core 与跨挂载点复制逻辑，避免 backend 各自实现一套搬运语义。
3. 接着同时完成 littlefs backend 与 fatfs backend 的 contract 对接，确保公共接口不是为单一 backend 定制。
4. 然后补 `User/port/` 里的项目绑定文件。
5. 最后再改 `memory.*`、`memory_debug.*`、`sysmgr.c`、`system_debug.c`，完成兼容层和系统接线。

这里的“顺序”只用于同一次实现内部安排，不表示可以拆成多个里程碑交付。

## 15. 结论

这个 `vfs` 不应该被设计成“某个文件系统库的封装”，而应该被设计成“统一路径空间 + 挂载点分发 + backend 适配 + blockdev 绑定”的轻量文件系统框架。

这次规划应直接以“一次性完整落地”为目标：同一批改动里把 core、littlefs、fatfs、项目绑定、兼容层、调试层、文档和接线一起完成。这样才能真正验证抽象是否稳定，也避免后续再因为单 backend 先行而回头改 contract。
