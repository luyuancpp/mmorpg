# 新旧版本混跑时的玩家持久字段保护

> **状态**：设计稿 v0.3（2026-10-19，Claude）。**未落码、未编译、未测试**（AGENTS.md §10.1）。经两轮只读对抗评审：v0.1 被推翻三处核心做法，v0.2 又发现一个必现的误报（S-A）以及若干缺口。v0.3 已按两轮结论修订，明细见 §15。负责人评审通过后按 §9 分批实施，每批由 Codex 验证。
> **起因**：2026-10-18 核对灰度 / 不停服 / 金丝雀时发现，新旧版本混跑会丢玩家持久字段。负责人 2026-10-19 要求先补三件事：①存盘保留不认识的字段；②Redis 父快照记录写方版本并校验；③go/db 只写改动列。先出设计。
> **范围**：玩家数据链，即 C++ scene 内存 → Redis 父快照 `PlayerAllData:{id}` → Kafka `db_task_zone_*` → go/db → MySQL 两张玩家表；外加 login 冷加载、回档快照、离线工具这些旁路。guild、mail、trade 各自的库表，以及客户端契约，不在本文范围内（§12）。
> **证据口径**：[C] 表示对照 2026-10-19 工作区代码读过；[I] 表示推断。相关文件正被其他会话修改，行号会漂移，所以正文以函数名为主。

---

## 0. 结论速览

1. **根因**：链路上有几个环节会**按本版本认识的字段重新拼一份数据**，再整份写回。新版加的字段因此在旧版手里被抹掉（§1.3 的 L1–L7），而且全程不报错（L8）。
2. **一条不变量统一三件事**：每张玩家表消息带一个**覆盖世代** `persist_schema_gen`，意思是"这份数据里，这个世代及以前定义的字段，值都是真实的"。数据每经过一道读写，只有两种合法结局：
   - **完整保留**，输出世代 ≥ 输入；
   - **拒绝或延后**，不产出任何副本。
3. **三个机制**，与负责人提的三件事一一对应：
   - **机制一 · 未知字段跟着对象走**：
     - 重建层改为把未知字段保存在运行时对象里；做不到的消息类型一律冻结，冻结类型不许再加字段。
     - 一个通用的**往返一致性测试**机械地守住这条规则。
     - 有一类语义耦合，"跟着对象走"也挡不住：合堆、等价判定、逻辑重置、跨实体引用。对这类情况，旧版代码对"带着未知字段的对象"拒绝执行这些操作（§4.4）。
   - **机制二 · 覆盖世代校验**：
     - Redis 严格提交屏障**只在同一份数据的第一次写入时**比较"加载时世代 ≥ 已记录世代"，重放请求跳过比较；hash 只升不降。
     - login 加载时做同样的门禁。
     - 违反就判定"链路上有人截断过数据"，fail-closed。
   - **机制三 · go/db 不写写方不认识的列**：
     - body 世代超过本实例世代：延后；
     - body 世代低于 SQL 行世代：拒写，进死信；
     - 未盖戳的 body（引导期关闭后）：拒写，进死信；
     - 其余：整行写。

     不做"只写改动列"，也不做"按列掩码写"，理由见 §6.1。**这一条需要负责人确认。**
4. **世代号由生成器发**：台账是 `proto/persist_schema_gen.txt`（仿 `proto/message_id.txt`）。不能安全携带的改动，在生成期就失败（§3.2）。
5. **上线与回滚**：
   - 先上线机制本身，引导期内禁止升世代；
   - 此后每次升世代，按"DDL → go/db 全部 zone → login → scene（可以金丝雀）"的顺序发；
   - go/db 回滚到更低世代，只会卡住，不丢数据；
   - scene 回滚有前提：新版本没有扩展持久值域。在 §12 的删数据点修好之前，这个前提必须在发布单里写明（§8.3）。
6. **Java 版**：存储是单个 BLOB，机制三不适用，机制一、二同样需要，本批只登记（§11）。

---

## 1. 问题与现状

### 1.1 数据流

```text
C++ scene（ECS 组件，内存是权威）
  │  MarshalPlayerForSave：从组件重拼 PlayerAllData
  ▼
Redis 父快照  PlayerAllData:{id}（PlayerAllData 原始字节）
  │  严格提交屏障 Lua：写父快照 + save_barrier.pending + 完整投递意图
  ▼
Kafka db_task_zone_*  DBTask{body = player_database / player_database_1 的字节}
  ▼
go/db：Unmarshal(body) → 行内围栏事务（FOR UPDATE 读当前行）→ proto2mysql tx.Save
  ▼
MySQL player_database / player_database_1（每个顶层字段一列；子消息存成二进制 PB 的 BLOB）

冷加载（父快照缺失时）：
login → go/db 读任务 → SELECT 本版本认识的列 → 读回执 → login 组装父快照，
两表 SQL ACK 与行围栏都等于 pending 才发布
```

### 1.2 已经能保留未知字段的环节 [C]

| 环节 | 为什么能保留 |
|---|---|
| Go 侧中间跳：login 冷加载组装、`backfillPlayerIdentity`、go/db 写前 Unmarshal | Go protobuf 默认保留未知字段 |
| proto2mysql 的子消息、map、list 列 | `pbconv.SerializeFieldValue` 用 `proto.Marshal` 编码 |
| C++ 中以 PB 直接存放的组件 | 加载时 `emplace<Comp>(player, message.xxx())` 拷贝整份，存盘时 `CopyFrom` 带回。加载期的修正都是就地改字段：`ApplyClassInitialAttributesOrRevive`、`ClampToMaxLevel`、`PlayerAttributeSystem::InitializeOnLoad`、`PetSystem::InitializeOnLoad`、各账本的 `RemoveLast` / `DeleteSubrange`（评审核实）。运行期也没有高频整体替换持久组件；唯一的整块替换是瞬移，会替换 Vector3 / Rotation |
| 物品实例 | `ItemEntry.instance = 15` 承载整份 `ItemComp`（`bag_marshal.cpp`） |

### 1.3 会丢字段的环节 [C]

| 编号 | 位置 | 丢什么 |
|---|---|---|
| L1 | `SavePlayerToRedisImpl` 新建空消息交给 `MarshalPlayerForSave`；退出收敛比较、`discardedProbe` 也一样（`player_lifecycle.cpp`） | 两张表消息的**顶层未知字段**，也就是新增的整列 |
| L2 | `PlayerCurrencyComp::SaveToProto` 先 `clear_debts()`，再逐字段重建（`player_currency_comp.h`） | `CurrencyDebtEntry` 的新字段（资产路径） |
| L3 | `mission_marshal::Marshal` 先 `Clear()` 再重建；`Unmarshal` 只拷两个 map | QuestAllData / QuestScopeData / MissionListComp 的顶层新字段 |
| L4 | `bag_marshal::Marshal` 先 `Clear()`，再重建布局层；`ItemEntry.equip` 由 `ItemComp.equip` 重新算出，是投影 | 布局层的新字段 |
| L5 | `SanitizeSkillList` 新建组件后整体替换（`player_skill.cpp`） | PlayerSkillListComp 的顶层新字段 |
| L6 | `SnapshotSystem::CaptureAndSend` 直接拼一份新消息（`snapshot_system.cpp`） | 同 L1，丢在回档快照里 |
| L7 | go/db 写（`tx.Save` 整行 UPDATE）和读（只 SELECT 已知列）都只覆盖本版本认识的列 | 新版 go/db 把旧写方 body 里缺的新列写成零值；旧版 go/db 冷加载读出截断的副本 |
| L8 | 全链路 | 没有写方版本，也没有覆盖信息，丢了零报错 |

还有一种情况：`stress_test_probe`、`merge_state` 这些已知字段 scene 不 marshal，每次存盘都会被清空（评审核实）。这不属于未知字段问题，处理方式见 §3.2 的冻结清单和 §12。

### 1.4 后果

- **不能金丝雀**：金丝雀本身就是混版本运行。
- **不能回滚**：回滚 scene 后，所有在线玩家的新字段都会被抹掉。
- **丢了不知道**：没有任何报错或指标。

---

## 2. 目标与不变量

- **P1：覆盖世代单调传播。** 每张玩家表消息都带覆盖世代 `c`。每一跳读写只能二选一：
  - 完整保留全部字段，输出 `c' = max(本跳世代, c)`；
  - 拒绝或延后。

  **推论**：只要每一跳都守住 P1，世代大于 `c` 才引入的字段在数据里缺失，就一定是从来没写过，按默认值处理是对的。
- **P2：** 旧版 scene 接手新数据时完整携带新字段；对携带着未知字段的对象，不做可能改变其语义的操作（§4.4）。
- **P3：** go/db 不写写方不认识的列；保存不完整的数据，就延后或拒写。
- **P4：** 违反 P1 的情况，最终一律 fail-closed，配指标和告警，player_id 不进 label。C3 / C4 在引导期可以先用 warn 模式观测（§5.2），残余风险列在 §13。

**非目标**：
- 持久值域扩展（§12）；
- Redis Cluster 跨槽；
- 新旧字段的语义耦合，超出 §4.4 守卫范围的部分。这部分靠 R5 和评审清单约束。

---

## 3. 核心概念：持久 schema 世代

### 3.1 世代号与台账

- **世代号**：`uint32`，从 1 开始单调递增。覆盖从两张玩家表消息（`OptionIsPlayerDatabase`）可达的全部字段。`PlayerAllData` 顶层不经过 SQL，所以**不作为根**，并且冻结。
- **台账** `proto/persist_schema_gen.txt`：由生成器维护，提交进仓库。格式：
  ```text
  1=baseline:<规范化字段集合的 sha256>
  2=player_database.foo_component(22):message:FooComp
  2=CurrencyDebtEntry.interest(9):uint64
  ```
- **生成器的行为**：
  - 只有新增 → 追加世代 `N+1`；
  - 删除且写了 `reserved N;` → 把原行标为 retired；
  - 改号、改类型、复用号 → 生成失败（AGENTS §4.3）。
- **退役字段**：残留字节会作为未知字段一直被携带，因为字段号永不复用，这是无害的。机制三按世代号判断，不看有没有未知字段，所以不会被这些残留卡住。
- **生成产物**（不手改）：C++ 常量 `kPersistSchemaGen`；Go 包 `go/shared/generated/persistschema` 的常量 `Gen`；C++ 往返一致性测试用的"冻结类型 / 投影字段"清单。
- **只在主干发号**：发布分支和 hotfix 分支不得自己追加世代。需要新持久字段时，先合主干拿号，再把台账行原样 cherry-pick 过去。

### 3.2 可携带性规则

| 规则 | 不可携带的改动 | 怎么做 |
|---|---|---|
| R1 | 给**冻结类型**加字段（生成期强制） | 先给这个类型实现运行时携带，并纳入往返测试，再解冻；或者把数据放进可携带的类型，例如物品数据放进 `ItemComp`（AGENTS §11.6） |
| R2 | 给已有的持久 oneof 加新成员（生成期强制） | 字段放到 oneof 之外，或者走破坏性变更流程（整批停机换版，或 expand / contract） |
| R3 | 改号、改类型、复用号（生成期强制） | 用 `reserved` |
| R4 | 字段迁移：子消息字段提升为列、拆分、换存储位置。生成器只能检测"同一世代内既删又增"并给出提示 | expand / contract：新旧位置双写一个完整版本周期，然后再切换读方 |
| R5 | **语义耦合字段**：会参与等价判定、合堆、合并、逻辑重置的字段，或者按 ID 引用其他实体、而这类实体旧代码能删除后重建的字段。生成器无法检测，列入评审清单 | 依赖 §4.4 的守卫（旧代码遇到带未知字段的对象就拒绝做这类操作）；守卫覆盖不到的场景，走破坏性变更流程。跨实体引用只能用永不复用的实例 ID |

**两个 option**，都定义在 `proto/db/proto_option.proto`。生成器从描述符的原始字节动态读取，不需要升级外部模块 `protooption`：

- message 级 `OptionPersistFrozen`：冻结类型。往返测试不在这类节点上注入，但会继续向下遍历它的子字段。
- field 级 `OptionPersistProjection`：投影字段。存盘时由另一个载体重新算出，往返测试不在这个字段上注入。生成器检查：投影字段的类型，必须另有一条非投影路径可达，或者本身就是冻结类型。

**v0.3 的初始冻结清单 / 投影清单** [C]：

| 类型或字段 | 处理 | 原因 |
|---|---|---|
| `PlayerAllData` | 冻结 | 不经过 SQL，`bag_data` / `quest_data` 只是兼容副本 |
| `BagAllData`、`DynamicBagData`、`ItemEntry` | 冻结 | 布局层由 `bag_marshal` 重建。注意 `ItemEntry.instance`（ItemComp）是载体，**不冻结** |
| `ItemEntry.equip` | 投影 | 由 `ItemComp.equip` 重新算出；`EquipInstanceData` 经 `ItemComp.equip` 可达，保持可携带 |
| `QuestAllData`、`QuestScopeData`、`QuestEntry` | 冻结 | 由 `mission_marshal` 重建；`active` / `QuestEntry` 是只读的旧格式。`QuestScopeData.mission_list`（MissionListComp）是载体，不冻结 |
| `MailAllData`、`MailEntry` | 冻结 | 没有写入方 |
| `PlayerStressTestProbe`、`PlayerMergeStateComp` | 冻结 | scene 不 marshal 这两类字段（压测探针、合服标记，由工具写入）。是否属于有意设计，见 §12 |
| `Vector3`、`Rotation` | 冻结 | 共用的几何类型，瞬移时整块替换 |
| map 的 entry 类型 | 不纳入注入，也不纳入覆盖率统计 | 由生成器自动识别（`map_entry` 选项） |

### 3.3 世代戳

- **位置**：两张表消息各加 `uint32 persist_schema_gen`：`player_database` 用 21 号，`player_database_1` 用 5 号。
- **取值约定**：0 表示未盖戳，包括机制上线前写入的数据和 login 的新号种子。比较时 0 一律按 1 处理。所有写入方写入的值都必须 ≥ 1。
- **为什么放在表消息里**：戳和内容在同一份字节里，自动跟着父快照、DBTask body、MySQL 列、子表缓存、跨 zone 迁移、回档快照走；Go 中间跳不认识这个字段也会原样保留；它本身就是一列，go/db 读行时自动带上。
- **加载世代** = 两表戳的**最小值**（0 按 1 处理）。取最小值，是为了不让某一张表被截断的事实被另一张表的高戳洗白。
- **输出世代** = `max(kPersistSchemaGen, 加载世代)`，两张表盖同一个值。
- **save_barrier hash 里的 `schema_gen`**（§5.2）只是索引，不是权威。hash 不存在、或者里面没有这个字段时，**一律不比较**。会出现这种情况的有：机制前的存量玩家、merge_zone 删过 hash、恢复工具重建过 hash、本地 debug_import。

---

## 4. 机制一：未知字段跟着对象走

### 4.1 为什么不从加载原件嫁接

v0.1 的做法是在实体上保存加载原件，存盘时按身份键把原件里的未知字段嫁接回去。评审 S1 证明这会**复活已删除元素的新字段**。

例子：欠款还清后删除，之后 `AttachDebt` 用同一个 `currency_type` 新建欠款，嫁接就把旧欠款的新字段挂到了新欠款上。任务放弃后重新接取，情况相同。

根因是嫁接把未知字段的生命周期，和它所属对象的生命周期拆开了。v0.3 遵循 protobuf 本身的语义：**未知字段只存在于它所属的对象里**，对象被删除，未知字段也随之消失。

### 4.2 改造清单

| 编号 | 改造 |
|---|---|
| L1 | 新增两个持久数据类组件：`PlayerDatabaseCarryComp`、`PlayerDatabase1CarryComp`。各保存对应表消息的**顶层**未知字段，以及该表加载时的戳。<br>**创建点**：分别在手写的 `PlayerDatabaseMessageFieldsUnmarshal` / `PlayerDatabase1MessageFieldsUnmarshal` 里创建，不放在 `InitPlayerFromAllData`。这样经过生成的 `PlayerAllDataMessageFieldsUnmarshal` 加载的测试实体也带有这两个组件（评审 M-E）。<br>存盘入口缺少它们，就整份拒存（持久数据类规则）。<br>顶层未知字段是旧代码完全没有逻辑对应的整列，旧代码不会删除或重置它们，所以这里不会出现 S1 那种复活。按 ID 引用其他实体的新列另当别论，归 R5 / §4.4 管 |
| L2 | 运行时结构 `CurrencyDebt` 增加 `std::string unknownFields`（`UnknownFieldSet` 的序列化）。`LoadFromProto` 时保存，`SaveToProto` 时写回。欠款删除后重建得到的是新对象，未知字段为空 |
| L3 | `MissionListComp` 改为整份保存，`Unmarshal` 不再只拷两个 map；QuestAllData / QuestScopeData 冻结 |
| L4 | 布局层冻结；`ItemEntry.equip` 标为投影 |
| L5 | `SanitizeSkillList` 替换组件时，保留原组件的顶层未知字段（删技能问题另见 §12） |
| L6 | 回档快照改走统一入口 |
| L7（新增） | 逻辑重置统一改为"`Clear()` 后重设身份字段"。现有的 `ResetAssetOpStreamForEpoch`（`asset_op_ledger.cpp`）是逐字段清空，未知字段会跨纪元存活下来，把旧纪元的状态带进新纪元（评审 M-F，资产路径） |

**统一入口**：新增手写模块 `services/scene/player/system/player_persist_marshal.{h,cpp}`。

```cpp
// 存盘、退出收敛比较、跨 zone 迁移、回档快照的唯一拼装入口:比的、写的、快照的是同一个形状。
// 步骤:PlayerAllDataMessageFieldsMarshal → 填两表 player_id → 两表并回顶层未知字段(两个 Carry 组件)
//      → 两表盖戳 max(kPersistSchemaGen, 加载世代)。
// 返回 false = 持久组件缺失(含两个 Carry 组件);调用方必须拒绝这次写入。
[[nodiscard]] bool MarshalPlayerForPersist(entt::entity player, Guid playerId, PlayerAllData& out);
```

- `MarshalPlayerForSave` 改为转调它；退出收敛和两处 `discardedProbe` 因此一并覆盖。
- `SnapshotSystem::CaptureAndSend` 改为调它。
- `player_data_loader.h` 是生成产物，**不改**。
- 盖戳在快路径比较之前完成；`last_save_seq` 照旧在快路径之后写。

### 4.3 往返一致性测试

**宿主**：放在 bag_test 工程里。它已经调用 `PlayerAllDataMessageFieldsUnmarshal`，SetUp 里也已经加载了 Mission、Condition、Class、AttributePool、AttributeDimension、AttributeRule 等配置表。还要补加载 SkillTable 和宠物相关表，否则 `SanitizeSkillList` 会把技能节点全部删掉（评审核实）。

**夹具**：现有夹具只覆盖背包和任务，需要新建一份**覆盖全部持久组件**的夹具，并使用真实的表 ID。

**步骤**：

1. 用反射遍历夹具里所有已设置的消息节点。在每一个**非冻结、非 map entry、不经由投影字段到达**的节点上，注入一个未知 varint 字段：字段号不超过 2^29−1，并避开 19000–19999；值是该节点的唯一标签。冻结节点本身不注入，但继续向下遍历。比如 `ItemEntry` 冻结，但它的 `instance`（ItemComp）照常注入。
2. 依次执行 `PlayerAllDataMessageFieldsUnmarshal` → `MarshalPlayerForPersist`。
3. 断言：
   - 全部标签都出现在输出里，并且所在节点的类型与注入时相同（按标签的多重集比对，不依赖元素顺序）；
   - 两张表的戳等于期望值。
4. **覆盖率断言**：从两张表可达的每一个非冻结类型，至少要有一个节点被注入。缺失时测试失败，并给出缺失的类型名。例如 `EquipInstanceData` 只能经由 `ItemComp.equip` 满足。

**测试改动面**（评审 M-E）：手工搭建实体的测试都要补齐两个 Carry 组件，或者改用统一入口，否则会进入拒存分支。已知涉及：`cross_zone_test`（`MarshalAsSaved` 不盖戳，和盖了戳的 current 比较必然不等）、`save_seq_test`、`db_task_dispatch_test`、`player_feature_persistence_test`。

### 4.4 语义耦合的守卫（评审 M-F）

对象内携带只保证"字节不丢"，不保证"旧代码对这个对象的操作，在新语义下依然正确"。仓库里有三类操作已经注定会出问题，需要现在就在当前版本里加守卫。这样等以后它们成为"旧版本"时，守卫才会生效：

| 操作 | 现状 [C] | 守卫 |
|---|---|---|
| 合堆、等价判定 | `ItemStore::CanStack` 只判断 `has_equip`；`ApplyStackFill` / `MergePartialStacks` 按配置合堆。`ItemEntry` 的 TODO 里已规划 bound_state、expire_at | 如果任一方 `ItemComp` 带有未知字段，`CanStack` 返回 false。旧版不会把带新属性（比如绑定、限时）的物品并进普通堆 |
| 合并 | `AttachDebt` 会把新欠款并进已有欠款 | 已有欠款带未知字段时，`AttachDebt` 按 fail-closed 返回失败（GM 操作返回错误），不做合并 |
| 逻辑重置 | `ResetAssetOpStreamForEpoch` 逐字段清空 | 改为 `Clear()` 后重设（§4.2 L7） |

跨实体引用（比如新列按 pet_id 引用宠物，而旧代码能按同一个 pet_id 恢复宠物）没有通用守卫。归入 R5：只能引用永不复用的实例 ID，否则按不可携带处理。

### 4.5 成本

平时（全网同一版本）没有任何未知字段，Carry 组件里只是两个空集合，欠款的 `unknownFields` 为空串，不做反射遍历。混版本期间的开销，与未知字段的字节数成正比。

---

## 5. 机制二：覆盖世代校验

### 5.1 校验点

| 点 | 位置 | 条件 | 动作 | 观测 |
|---|---|---|---|---|
| C3 | C++ 严格提交屏障 Lua | 这份数据的**第一次写入**（§5.2）、且不是同版本重放时，加载世代 < hash 里的 `schema_gen`，或者存储值已损坏 | warn：照写，返回 2；enforce：拒写，返回 3 | C++ 日志行 `metric=persist_schema_regression where=redis`，用 LogQL 告警（C++ 节点没有 Prometheus 出口） |
| C4 | login `beginParentLoad` 快路径 | 父快照的加载世代 < hash 里的 `schema_gen` | warn：照常 EnterGame，计数；enforce：`beginParentLoad` 返回错误。走现有失败路径，经 `notifyEnterGameFailed` 推送固定的 `kEnterSceneFailed`，**不新增 tip**。按现行契约，失败原因不外发给客户端（评审核实） | `login_persist_schema_regression_total` |
| C0 / C1 / C1b / C2 | go/db | 见 §6.2 | 延后 / 死信 / 读失败 | `db_task_deferred_total{reason}`、死信原因 |

### 5.2 Redis 严格提交屏障

**C3 只比较"一份数据的第一次写入"**（修复评审 S-A）。

v0.2 要求每次存盘都满足"加载世代 ≥ hash"，而写入成功后 hash 会被抬到输出世代。于是新版 scene 接手旧戳数据后，从第二次存盘起每次都会被判为倒退；enforce 模式下就是全服丢进度。

修正：
- 实体只在**它发起的第一次存盘**里，把真实的加载世代作为 `ARGV[9]` 传给脚本；
- 一旦发起过存盘，此后的 `ARGV[9]` 一律等于本次的输出世代 `ARGV[8]`。因为之后它写入的数据，基底就是它自己的上一次写入。

这样只有"接手别人写的数据"的那一刻才会被比较，这也正是截断可能被引入的地方。

重放的情况：提交其实已经成功，只是回包丢了，`OnReconnected` 重发同一个元素。这时会走现有的同版本重放分支（`pending == ARGV[3]`，且父快照字节相同），**跳过 C3**，返回 1。

| 项 | 规则 |
|---|---|
| 参数 | 由 7 个增加到 10 个：`ARGV[8]` 输出世代，`ARGV[9]` 加载世代（规则见上），`ARGV[10]` 模式，取 `warn` 或 `enforce`。旧二进制自带旧版脚本源码，只发 7 个参数，不受影响（`ScriptSlot` 按源码计算 SHA） |
| 比较 | 新增函数 `num_gt`：先比长度，再按字节比较。现有的 `gt` 只适用于等长字符串 |
| 存储值异常 | hash 里的 `schema_gen` 格式损坏时，视为倒退证据，按模式返回 2 或 3，**不返回 error_reply**。返回 error_reply 会让 `OnSaved` 无限重试，这名玩家就永远存不了盘（评审 M-B） |
| 参数异常 | 参数不合法时，仍然返回 error_reply。这是 C++ 的编程错误，会被单测拦住 |
| 写入 | 只要写入成功（无论 warn 还是 enforce），在同一个 `HSET` 里写 `schema_gen = max(存储值, ARGV[8])`。**只升不降**，否则 warn 模式下低世代写方会把 hash 拉低，截断就此被洗白（评审 M-A） |
| 返回值处理 | `OnSaved`（`redis_client.h`）对屏障元素：返回 1 或 2 都算提交证据，触发 `publishDbTasks` 与 `save_callback_`；返回 2 另打计数日志。返回 3 不重试、不触发提交钩子，同一个 key 的排队值一并丢弃，走**新增的回调**。这个回调不能复用 `HandlePlayerSaveRejected`：它会按 epoch 重存而无限循环，或者按"废黜"处理，语义都不对。新回调在 `core/system/redis.cpp` 接线：退出中走 `DestroyDeposedPlayer`；交接中走 `AbortTravelHandoff`；其余情况保留实体、打日志、本次不落盘 |
| 模式配置 | `BaseDeployConfig` 新增 `persist_schema_regression_mode`，默认 `warn`。改动面：`config.cpp` 读取、`bin/etc/base_deploy_config.yaml`、`k8s_deploy.ps1` 的 ConfigMap 及其契约测试、`start_game.ps1`。login 用 `login.yaml` 里的同名项 |

**enforce 模式的代价**：被判倒退的玩家，本次在线进度不会落盘。按 §5.3 的推导，合法流程不会触发；触发了就说明数据已经被截断。此时宁可不写，也不能让截断的数据去覆盖 SQL。

### 5.3 为什么合法流程不会触发 C3 / C4 [C/I]

- **第一次写入的加载世代来自父快照**：
  - 父快照最近一次是 scene 写的：加载世代 = 那次的输出世代 = hash 值。
  - 最近一次是 login 冷加载写的：它要求两表 SQL ACK 和行围栏都等于 pending（`parent_commit_barrier.go`），所以 SQL 行里已经包含最近一次 scene 写入，行戳 ≥ hash。
  - 最近一次是 login backfill 写的：Go 原样保留，戳不变。
- **后续写入的加载世代等于输出世代**，而 hash ≤ 输出世代（只升不降，且都由这个实体写入）。
- **新号种子**：hash 还不存在，不比较。
- **引导期**：0 按 1 处理，存量数据都是 1。

**唯一的合法例外是回档**：回档可能把数据恢复到更低的戳。目前 `RollbackFence` 还没有赋值、一律返回 NotImplemented；将来实现回档时，必须显式重置 `schema_gen`。

**引导期掩盖问题的教训**（评审 S-A）：全网世代都是 1 时，任何"只在升世代后才出现"的缺陷都不会暴露。所以 §8.1 规定：切到 enforce 之前，必须在 staging 上做一次**真实升世代**的演练。

### 5.4 各写入方

| 写入方 | 要求 |
|---|---|
| login 冷加载组装 | 不改逻辑。C4 只在快路径检查 |
| login `backfillPlayerIdentity` | 不改逻辑。加测试，验证未知字段和戳都保留 |
| login 新号种子 | **不盖戳**，保持 0。原因：`isUnownedLoginSeed` 用 `proto.Equal` 把父快照和骨架比较，未知字段也参与比较；login 混版本运行时，各 pod 盖的戳不同，会导致新号首次入场失败（评审 M3） |
| login `beginParentLoad` 脚本 | 在返回的三元组后追加第 4 个元素 `HGET schema_gen`，供 C4 使用；`inspect` 与 publish 共用这个脚本，追加元素无害（评审核实） |
| go/db 读回执与子表缓存 | 经过 C2 后原样包含行戳 |
| go/db 写后发布缓存 / ACK / 资产证据 | 不改。机制三不做部分写入，body 就是整行 |
| go/db 补投 | 无。投递的字节里已经带戳 |
| `go/db/cmd/save_barrier_restore` | 接受 `schema_gen` 字段：把 `HLEN == 3` 改为字段白名单。重建 hash 时写 `schema_gen = max(1, 两行戳的最大值)` |
| `go/db/cmd/retry_requeue` | 重放的任务照常经过 §6.2 的判定 |
| `tools/merge_zone` | 无。戳随列复制；hash 被删除后视为"不比较" |
| `go/data_service/cmd/debug_import`（只在本地用） | 增加前置检查：行戳 > 工具世代时拒绝导入 |
| data_service 回档 | 将来实现时：原样保存字节，显式重置 `schema_gen` |
| `go/db/cmd/data_stress` | 盖本二进制的世代 |
| `go/db/cmd/verifier` | 把戳当作普通列比较 |
| C++ owner_epoch 为 0 的兼容路径（`Save`，不走屏障） | 不经过 C3，也不写 hash（评审 L-D）。这是现有的兼容窗口，保护等级维持现状，列为残余风险 |

评审核实过的只读方：`java/`、`go/scene_manager`、friend、match、data_service 内部逻辑。旧的 Kafka 跨 zone 搬数据链已经下线。C++ 只有 `InitPlayerFromAllData` 一个加载入口。

每一批开工前，按 AGENTS §11.4 重新 grep 清点一次。

---

## 6. 机制三：go/db 不写写方不认识的列

### 6.1 为什么不是"只写改动列"，也不是"按列掩码写"

1. **兼容要防的是"写方不认识的列被写成零值"**，这件事由写方世代唯一确定。"只写改动列"解决的是另一个问题：减少写入量。
2. **"只写改动列"会破坏现有的收敛保证。** 现在的设计（`db-task-delivery-and-retention.md` §A）是每次整份重写、从不重放旧载荷。改成只写改动列后，只要有一次写入被围栏拒绝或进了死信，它改过的那几列就永远缺失，而且不报错。另外，它需要 C++ 维护逐列的脏标记，这正是 `proto_dirty_compare.h` 文首记录的那类缺陷。
3. **v0.1 的"按列掩码写"，只在倒退写时才会改变结果。** 而倒退写本身就说明链路上有截断：掩码保住了新列，但旧列 BLOB 里的嵌套新字段仍会被覆盖，行戳还会声称数据完整。所以对倒退写直接拒写，更正确，也更简单。

"按改动列写"如果另有性能上的需要，作为独立议题提出，并附压测数据（AGENTS §11.3）。

### 6.2 规则

记号：body 戳为 `s`，本实例世代为 `G`，加锁读到的行戳为 `r`。比较时 0 都按 1 处理。

| 编号 | 位置 | 条件 | 动作 |
|---|---|---|---|
| C0 | 写任务，进 SQL 之前 | 本 zone 已关闭引导期，且 `s` 的原值是 0 | **确定性拒写，进死信**，原因 `persist_unstamped`。不做无限延后（评审 M-G）：机制前的 body 在引导期关闭后永远不会成功，延后只会让重试队列永远清不空。死信保留，以后仍可人工重放 |
| C1 | 写任务，进 SQL 之前 | `s > G` | 延后，不耗预算，原因 `schema_too_new`。按世代号判断，因为 proto3 不序列化零值，新列被清零时 body 里根本没有这个字段。升级完成后自然消化 |
| C1b | 行内围栏事务内，围栏判定之后、`tx.Save` 之前 | `s < r` | 违反 P1，**不写**，直接进死信，原因 `schema_regression`，并告警 |
| C2 | 读任务，SELECT 之后 | `r > G` | 立即回失败结果 `Success=false, Error="schema_too_new"`，不回填缓存 |
| 其余 | | | 与现在相同：整行 `tx.Save`。行不存在时走事务外 `Insert`，本版本认识的列按消息值写入（proto3 零值），等价于"从没写过" |

**接线方式**（评审核实了现成路径）：

- **C1b**：
  - `writeOutcome.kind` 增加 `schema_regression`，走现有的 `quarantineOrderingConflict` 路径。这条路径不重试、直接进死信，还会调用 `markSaveOutboxTerminal`。
  - legacy 分支和 dual 的 `default` 分支，都在 `tx.Save` 之前各判断一次。
  - `runDBOp` 的 kind 白名单和提前返回要加上它，否则它会被当成 unknown outcome，进入耗预算的重试。
  - `handleTask` 的 switch 增加对应 case，指标增加对应的死信标签。
  - 围栏结局 older_seq / stale_owner / equal 的优先级保持不变。
- **C2**：在 `handleTask` 里对哨兵错误单开分支，调用 `failRead`（`placement_route.go`）。不能落到 `deferFailedTask`，否则会照常耗费读预算重试（评审 M7）。
- **C0 标记**：参照 `db:save_protocol` 标记，用 `db:persist_schema:bootstrap_closed:zone:{z}`，读取位置在 `db.go` 的 `AnnounceStrictGeneration` / `WatchLegacy` 旁边：
  - 启动时读一次，读取失败就拒绝启动；
  - 之后按 `WatchLegacy` 的 5 秒节拍刷新，结果存入 `atomic.Bool`，一旦置真就不再回退；
  - 不在每条任务上都去 GET。
- **新的延后原因**：`deferReason`（`store_failure.go`）和 `retryBudgetExempt`（`placement_route.go`）各加一个哨兵；`deterministicWriteRejectReason` 不收这个哨兵。
- **日志限速**：C1 延后时每条任务都会打一行 ERROR，纳入现有的限速汇总（评审 L-C）。

**延后的代价**：延后是针对单条任务的，不会阻塞整个分区。但在 go/db 回滚、或者违反发布顺序期间，同一个 key 的每次存盘都会被延后，重试列表会持续增长 [I]。告警阈值设为持续 5 分钟不为 0，处置方式是升回去，或者把滚动发布走完。

**单表死信**：C1b 或 C0 只让其中一张表的任务进了死信时，两表的 ACK 永远不会相等，这名玩家的冷加载和交接会一直卡住（评审 L-G）。运维手册要写明处置步骤：先确认 Redis 父快照完好，再按 `retry_requeue` 的流程重放，或者人工修复。

### 6.3 DDL 顺序

沿用现有门禁：新版 go/db 启动时，如果库里缺 proto 声明的列，就拒绝启动（`assertSchemaUpToDate`），所以 DDL 一定先于新版 go/db。

---

## 7. 混版本矩阵

假设世代 6 新增了顶层列 `X` 和子消息字段 `y`。

| 场景 | 结果 |
|---|---|
| 新版 scene(6) → 新版 go/db(6) | 整行写，行戳为 6 |
| 新版 scene(6) **接手戳为 5 的数据，并连续存盘多次** | 第一次：加载世代 5，hash 为 5 或不存在，通过，hash 升到 6。之后每次：`ARGV[9]` 等于 6，等于 hash，通过（这一行是修复 S-A 的回归用例） |
| 第一次存盘已提交，但回包丢失后重发 | 走同版本重放分支，跳过 C3，返回 1 |
| 旧版 scene(5) 接手戳为 6 的数据 → 新版 go/db(6) | 对象内携带 `X` 和 `y`；输出世代 6，`s = G`，整行写，数据不丢 |
| 旧版 scene(5) 接手戳为 5 的数据 → 新版 go/db(6) | `s = 5`，`r ≤ 5`，整行写；`X` 写为零值，因为它从来没被写过 |
| 新版 scene(6) → 旧版 go/db(5) | C1 延后并告警，等新版 go/db 处理 |
| 冷加载：行戳 6 → 旧版 go/db(5) 读 | C2 立即失败，玩家稍后重试 |
| 冷加载：行戳 6 → 新版 go/db 读 → 旧版 login 组装 | Go 原样保留，父快照戳为 6 |
| 引导期关闭后，误部署了机制前的 scene | 它写出的父快照戳为 0，内容已被截断；它的 DBTask 被 C0 送进死信，SQL 不受影响。下次加载时 C4 或 C3 判定倒退（加载世代 1 < hash 6），enforce 模式下拒绝。按运维手册处理：清掉父快照，走冷加载，从 SQL 恢复。它在线期间的进度会丢，这是误部署的代价 |
| 全部 scene 从 6 回滚到 5 | 对象内携带，数据不丢。**前提**是世代 6 没有扩展持久值域（§8.3） |

---

## 8. 发布与回滚顺序

### 8.1 引导期：上线机制本身

1. **生成器就位**：台账写入世代 1 的基线，冻结清单和投影清单通过生成器检查，往返一致性测试通过。
2. **引导期内禁止升世代**：生成器看到台账上的"引导中"标记，就拒绝追加世代。
3. **发布顺序**：
   - DDL：两张表各加戳列，默认值 0；
   - go/db：全部 zone，此时不开 C0，`G = 1`；
   - login；
   - scene：开启携带、盖戳，C3 / C4 用 warn 模式。
4. **判定全网已无机制前的写方**：只看 go/db 侧原值为 0 的写 body 计数。这个计数归零，并持续一个完整的存盘周期后，由负责人逐个 zone 写入引导关闭标记（开启 C0），再去掉台账上的"引导中"标记。
5. **切到 enforce 之前**，必须先在 staging 上做一次**真实升世代**的演练：加一个测试用的持久字段，让 gen 升到 2；然后用新旧两套二进制按 §7 的矩阵混跑，重点验证"新版接手旧戳数据并连续存盘"和"回包丢失后重放"两项。演练中 C3 / C4 的倒退计数为 0，并且线上 warn 模式观察期内计数也为 0，才能切到 enforce。

### 8.2 此后每次升世代

`DDL（如果有新列）→ go/db 全部 zone → login → scene（可以按 zone 金丝雀）`

顺序如果被违反，C1 / C2 只会延后或失败，不会丢数据。

可选项（第 4 批）：在 go/db 的能力心跳里带上世代号，`k8s_deploy.ps1` 发布 scene 之前做只读预检。

### 8.3 回滚规则

| 对象 | 能否回滚 | 说明 |
|---|---|---|
| login | 可以回滚到任何一个带本机制的版本 | Go 原样保留 |
| scene | **有条件** | 对象内携带保证未知字段不丢。但**持久值域扩展**不在台账里：比如新增包类型后回滚，旧版 `bag_marshal::Unmarshal` 会丢掉越界物品；新增技能后回滚，`SanitizeSkillList` 会删掉表里查不到的技能。§12 的这两处修好之前，发布单必须写明本版本"没有扩展持久值域"，否则不能回滚 scene |
| go/db | 只能回滚到世代 ≥ 数据最高戳的版本 | 回滚得更低会卡住存盘和冷加载，但不丢数据，处置方式是升回去 |
| 回到机制上线前的二进制 | 禁止 | 引导期关闭后，C0 与 C3 / C4 会兜底 |

### 8.4 告警与运维手册

| 告警 | 来源 | 处置 |
|---|---|---|
| `DbTaskSchemaTooNew` | go/db 的 Prometheus 指标 | 检查 go/db 版本，升回去，或者把滚动发布走完 |
| `DbTaskPersistUnstamped` | go/db 死信原因 | 找到机制上线前的写方，下线它 |
| `PersistSchemaRegression` | go/db 死信原因、login 指标、C++ 日志（LogQL） | 按 `where` 定位出问题的那一跳，暂停该跳的发布；清掉父快照，走冷加载恢复。单表死信按 §6.2 处理 |

- 运维手册写进 `deploy/k8s/README.md`；
- Prometheus 告警规则写在 `deploy/k8s/persist-schema-alerts.yaml`；
- C++ 侧的告警按 `docs/ops/grafana-loki-local-logs.md` 的 LogQL 方式配置。

---

## 9. 分批实施（每批 < 30 个文件，AGENTS §10.2）

| 批 | 内容 | 主要改动 | Codex 验证 |
|---|---|---|---|
| 1 | 台账；生成器（发号、冻结和投影检查、R2 / R3、"引导中"标记）；两个 option 及其标注；两张表的戳字段；生成常量与清单 | `proto/db/proto_option.proto`、各持久 proto 的标注、`proto/persist_schema_gen.txt`、`tools/proto_generator/protogen/internal/...`（新增生成器及测试）、生成产物 | 先 `proto-gen-build`，再 `proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`；protogen 跑 `go test ./...`；最后完整编译 C++ 与全部 Go module。生成的 .pb 文件如果超过 30 个，先按 §10.2 报告 |
| 2 | go/db：C0 / C1 / C1b / C2 及其接线，新的延后原因和死信原因，指标、告警、引导期标记，日志限速；cmd/migrate 的 DDL；save_barrier_restore、debug_import、data_stress 的配套改动 | `go/db/internal/kafka/{player_row_fence,key_ordered_consumer,store_failure,placement_route}.go`、`go/db/db.go`、`go/db/internal/metrics`、`go/db/internal/barrierrestore/scripts.go`、`go/data_service/cmd/debug_import`、`deploy/k8s/persist-schema-alerts.yaml`、测试 | go/db 与 data_service 跑 `go test ./...`、`go vet`；真库集成测试（自起一次性 MySQL / TiDB） |
| 3a | C++ 携带：两个 Carry 组件（在手写 loader 里创建）、统一入口、欠款与任务列表的携带、`SanitizeSkillList` 保留顶层未知字段、`ResetAssetOpStreamForEpoch` 改为 `Clear()`、§4.4 的两个守卫、回档快照改入口、盖戳、往返一致性测试与全组件夹具、受影响测试的修正 | `services/scene/player/{comp,system}/...`、`modules/currency/...`、`mission_marshal.cpp`、`player_skill.cpp`、`asset_op_ledger.cpp`、`modules/bag/item_store.cpp`、`snapshot_system.cpp`、bag_test 及 §4.3 列出的测试、vcxproj / CMakeLists。文件可能接近 30 个，必要时把守卫拆成 3a′ | `game.sln` 串行 `/m:1` 完整编译；新测试和所有存盘相关单测 |
| 3b | C++ 校验：严格屏障 Lua 增加到 10 个参数、`num_gt`、返回值 2 / 3、`OnSaved` 的处理与新回调（`core/system/redis.cpp`）、首次写入加载世代的传递、`BaseDeployConfig` 及部署渲染 | `redis_client.h`、`core/system/redis.cpp`、`player_lifecycle.cpp`、`currency_test.cpp`（现有用例断言"返回 2 是坏回包"、字面量白名单、以 7 个参数直接 EVAL 的真 Redis 用例、调用文本契约，这几处都要改写）、`base_deploy_config` 的 proto / `config.cpp` / yaml、`k8s_deploy.ps1` 及其契约测试、`start_game.ps1` | `game.sln` 完整编译；Redis 脚本测试；k8s_deploy 契约测试 |
| 4 | login：C4（`beginParentLoad` 增加第 4 个返回元素并比较）、种子保持不盖戳的测试、backfill 往返测试、`login.yaml`；可选的能力心跳与部署预检；运维手册 | `go/login/internal/logic/pkg/dataloader/...`、`go/login/etc/login.yaml`、`deploy/k8s/README.md` | login 跑 `go test ./...`、`go vet` |
| 5 | §12 中持久值域的两个删数据点（这是 scene 无条件回滚的前提） | `player_skill.cpp`、`bag_marshal.cpp` 及测试 | 同 3a |

每批完成后在 `docs/PROGRESS.md` 追加一条，并注明 Java 版的状态（§11）。

---

## 10. 测试计划

**C++（3a / 3b / 5）**：
- 往返一致性测试，包括覆盖率断言。
- 欠款：删除后用同一个 `currency_type` 重建，不应带上旧的未知字段；不删除时，未知字段应保留。
- 任务：放弃后重新接取，不应带上旧的未知字段。
- 合堆守卫：带未知字段的物品不参与合堆。
- `AttachDebt` 守卫：已有欠款带未知字段时，返回失败。
- `ResetAssetOpStreamForEpoch` 之后，未知字段为空。
- 两张表的顶层未知字段往返后保留；加载世代取两表的 min，输出世代取 max；同一份数据连续存两次，第二次命中快路径。
- 严格屏障 Lua：
  - `num_gt` 的边界：9 对 10、10 对 9、1 对 19、相等；
  - hash 不存在 / 没有 `schema_gen` / 值损坏；
  - 首次写入时倒退：warn 返回 2，enforce 返回 3；
  - **同一实体连续两次存盘**都通过（S-A 回归）；
  - **回包丢失后重发**走重放分支，返回 1；
  - warn 模式下 hash 只升不降；
  - 参数个数不对时报错。
- `OnSaved` 与新回调在退出中、交接中、在线三种状态下的分派。

**Go（2 / 4）**：
- C0：引导期关闭后，`s = 0` 进死信。
- C1：`s > G` 时延后且不耗预算；新列被清零（body 里没有该字段）时同样延后。
- C1b：`s < r` 时进死信，SQL 不变；围栏判定的优先级不变；runDBOp 白名单包含新的 kind。
- C2：立即失败，不回填缓存，不消耗读预算。
- 用 `dynamicpb` 模拟高世代的 body。
- login C4 的 warn 与 enforce 两种模式；种子不盖戳时 `isUnownedLoginSeed` 判断不变；backfill 往返保留戳和未知字段。
- save_barrier_restore 接受 `schema_gen`，写入值 ≥ 1；debug_import 的前置检查生效。

**生成器（1）**：
- 新增字段 → 追加世代；
- 删除并写了 reserved → 标为 retired；
- 以下情况生成失败：改类型、复用号、给 oneof 加成员、给冻结类型加字段、投影字段的类型没有其他载体可达、"引导中"期间追加世代；
- 输出稳定、可重复。

**真实环境（第 2、3 批之后，由 Codex 或负责人执行）**：按 §8.1 第 5 步做真实升世代演练，按 §7 矩阵逐行混跑验证。这一步不能用单测代替；跑完之前，不能宣称"混版本安全"。

---

## 11. Java 版对齐（AGENTS §12）

- **形态** [C]：玩家状态存在 `player_state.data` 这一个 BLOB 里，内容是 `PlayerState` 的 PB，用 `toByteArray` / `parseFrom` 读写；`player` 表的其他列由固定 SQL 写入。
- **适用性**：
  - 机制三不适用。
  - 机制一（对象内携带 + 往返一致性测试 + 语义耦合守卫）和机制二（世代戳 + 加载 / 写入校验）都需要做。
- **状态**：**待做**。Java 要按自己的存储形态来实现（§12 规定服务端内部按 Java 惯用方式）。第 1 批实施时，在 Java 仓的 `PARITY.md` 登记一行。

---

## 12. 不在本次范围、但属于同类的丢数据点

| 问题 | 现状 [C] | 建议 |
|---|---|---|
| 持久值域：旧配置表删掉新技能 | `SanitizeSkillList` 删除表里查不到的技能 | 第 5 批：保留，但不生效 |
| 持久值域：越界的包类型 | `bag_marshal::Unmarshal` 丢弃越界条目；`capacities` 只输出 `kBagTypeCount` 个 | 第 5 批：越界条目保留，原样存回 |
| 收件箱、资产通道载荷的新字段 | 旧版执行器会忽略新字段，记为 APPLIED | 给载荷加版本号，或执行器遇到未知字段时 fail-closed；开启收件箱消费之前必须修 |
| Kafka 控制面命令 | 遇到未知事件直接丢弃 | 新增事件类型时配发件箱或重投机制 |
| guild、mail、trade 的库表 | 未核对 | 按本文的不变量逐个评估 |
| 客户端契约 | 见 `published-contract-compatibility-20261017.md` | 已有独立方案 |
| 待确认 | scene 不 marshal `merge_state(10)` 和 `stress_test_probe(9)`，每次整行写都会把它们清空 | 确认是否是有意设计（例如"首次存盘清除合服标记"）。v0.3 先把它们冻结 |

---

## 13. 风险与待负责人拍板

1. **"只写改动列"改为"超前延后、倒退拒写、未盖戳拒写、其余整行写"**（§6.1），请确认。
2. **option 的读取方式**：推荐生成器动态读取，不升级外部模块。
3. **C3 / C4 先 warn 后 enforce**。切换的前提是 §8.1 第 5 步的真实升世代演练通过。enforce 的代价是被判截断的玩家本次在线进度不落盘。
4. **引导期禁止升世代**，时长取决于全网换版的速度。
5. **冻结清单、R2 / R4 / R5，以及 §4.4 的守卫**会改变今后的开发习惯：
   - 背包布局层、任务容器层不能再直接加字段；
   - 带新属性的物品，在旧版里不会被合堆；
   - 带新字段的欠款，在旧版里不允许再挂新欠款。
   这些需要负责人认可。
6. **残余风险**：
   - 超出守卫范围的语义耦合；
   - 回档功能实现时必须重置 `schema_gen`；
   - 误部署机制前的 scene 期间，在线进度会丢；
   - warn 期间截断只告警、不拦截；
   - owner_epoch 为 0 的兼容路径没有 C3 保护。
7. **评审清单**（凡是修改持久 proto 的 PR 都要过）：
   - 生成器是否追加了世代；
   - 是否在主干上发号；
   - 冻结类型有没有被加字段；
   - 有没有字段迁移（R4）；
   - 有没有语义耦合字段（R5）：等价判定、合堆、合并、逻辑重置、跨实体引用；
   - 有没有扩展持久值域（发布单要写明"scene 不可回滚到此前版本"）；
   - 新字段被旧写方原样携带时，语义是否仍然成立。

---

## 14. 证据索引

- **C++**：
  - `cpp/libs/services/scene/player/system/`：`player_lifecycle.cpp`（`MarshalPlayerForSave`、`SavePlayerToRedisImpl`、`OnDbTaskDispatchSettled`、`InitPlayerFromAllData`、`HandlePlayerSaveRejected`）、`player_data_loader.h`（生成产物，模板为 `player_data_loader.h.tmpl`）、`player_database_loader.cpp`、`player_database_1_loader.cpp`、`mission_marshal.cpp`、`bag_marshal.{h,cpp}`、`player_skill.cpp`、`player_attribute.cpp`、`player_pet.cpp`；
  - `cpp/libs/modules/currency/{comp/player_currency_comp.h,system/currency_system.cpp}`、`cpp/libs/modules/mission/system/mission.cpp`、`cpp/libs/modules/bag/item_store.cpp`；
  - 资产通道账本 `asset_op_ledger.cpp`；
  - `cpp/libs/modules/snapshot/snapshot_system.cpp`（其中的 `kSnapshotSchemaVersion` 是快照条目信封的格式版本，与本文的世代无关）；
  - `cpp/libs/engine/infra/storage/redis_client/redis_client.h`（`kSaveWithCommitBarrierLuaScript`、`ScriptSlot`、`OnSaved`、`OnReconnected`）、`cpp/libs/services/scene/core/system/redis.cpp`；
  - 测试：`cpp/tests/` 下的 `bag_test`（`player_feature_persistence_test.cpp`）、`currency_test`、`cross_zone_test`、`save_seq_test`、`db_task_dispatch_test`。
- **go/db**：
  - `go/db/internal/kafka/`：`key_ordered_consumer.go`（`handleDBReadOp`、`publishDBReadResult`、`handleDBWriteOp`、`runDBOp`、`handleTask`、`quarantineOrderingConflict`）、`player_row_fence.go`、`player_cache_publish.go`、`store_failure.go`、`placement_route.go`（`failRead`、`retryBudgetExempt`）、`save_outbox_note.go`；
  - `go/db/db.go`、`go/db/internal/logic/pkg/proto_sql/db.go`（`assertSchemaUpToDate`）、`go/db/internal/saveoutbox/{generation,scripts}.go`、`go/db/internal/barrierrestore/scripts.go`。
- **proto2mysql**：v0.2.0 的 `proto2mysql.go`、`pbconv/convert.go`。
- **login**：
  - `go/login/internal/logic/pkg/dataloader/`：`ensure_player_all_data_async.go`、`parent_commit_barrier.go`（`beginParentLoad`、`isUnownedLoginSeed`）、`new_player_seed.go`；
  - `go/login/internal/logic/clientplayerlogin/`：`player_class_backfill.go`、`entergamelogic.go`（`notifyEnterGameFailed`）。
- **其他**：`tools/merge_zone/player_rows.go`、`go/data_service/cmd/debug_import/bundle_import.go`。
- **proto**：`proto/common/database/{player_cache,mysql_database_table,bag_quest_mail_data}.proto`、`proto/db/proto_option.proto`、`proto/message_id.txt`。
- **Java**：`D:\luyuan\xuanming-server-mmo\game-player-store\src\main\java\com\game\player\store\{PlayerStore,PlayerMapper,PlayerStateRow}.java`。
- **关联文档**：`db-write-fencing-mysql.md`、`db-task-delivery-and-retention.md`、`db-task-store-unavailable-deferral.md`、`ecs-component-access-rules.md`、`bag-instance-layout-split.md`、`data-overwrite-rollback-audit-20261018.md`、`published-contract-compatibility-20261017.md`。

---

## 15. 修订记录

### v0.1

首稿。核心做法：从加载原件嫁接未知字段；Lua 比较输出世代；go/db 按列掩码写。

### v0.2（第一轮评审）

| 评审项 | 修正 |
|---|---|
| S1 嫁接会复活已删除元素 | 改为"未知字段跟着对象走"，加冻结规则和往返一致性测试 |
| S2 截断被洗白 | C3 比较加载世代；新增 login 门禁 C4 |
| S3 零值不序列化 | C1 改为按世代号判断 |
| S4 退役字段 | C1 不再看未知字段 |
| S5 回滚承诺 | 改为有条件 |
| M1–M9 | 返回值、比较函数、种子不盖戳、恢复工具、`s < r` 拒写、R4、C2 立即失败、C0 与主干发号、LogQL |

### v0.3（第二轮评审）

| 评审项 | 修正 |
|---|---|
| S-A C3 必现误报 | 只在首次写入时比较加载世代，重放跳过；新增 §7 回归行与 §8.1 第 5 步演练 |
| M-A | hash 只升不降 |
| M-B | 增加模式参数；存储值异常视为倒退证据，不回 error；所有写入值 ≥ 1 |
| M-C | 返回值 3 的新回调与分派；补全 3b 文件清单（`core/system/redis.cpp`、`currency_test.cpp`） |
| M-D | 补全冻结清单；增加字段级投影 option；排除 map entry |
| M-E | Carry 组件改在手写 loader 里创建；补充测试改动面 |
| M-F | 新增 R5 与 §4.4 守卫；`ResetAssetOpStreamForEpoch` 改为 `Clear()` |
| M-G | C0 改为确定性进死信 |
| L-A | 加载世代取两表最小值 |
| L-B | C1b / C2 接线 |
| L-C | 日志限速 |
| L-D | epoch=0 兼容路径列为残余风险 |
| L-E | C4 不新增 tip |
| L-F | 冻结 Vector3 / Rotation |
| L-G | 单表死信的处置 |
