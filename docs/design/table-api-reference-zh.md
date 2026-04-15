# 配置表 API 参考手册

由 Data Table Exporter 自动生成。示例使用 C++ 语法；Go 和 Java 有等价 API。

---

## 目录

- [1. 全局表加载器](#1-全局表加载器)
- [2. Manager 基础查询](#2-manager-基础查询)
- [3. 二级索引 (idx)](#3-二级索引-idx)
- [4. 复合键](#4-复合键)
- [5. 外键 (fk / gfk)](#5-外键-fk--gfk)
- [6. 反向外键 (HasMany)](#6-反向外键-hasmany)
- [7. 表达式列](#7-表达式列)
- [8. 实用方法](#8-实用方法)
- [9. 热更新](#9-热更新)
- [10. Excel 选项行速查](#10-excel-选项行速查)
- [11. 线程安全](#11-线程安全)

---

## 1. 全局表加载器

定义在 `all_table.h` / `all_table.cpp`。

```cpp
// 串行加载：先全部 Load()，再全部 LoadSuccess()，最后全局回调。
void LoadTables();

// 并行加载：每个表在独立线程中 Load()，屏障等待，
// 然后串行调用所有表的 LoadSuccess()，最后全局回调。
void LoadTablesAsync();

// 注册全局回调，在所有表 Load+LoadSuccess 完成后调用。
void OnTablesLoadSuccess(const LoadSuccessCallback& callback);

// 从磁盘重新加载所有表，并递增版本号。
// 警告：旧的 proto 数据指针在此调用后失效。
void ReloadTables();

// 单调递增计数器，每次 ReloadTables() 后加 1。
uint64_t GetTableLoadVersion();
```

### 加载生命周期

```
阶段 1: Load()         -- 解析 pb 文件，构建 idMap + 所有索引（每个表独立，并行安全）
阶段 2: LoadSuccess()  -- 每个表的回调（此时所有表已加载完毕，可安全做跨表组建）
阶段 3: 全局回调        -- OnTablesLoadSuccess 注册的回调
```

---

## 2. Manager 基础查询

每个 `XxxTableManager` 都是单例，提供以下 API。示例使用 `MissionTableManager`。

### FindById / FindByIdSilent

```cpp
// 返回 {行指针, 错误码}。未找到时打印警告日志。
auto [row, err] = MissionTableManager::Instance().FindById(1001);

// 静默版本：未找到时不打日志。
auto [row, err] = MissionTableManager::Instance().FindByIdSilent(1001);
```

### FindAll

```cpp
// 返回完整的 protobuf 数据容器 (MissionTableData)。
const auto& allData = MissionTableManager::Instance().FindAll();
for (int i = 0; i < allData.data_size(); ++i) {
    const auto& row = allData.data(i);
}
```

### GetIdMap

```cpp
// 返回 unordered_map<uint32_t, const MissionTable*>。
const auto& idMap = MissionTableManager::Instance().GetIdMap();
```

### FindByKey（唯一键列）

```cpp
// 对 Excel 中标记了 `key` 选项的列。
auto [row, err] = XxxTableManager::Instance().FindByColumnName(keyValue);
const auto& keyMap = XxxTableManager::Instance().GetColumnNameMap();
```

### FindByIds（批量查询，类似 SQL IN）

```cpp
// 返回匹配的行向量。未找到的 id 静默跳过。
auto rows = MissionTableManager::Instance().FindByIds({1001, 1002, 1003});
```

### Exists

```cpp
bool exists = MissionTableManager::Instance().Exists(1001);

// 对唯一键列：
bool exists = XxxTableManager::Instance().ExistsByColumnName(keyValue);
```

### Count

```cpp
std::size_t total = MissionTableManager::Instance().Count();

// 对多键列：
std::size_t n = XxxTableManager::Instance().CountByColumnName(keyValue);
```

---

## 3. 二级索引 (idx)

在 Excel 第 4 行（选项行）对某列添加 `idx`。加载时构建 `unordered_map<K, vector<const T*>>`。

### Excel 配置

| id | level | ... |
|----|-------|-----|
| uint32 | uint32 | ... |
| common | common | ... |
| **bit_index** | **idx** | ... |

### 生成的 API

```cpp
// 获取所有 level 等于指定值的行（返回 const 引用，零分配）。
const auto& rows = TestMultiKeyTableManager::Instance().GetByLevel(5);

// 获取完整索引 map。
const auto& levelIndex = TestMultiKeyTableManager::Instance().GetLevelIndex();

// 统计 level 等于指定值的行数。
std::size_t n = TestMultiKeyTableManager::Instance().CountByLevelIndex(5);
```

**性能**: O(1) 查找，返回 `const vector&` -- 每次调用零内存分配。

---

## 4. 复合键

在 Excel 第 4 行对多个列添加 `composite:组名`。

### Excel 配置

| id | zone_id | server_id |
|----|---------|-----------|
| uint32 | uint32 | uint32 |
| common | common | common |
| | **composite:ZoneServer** | **composite:ZoneServer** |

### 生成的 API

```cpp
// 按复合键（多列组合）查找。
const auto* row = XxxTableManager::Instance().FindByZoneServer(zoneId, serverId);

// 检查是否存在。
bool exists = XxxTableManager::Instance().ExistsByZoneServer(zoneId, serverId);
```

---

## 5. 外键 (fk / gfk)

### Excel 配置

在第 4 行（选项行）：
- 标量列: `fk:目标表` 或 `fk:目标表.列名`
- repeated 列: `fk:目标表`（自动识别为 repeated）
- 组字段列（repeated）: `gfk:目标表`

### 示例: Mission.xlsx

| id | condition_id | reward_id |
|----|-------------|-----------|
| uint32 | repeated uint32 | uint32 |
| common | common | common |
| bit_index | **gfk:Condition** | **fk:Reward** |

### 生成的 API (C++)

位于 `xxx_table_fk.h`：

```cpp
#include "mission_table_fk.h"

// ----- 标量外键: reward_id -> Reward -----

// 传行引用：
const RewardTable* reward = GetMissionRewardIdRow(missionRow);

// 传 mission id：
const RewardTable* reward = GetMissionRewardIdRow(missionId);

// ----- repeated 外键 (gfk): condition_id[] -> Condition -----

// 传行引用：
std::vector<const ConditionTable*> conditions = GetMissionConditionIdRows(missionRow);

// 传 mission id：
std::vector<const ConditionTable*> conditions = GetMissionConditionIdRows(missionId);
```

### 生成的 API (Go)

位于 `mission_table_fk.go`：

```go
import "table"

// 传行：
conditions := table.GetMissionConditionIdRows(row)
reward, ok := table.GetMissionRewardIdRow(row)

// 传 id：
conditions := table.GetMissionConditionIdRowsById(missionId)
reward, ok := table.GetMissionRewardIdRowById(missionId)
```

### 生成的 API (Java)

位于 `MissionTableForeignKeys.java`：

```java
// 传行：
List<ConditionTable> conditions = MissionTableForeignKeys.getConditionIdRows(row);
RewardTable reward = MissionTableForeignKeys.getRewardIdRow(row);

// 传 id：
List<ConditionTable> conditions = MissionTableForeignKeys.getConditionIdRows(tableId);
RewardTable reward = MissionTableForeignKeys.getRewardIdRow(tableId);
```

### 外键是无状态的

外键 helper 是**内联函数，每次调用实时查询**目标表的当前 Snapshot。不构建缓存数据结构，因此：

- 热更新后无需重建
- `ReloadTables()` 后下一次调用自动看到最新数据
- 线程安全（读取不可变 Snapshot）

---

## 6. 反向外键 (HasMany)

为**标量** FK 列自动生成。加载时构建 `unordered_map<K, vector<const T*>>` 索引。

```cpp
// 查找所有引用 reward_id == 2001 的 Mission 行。
const auto& missions = FindMissionRowsByRewardId(2001);

// Go：
missions := table.FindMissionRowsByRewardId(2001)

// Java：
List<MissionTable> missions = MissionTableForeignKeys.findRowsByRewardId(2001);
```

**注意**: 反向外键仅针对标量 FK 列生成，不针对 repeated/gfk 列。

---

## 7. 表达式列

在 Excel 第 4 行添加 `expr:double` 或 `expr:int`。列值为表达式字符串（如 `"x*2+1"`）。

```cpp
// 对指定行 id 求表达式值。
double value = XxxTableManager::Instance().GetColumnName(tableId);

// 在求值前设置表达式参数。
XxxTableManager::Instance().SetColumnNameParam({playerLevel, attackPower});
```

---

## 8. 实用方法

### Where（条件过滤）

```cpp
// 返回所有满足条件的行。
auto rows = MissionTableManager::Instance().Where([](const MissionTable& row) {
    return row.mission_type() == 1;
});
```

### First

```cpp
// 返回第一个满足条件的行，未找到返回 nullptr。
const auto* row = MissionTableManager::Instance().First([](const MissionTable& row) {
    return row.mission_type() == 1;
});
```

### RandOne

```cpp
// 随机返回一行，表为空时返回 nullptr。
const auto* row = MissionTableManager::Instance().RandOne();
```

### SetLoadSuccessCallback（每个表独立）

```cpp
// 注册在 LoadSuccess() 阶段调用的回调。
// 此时所有表已完成 Load()，可安全做跨表操作。
MissionTableManager::Instance().SetLoadSuccessCallback([]() {
    // 在这里构建自定义的跨表数据结构。
});
```

---

## 9. 热更新

### 工作原理

1. `ReloadTables()` 调用 `LoadTables()`，重建所有 Snapshot。
2. 每个 `Load()` 创建全新的 `Snapshot`（新的 idMap、新的索引、新的数据）。
3. 旧 Snapshot 被原子替换（`unique_ptr` swap）。
4. `LoadSuccess()` 回调在所有表加载完毕后触发。
5. 全局 `loadSuccessCallback` 最后触发。
6. 版本号递增。

### 安全性

| 操作 | 效果 |
|------|------|
| 增加行 | 新 Snapshot 包含新行 + 更新的索引 |
| 删除行 | 新 Snapshot 不包含已删行 |
| 修改行 | 新 Snapshot 包含修改后的数据 |
| FK 查询 | 无状态，始终查询当前 Snapshot |
| idx 查询 | 索引在 Load() 中重建，返回新数据的 const 引用 |
| 反向 FK | 索引在 Load() 中重建，返回新数据的 const 引用 |

### 注意事项

`ReloadTables()` 之后，任何持有的旧 Snapshot 的**原始指针**或 **string_view** 都将**悬垂**。重载后必须重新查询。

---

## 10. Excel 选项行速查

Excel 第 4 行定义每列的选项（空格分隔）：

| 选项 | 说明 | 示例 |
|------|------|------|
| `key` | 唯一键列（生成 FindByXxx, ExistsByXxx） | `key` |
| `multi` | 多键列（允许重复，生成 CountByXxx） | `multi` |
| `idx` | 二级索引（生成 GetByXxx，O(1) 向量查询） | `idx` |
| `bit_index` | 位索引，用于领取状态追踪 | `bit_index` |
| `fk:表名` | 外键，指向 表名.id | `fk:Reward` |
| `fk:表名.列` | 外键，指向 表名.指定列 | `fk:BaseScene.scene_id` |
| `gfk:表名` | 组外键（repeated 字段指向 表名.id） | `gfk:Condition` |
| `composite:名称` | 复合键分组 | `composite:ZoneServer` |
| `expr:类型` | 表达式列 | `expr:double` |

---

## 11. 线程安全

- **加载阶段**: 每个表在独立线程中加载（`LoadTablesAsync`），只访问自身数据，无跨表依赖。安全。
- **读取阶段**: 所有读取通过不可变 Snapshot 进行。多线程可并发读取。安全。
- **重载**: `ReloadTables()` 须在单线程（主线程/加载线程）中调用。swap 后，读者在下次访问时看到新数据。
- **FK helper**: 无状态内联函数，读取当前 Snapshot。线程安全。

---

## 速查表：命名规范

| 模式 | C++ | Go | Java |
|------|-----|-----|------|
| 单例 | `XxxTableManager::Instance()` | `XxxTableManagerInstance` | `XxxTableManager.getInstance()` |
| 按 id 查找 | `FindById(id)` | `FindById(id)` | `findById(id)` |
| FK (传行) | `GetXxxColRow(row)` | `GetXxxColRow(row)` | `getColRow(row)` |
| FK (传id) | `GetXxxColRow(id)` | `GetXxxColRowById(id)` | `getColRow(id)` |
| FK repeated (传行) | `GetXxxColRows(row)` | `GetXxxColRows(row)` | `getColRows(row)` |
| FK repeated (传id) | `GetXxxColRows(id)` | `GetXxxColRowsById(id)` | `getColRows(id)` |
| 反向 FK | `FindXxxRowsByCol(key)` | `FindXxxRowsByCol(key)` | `findRowsByCol(key)` |
| 二级索引查询 | `GetByCol(key)` | `GetByCol(key)` | `getByCol(key)` |
| 条件过滤 | `Where(pred)` | -- | -- |
| 首条匹配 | `First(pred)` | -- | -- |
| 随机一条 | `RandOne()` | -- | -- |
