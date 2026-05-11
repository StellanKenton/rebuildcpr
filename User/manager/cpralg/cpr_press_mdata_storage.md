---
doc_role: implementation-spec
layer: manager
module: cpralg
status: draft
portability: project-bound
public_headers:
    - cpralgmgr.h
core_files:
    - cpralgmgr.c
depends_on:
    - ../memory/memory.h
---

# CPR 按压数据 mdata 存储实现说明

## 1. 目标

每次算法识别到一次有效按压时，把本次按压数据追加保存到 `/mdata` 目录下的二进制文件中。

保存字段来自当前 `CPR_Data_Typedef`：

| 字段 | 类型 | 来源 |
| --- | --- | --- |
| `Depth` | `uint8_t` | `s_CPR_Data.Depth` |
| `Freq` | `uint8_t` | `s_CPR_Data.Freq` |
| `RealseDepth` | `uint8_t` | `s_CPR_Data.RealseDepth` |
| `TimeStamp` | `uint32_t` | `s_CPR_Data.TimeStamp` |

`BootTimeStamp` 不写入每条按压记录。它只用于生成本次开机的数据文件名。

## 2. 文件命名

所有文件放在 memory 模块管理的 `/mdata` 目录下，也就是 littlefs 的 `/mem/mdata`。

同一次开机期间，`BootTimeStamp` 固定不变。文件名前缀使用 `BootTimeStamp` 的 8 位大写十六进制字符串：

```text
/mdata/<BootTimeStamp>.bin
/mdata/<BootTimeStamp>-1.bin
/mdata/<BootTimeStamp>-2.bin
...
```

示例：

```text
/mdata/00012A8F.bin
/mdata/00012A8F-1.bin
/mdata/00012A8F-2.bin
```

规则：

- 第一个文件不带 `-0` 后缀。
- 当前文件达到本规格定义的可写上限后，切换到下一个文件。
- 设备重启后如果 `BootTimeStamp` 变了，直接另起新的文件名前缀。
- 不需要扫描、删除或合并旧 `BootTimeStamp` 的文件。

## 3. 单条记录格式

磁盘格式固定为 7 字节，禁止直接 `sizeof(struct)` 写入，避免编译器 padding 导致记录长度变化。

字节布局：

| 偏移 | 长度 | 内容 |
| --- | --- | --- |
| 0 | 1 | `Depth` |
| 1 | 1 | `Freq` |
| 2 | 1 | `RealseDepth` |
| 3 | 4 | `TimeStamp`，little-endian |

序列化示例：

```c
static void cprAlgMgrEncodePressRecord(const CPR_Data_Typedef *data, uint8_t record[7])
{
    record[0] = data->Depth;
    record[1] = data->Freq;
    record[2] = data->RealseDepth;
    record[3] = (uint8_t)(data->TimeStamp & 0xFFU);
    record[4] = (uint8_t)((data->TimeStamp >> 8U) & 0xFFU);
    record[5] = (uint8_t)((data->TimeStamp >> 16U) & 0xFFU);
    record[6] = (uint8_t)((data->TimeStamp >> 24U) & 0xFFU);
}
```

## 4. 文件容量

单个文件最大按 300 条完整按压记录控制：

```c
#define CPR_ALG_MGR_MDATA_RECORD_SIZE         7U
#define CPR_ALG_MGR_MDATA_RECORDS_PER_FILE    300U
#define CPR_ALG_MGR_MDATA_FILE_MAX_SIZE       \
    (CPR_ALG_MGR_MDATA_RECORD_SIZE * CPR_ALG_MGR_MDATA_RECORDS_PER_FILE)
```

计算结果：

```text
300 * 7 = 2100 字节
```

因此每个文件最多写 300 条完整按压记录，文件最大数据长度为 2100 字节。不要把一条记录拆到两个文件中。

切换条件：

- 当前文件已经写入 `300` 条记录时，下一条记录写入下一个文件。
- 或者通过 `memoryGetFileSize()` 发现当前文件大小加 7 会超过 2100 时，先切换到下一个文件再写。

## 5. 目录总容量

`/mdata` 目录最多保留 30000 条完整按压记录：

```c
#define CPR_ALG_MGR_MDATA_MAX_RECORDS         30000U
#define CPR_ALG_MGR_MDATA_MAX_FILES           \
    (CPR_ALG_MGR_MDATA_MAX_RECORDS / CPR_ALG_MGR_MDATA_RECORDS_PER_FILE)
```

计算结果：

```text
30000 / 300 = 100 个满文件
```

超过 30000 条时，删除最旧的一个文件。这里的“最旧”按文件名中的 `BootTimeStamp` 和分片序号判断：

1. `BootTimeStamp` 数值更小的文件更旧。
2. `BootTimeStamp` 相同时，分片序号更小的文件更旧。
3. 无后缀文件按分片序号 `0` 处理，例如 `00012A8F.bin` 早于 `00012A8F-1.bin`。

目录清理应在追加新记录前执行。推荐逻辑：

1. 遍历 `/mdata` 目录，只统计符合命名规则的 `.bin` 文件。
2. 每个文件的记录数按 `size / 7` 计算，非 7 整数倍的尾部字节忽略。
3. 如果 `当前总记录数 + 1 <= 30000`，不删除文件。
4. 如果会超过 30000，删除最旧的一个文件。
5. 删除后重新计算，直到 `当前总记录数 + 1 <= 30000`。

注意：一次只需要为即将写入的 1 条记录腾空间，但实现可以使用 `while`，这样能处理异常文件或总数已经明显超限的情况。

## 6. 集成位置

当前有效按压的判断点在 `cprAlgMgrProcess()`：

```c
if (lCprRst.SinglePressUpdated) {
    s_CPR_Data.Depth = cprAlgMgrClampToU8(lCprRst.CPRDepth);
    s_CPR_Data.Freq = cprAlgMgrClampToU8(lCprRst.CPRRate);
    s_CPR_Data.RealseDepth = cprAlgMgrClampToU8(lCprRst.CPRReleaseDepth_Instantaneous);
    lPressTimeStamp = repRtosGetTickMs();
    s_CPR_Data.TimeStamp = lPressTimeStamp;
    ...
}
```

实现时在这里取一份本次按压快照，并在退出 critical section 后写入文件：

```c
CPR_Data_Typedef lPressData;
bool lShouldStorePress = false;

repRtosEnterCritical();
...
if (lCprRst.SinglePressUpdated) {
    ...
    lPressData = s_CPR_Data;
    lShouldStorePress = true;
}
...
repRtosExitCritical();

if (lShouldStorePress) {
    cprAlgMgrStorePressRecord(&lPressData);
}
```

不要在 `repRtosEnterCritical()` 和 `repRtosExitCritical()` 之间调用 `memoryAppendFile()`，因为文件系统写入可能耗时。

本功能第一版应放在 CPR 任务逻辑里触发，不放到 `memoryTask` 里做业务判断：

- `cprAlgMgrProcess()` 是当前唯一能准确知道 `lCprRst.SinglePressUpdated` 的位置，适合在这里判定“本次有效按压是否需要保存”。
- `memoryTask` 目前只负责 `memoryInit()` 和 `memoryProcess()`，其中 `memoryProcess()` 为空；它不应该反向理解 CPR 的 `Depth/Freq/RealseDepth/BootTimeStamp` 业务含义。
- 如果让 `memoryTask` 轮询 `s_CPR_Data` 或 `DataReady`，需要额外处理去重和并发，容易重复写或漏写。
- CPR manager 调用 `memoryAppendFile()`、`memoryDelete()`、`memoryListDir()` 这一层接口即可，不需要绕过 memory 模块直接操作 VFS/littlefs。

推荐第一版结构：

```text
cprAlgTask
  -> cprAlgMgrProcess()
      -> 检测到 SinglePressUpdated
      -> 在 critical section 内更新 s_CPR_Data 并复制一份本次按压快照
      -> 退出 critical section
      -> cprAlgMgrStorePressRecord(&lPressData)
          -> 使用 memory 模块接口追加写入 /mdata 文件
```

如果后续实测发现 flash 写入或目录清理耗时影响 `cprAlgTask` 的实时性，再升级为异步方案：

```text
cprAlgTask
  -> 只把 7 字节按压记录投递到一个小队列

storage worker
  -> 从队列取记录
  -> 执行 /mdata 文件切换、追加写入、超过 30000 条后的最旧文件删除
```

异步方案中，`memoryTask` 仍建议保持为 memory 模块自己的初始化/维护任务；CPR 按压历史存储可以是 `cpralg` 内部的 worker 或单独的业务存储 worker，不建议把 CPR 业务状态解析写进 `memoryProcess()`。

## 7. 状态变量

建议在 `cpralgmgr.c` 内新增静态状态：

```c
static uint32_t sCprAlgMgrMdataBootTimeStamp = 0U;
static uint32_t sCprAlgMgrMdataFileIndex = 0U;
static uint32_t sCprAlgMgrMdataRecordCount = 0U;
static bool sCprAlgMgrMdataReady = false;
```

含义：

- `sCprAlgMgrMdataBootTimeStamp`：当前存储状态对应的 `BootTimeStamp`。
- `sCprAlgMgrMdataFileIndex`：当前文件序号，`0` 表示无后缀文件，`1` 表示 `-1`。
- `sCprAlgMgrMdataRecordCount`：当前文件已经写入的完整记录数。
- `sCprAlgMgrMdataReady`：`/mdata` 目录和当前文件状态是否已经初始化。

当传入数据的 `BootTimeStamp` 与 `sCprAlgMgrMdataBootTimeStamp` 不一致时，重新初始化这组状态，从 index 0 开始写新文件。

## 8. 初始化与追加流程

推荐实现 6 个静态函数：

```c
static bool cprAlgMgrEnsureMdataDir(void);
static void cprAlgMgrBuildMdataPath(uint32_t bootTimeStamp, uint32_t fileIndex, char *path, uint32_t pathSize);
static bool cprAlgMgrParseMdataFileName(const char *name, uint32_t *bootTimeStamp, uint32_t *fileIndex);
static bool cprAlgMgrCleanupMdataIfNeeded(void);
static bool cprAlgMgrPrepareMdataFile(const CPR_Data_Typedef *data);
static bool cprAlgMgrStorePressRecord(const CPR_Data_Typedef *data);
```

### 8.1 目录创建

`cprAlgMgrEnsureMdataDir()`：

- 如果 `memoryExists("/mdata")` 返回 true，直接成功。
- 否则调用 `memoryMkdir("/mdata")`。
- 如果创建失败，记录日志并放弃本次存储。

### 8.2 路径生成

`cprAlgMgrBuildMdataPath()`：

```c
if (fileIndex == 0U) {
    snprintf(path, pathSize, "/mdata/%08lX.bin", (unsigned long)bootTimeStamp);
} else {
    snprintf(path, pathSize, "/mdata/%08lX-%lu.bin",
             (unsigned long)bootTimeStamp,
             (unsigned long)fileIndex);
}
```

路径 buffer 建议使用 `VFS_PATH_MAX` 或至少 64 字节。

### 8.3 文件名解析

`cprAlgMgrParseMdataFileName()`：

- 接受 `XXXXXXXX.bin` 和 `XXXXXXXX-N.bin` 两种格式。
- `XXXXXXXX` 必须解析为 8 位十六进制 `BootTimeStamp`。
- `N` 必须解析为十进制分片序号。
- `XXXXXXXX.bin` 的分片序号返回 0。
- 不符合规则的文件直接忽略，不参与 30000 条上限计算和最旧文件删除。

### 8.4 目录清理

`cprAlgMgrCleanupMdataIfNeeded()`：

1. 调用 `memoryListDir("/mdata", visitor, context, &entryCount)` 遍历文件。
2. 对每个合法 `.bin` 文件调用 `memoryGetInfo()` 或使用 visitor 提供的 `size` 统计记录数。
3. 找出最旧文件路径。
4. 如果总记录数加 1 会超过 `CPR_ALG_MGR_MDATA_MAX_RECORDS`，调用 `memoryDelete(oldestPath)` 删除最旧文件。
5. 删除文件后，如果被删除的是当前缓存的 `sCprAlgMgrMdataBootTimeStamp/sCprAlgMgrMdataFileIndex`，要把 `sCprAlgMgrMdataReady` 置为 false，让后续写入重新准备当前文件。

### 8.5 当前文件准备

`cprAlgMgrPrepareMdataFile()`：

1. 确保 `/mdata` 目录存在。
2. 调用 `cprAlgMgrCleanupMdataIfNeeded()`，保证本次追加不会让目录超过 30000 条记录。
3. 如果 `BootTimeStamp` 变化，重置 file index 和 record count。
4. 生成当前路径。
5. 调用 `memoryGetFileSize()`：
   - 文件存在：用 `size / 7` 作为当前记录数。
   - 文件不存在：当前记录数为 0。
6. 如果当前文件已经有 300 条记录，递增 file index，切换到下一个路径。
7. 重复检查，直到找到能写入完整记录的文件。

注意：如果发现文件大小不是 7 的整数倍，不继续追加到这个文件。直接切到下一个文件，避免破坏按压包对齐。

### 8.6 写入记录

`cprAlgMgrStorePressRecord()`：

1. 校验 `data != NULL`。
2. 调用 `cprAlgMgrPrepareMdataFile(data)`。
3. 把 `Depth/Freq/RealseDepth/TimeStamp` 编码成 7 字节。
4. 调用 `memoryAppendFile(path, record, sizeof(record))`。
5. 写入成功后 `sCprAlgMgrMdataRecordCount++`。
6. 如果写入失败，只记录日志并返回 false，不影响算法主流程、显示和音频提示。

## 9. 依赖与头文件

`cpralgmgr.c` 需要新增 include：

```c
#include "../memory/memory.h"

#include <stdio.h>
```

如果使用 `VFS_PATH_MAX` 作为路径 buffer，`memory.h` 已经包含 `vfs.h`。

## 10. 日志建议

使用已有 `gCprAlgMgrLogTag`。

建议只在异常或切换文件时打日志，避免每次按压都刷日志：

```c
LOG_I(gCprAlgMgrLogTag, "mdata switch file boot=0x%08lX index=%lu",
      (unsigned long)bootTimeStamp,
      (unsigned long)fileIndex);

LOG_E(gCprAlgMgrLogTag, "mdata append fail path=%s", path);
```

删除最旧文件时建议打日志：

```c
LOG_I(gCprAlgMgrLogTag, "mdata delete oldest path=%s records=%lu",
      path,
      (unsigned long)recordCount);
```

## 11. 验证点

实现完成后至少验证：

1. 启动后第一次有效按压会创建 `/mdata/<BootTimeStamp>.bin`。
2. 每条按压记录长度固定 7 字节。
3. 一个文件写满 300 条后大小为 2100 字节。
4. 第 301 条写入 `/mdata/<BootTimeStamp>-1.bin`。
5. 同一次开机多次按压都使用同一个 `BootTimeStamp` 前缀。
6. 重启后 `BootTimeStamp` 变化时另起新前缀文件。
7. `/mdata` 总记录数达到 30000 后，再写入 1 条会先删除最旧的一个文件。
8. 最旧文件按 `BootTimeStamp` 优先、分片序号其次判断。
9. memory 写入失败时不阻塞 `cprAlgMgrProcess()` 的主功能。
