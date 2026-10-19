# 新旧版本混跑时的玩家持久字段保护

> **状态**：设计稿 v0.2（2026-10-19，Claude）。**未落码、未编译、未测试**（AGENTS.md §10.1）。v0.1 经一轮对抗评审被推翻了三处核心做法，v0.2 按评审结论重写，修订明细见 §15。负责人评审通过后按 §9 分批实施，每批由 Codex 验证。
> **起因**：2026-10-18 核对灰度 / 不停服 / 金丝雀发布时发现，新旧版本混跑会丢玩家持久字段。负责人 2026-10-19 要求先补三件事：①存盘保留不认识的字段；②Redis 父快照记录写方版本并校验；③go/db 只写改动列。要求先出设计，再动代码。
> **范围**：只覆盖玩家数据链，即 C++ scene 内存 → Redis 父快照 `PlayerAllData:{id}` → Kafka `db_task_zone_*` → go/db → MySQL 两张玩家表；外加 login 冷加载、回档快照、离线工具这些旁路。guild、mail、trade 各自的库表，以及客户端契约兼容，都不在本文范围内（§12）。
> **证据口径**：[C] 表示对照 2026-10-19 工作区代码读过；[I] 表示推断。相关文件正被其他会话修改，行号会漂移，所以正文以函数名为主。

---

## 0. 结论速览

1. **根因**：链路上有几个环节会**按本版本认识的字段重新拼一份数据**，再整份写回。新版加的字段因此在旧版手里被抹掉（§1.3 表里 L1–L7），而且全程不报错（L8）。
2. **一条不变量统一三件事**。每张玩家表消息都带一个**覆盖世代** `persist_schema_gen`，含义是"这份数据里，这个世代及以前定义的字段，值都是真实的"。数据每经过一道读写，只有两种合法结局：
   - **完整保留**，输出世代 ≥ 输入；
   - **拒绝或延后**，不产出任何副本。

   守住这一条，新版读到的缺失新字段就一定是"从来没写过"，按默认值处理就是对的。
3. **三个机制**，与负责人提的三件事一一对应：
   - **机制一 · 未知字段跟着对象走**：存盘重拼的几个环节改为把未知字段保存在运行时对象里，对象删了，未知字段也跟着没了。实在做不到携带的"重建层"消息类型一律冻结，生成器不允许它们再加字段。一个通用的**往返一致性测试**用机械方式守住这条规则：给每层消息注入未知字段，加载再存盘，逐个核对。
   - **机制二 · 覆盖世代校验**：
     - Redis 严格提交屏障在原子脚本里校验"本次写方**加载时**的世代不低于已记录世代"；
     - login 加载时做同样的门禁；
     - 违反就判定"链路上有人截断过数据"，fail-closed。
   - **机制三 · go/db 不写写方不认识的列**：比"只写改动列"更简单，也更严。
     - body 的世代**超过**本实例世代：延后，交给新版实例处理；
     - body 的世代**低于** SQL 行的世代：拒写，进死信并告警；
     - 两者都不是：整行写。

     评审证明：按列掩码只在"倒退写"时才会改变结果，而那种情况下掩码仍会覆盖旧列 BLOB 里的嵌套新字段，所以改为直接拒写。为什么不做"只写改动列"，见 §6.1。**这一条需要负责人确认。**
4. **世代号由生成器发**：仿照 `proto/message_id.txt` 增加台账 `proto/persist_schema_gen.txt`，生成器比对两张玩家表可达的字段集合，自动升号；不能安全携带的改动在生成期直接失败（§3.2）。
5. **上线与回滚**：
   - 先上线机制本身，引导期内不允许升世代；
   - 之后每次升世代，按"DDL → go/db 全部 zone → login → scene（可以金丝雀）"的顺序发；
   - **go/db 回滚**到低于数据世代的版本，只会卡住（写延后、读失败、告警），不会丢数据；
   - **scene 回滚**有前提：新版本没有扩展持久值域，比如新增包类型、新的配置 ID。§12 的两处删数据点修好之前，不能宣称 scene 可以无条件回滚（§8.3）。
6. **Java 版**：存储是单个 BLOB，机制三不适用，机制一、二同样需要。本批只登记，原因见 §11。

---

## 1. 问题与现状

### 1.1 数据流

```text
C++ scene（ECS 组件，内存是权威）
  │  MarshalPlayerForSave：从组件重拼 PlayerAllData
  ▼
Redis 父快照  PlayerAllData:{id}（PlayerAllData 原始字节）
  │  严格提交屏障 Lua：写父快照，同时写 save_barrier.pending 与完整投递意图
  ▼
Kafka db_task_zone_*  DBTask{body = player_database / player_database_1 的字节}
  ▼
go/db：Unmarshal(body) → 行内围栏事务（FOR UPDATE 读当前行）→ proto2mysql tx.Save
  ▼
MySQL player_database / player_database_1
  （每个顶层字段一列；子消息字段按二进制 PB 存成 BLOB）

冷加载（父快照缺失时）：
login → go/db 读任务 → SELECT 本版本认识的列 → 读回执 → login 组装父快照，
在两表 SQL ACK 都等于 pending 后才发布
```

### 1.2 已经能保留未知字段的环节 [C]

| 环节 | 为什么能保留 |
|---|---|
| Go 侧中间跳：login 冷加载组装、`backfillPlayerIdentity` 读改写、go/db 写前的 Unmarshal | Go protobuf 默认保留未知字段，`proto.Marshal` 会原样写回 |
| proto2mysql 的子消息列 | `pbconv.SerializeFieldValue` 对 MessageKind、map/list 都用 `proto.Marshal`（`pbconv/convert.go`） |
| C++ 中以 PB 形式直接存放的组件：Transform、PlayerUint64Comp、PlayerSkillListComp、LevelComp、各类账本等 | 加载时 `emplace<Comp>(player, message.xxx())` 拷贝整份子消息，存盘时 `CopyFrom(*comp)` 再带回（`player_database_loader.cpp`） |
| 物品实例 | `ItemEntry.instance = 15` 承载整份 `ItemComp`（`bag_marshal.cpp` 的 `ItemToEntry` / `ItemFromEntry`） |

最后一行有个前提：运行期代码对这些对象做 `Clear`、整体替换或删除后，未知字段会跟着消失。按 protobuf 的语义这是正确的，因为旧代码重置了对象，新字段也应该随之重置。

### 1.3 会丢字段的环节 [C]

| 编号 | 位置 | 丢什么 |
|---|---|---|
| L1 | `SavePlayerToRedisImpl` 用 `std::make_shared` 新建空消息交给 `MarshalPlayerForSave`；退出收敛比较、`discardedProbe` 也都是新建（`player_lifecycle.cpp`） | player_database、player_database_1 两层的**顶层未知字段**，也就是新增的整列。旧版 scene 接手新数据后，EnterScene 之后的首次存盘一定真写，新列就此被抹掉 |
| L2 | `PlayerCurrencyComp::SaveToProto` 先 `clear_debts()` 再逐字段重建（`player_currency_comp.h:50-65`） | `CurrencyDebtEntry` 的新字段。这是**资产路径** |
| L3 | `mission_marshal::Marshal` 先 `out.Clear()` 再重建；`Unmarshal` 只拷 `missions` 和 `mission_begin_time` | QuestAllData、QuestScopeData、MissionListComp 的顶层新字段 |
| L4 | `bag_marshal::Marshal` 先 `Clear()`，再重建 BagAllData、DynamicBagData、ItemEntry 这些布局层 | 布局层的新字段（实例层由 instance 保住） |
| L5 | `PlayerSkillSystem::SanitizeSkillList` 新建一份 PlayerSkillListComp 再整体替换（`player_skill.cpp`） | PlayerSkillListComp 的顶层新字段。它还会删掉表里查不到的技能，见 §12 |
| L6 | `SnapshotSystem::CaptureAndSend` 直接拿新建的空消息拼数据（`snapshot_system.cpp`） | 同 L1，丢在回档快照里 |
| L7 | go/db 写和读都只覆盖本版本认识的列：写是 `tx.Save` 整行 UPDATE（proto2mysql `saveByPrimaryKey`），读是 `FindOneByWhereClause` 只 SELECT 已知列（`fieldsListSQL`） | 两种情况：新版 go/db 收到旧写方的 body 时会把新列写成零值；旧版 go/db 写不了新列，冷加载读回的也是截断副本 |
| L8 | 全链路 | 没有写方版本，也没有覆盖信息，上面任何一种丢失都**零报错** |

### 1.4 后果

- **不能金丝雀**：金丝雀本身就是新旧版本混跑，而混跑会丢字段。
- **不能回滚**：回滚 scene 后，所有在线玩家的新字段都会被旧 scene 的首次存盘抹掉。
- **丢了也不知道**：见 L8。

---

## 2. 目标与不变量

**P1：覆盖世代单调传播（核心不变量）。** 每张玩家表消息带覆盖世代 `c`。数据每经过一跳读写，只能二选一：

- 完整保留全部字段（包括本跳不认识的），输出 `c' = max(本跳世代, c)`；
- 拒绝或延后，不产出副本。

**推论**：只要每一跳都守住 P1，数据里缺失的、世代大于 `c` 才引入的字段，就一定从来没被写过，读方按默认值处理是对的。

**P2：** 旧版 scene 接手带新字段的数据时，必须完整携带这些字段。

**P3：** go/db 不写写方不认识的列；保存不完整的数据就延后或拒写。

**P4：** 破坏 P1 的情况，终态一律 fail-closed，并配有指标和告警，player_id 不进 label。引导期允许 C3 / C4 先以 warn 模式观测（§5.2），这段残余风险在 §13 列明。

**非目标：**
- 不支持"新字段与旧字段语义耦合"的改动。字段作者要保证新字段在旧写方原样携带时含义依然成立；做不到就按不可携带处理（§3.2）。
- 不解决持久值域扩展（§12）。
- 不支持 Redis Cluster 跨槽，沿用现行的"同一 Redis 实例"前提。

---

## 3. 核心概念：持久 schema 世代

### 3.1 世代号与台账

- **世代号**：`uint32`，从 1 开始单调递增。覆盖范围是从两张玩家表消息（`OptionIsPlayerDatabase`）可达的全部字段。`PlayerAllData` 顶层不经 SQL，冷加载时由骨架组装，所以**不作为根**，整体冻结（§3.2）。
- **台账**：`proto/persist_schema_gen.txt`，仿照 `proto/message_id.txt` / `event_id.txt`，由生成器维护、提交进仓库。格式：
  ```text
  1=baseline:<规范化字段集合的 sha256>
  2=player_database.foo_component(22):message:FooComp
  2=CurrencyDebtEntry.interest(9):uint64
  ```
- **生成器的行为**：
  - 只有新增字段：追加 `N+1` 行；
  - 删除字段且写了 `reserved N;`：原行标记 `retired`；
  - 改字段号、改类型、复用字段号：生成失败（AGENTS §4.3）。
- **退役字段**：退役字段的字节可能还留在存量数据里，会被当作未知字段一直携带下去。字段号永不复用，所以这样无害。机制三按世代号判断，不看"有没有未知字段"（§6.2），因此不会被这些残留字节卡住（修正评审 S4）。
- **生成产物**（在输出树里，不手改）：
  - C++ 常量 `kPersistSchemaGen`；
  - Go 包 `go/shared/generated/persistschema` 的常量 `Gen`。

  机制三不再需要"列 → 引入世代"表（§6.1）。
- **只在主干发号**：发布分支和 hotfix 分支不得自行追加世代。需要新增持久字段时，必须先合进主干、拿到号，再整行 cherry-pick 台账。否则会出现两个含义不同的"世代 6"（评审 M8）。这是流程规则，写进 §13 的评审清单。

### 3.2 可携带性规则（生成期强制）

| 规则 | 不可携带的改动 | 原因 | 正确做法 |
|---|---|---|---|
| R1 | 给**冻结类型**加字段 | 冻结类型由手写代码重建，不保留未知字段 | 先为这个类型实现运行时携带，并让往返一致性测试覆盖它，再解冻；或者把新数据放进可携带的类型（例如物品数据放进 `ItemComp`，AGENTS §11.6） |
| R2 | 给已有的持久 oneof 加新成员 | 旧写方设置另一个成员时，会与携带回来的未知成员冲突 | 新字段放在 oneof 外面，或者走破坏性变更流程（整批停机换版，或 expand / contract） |
| R3 | 改字段号、改类型、复用字段号 | 违反 AGENTS §4.3 | 用 `reserved` |
| R4 | 字段迁移：子消息字段提升为列、拆分字段、换存储位置 | 旧写方更新旧位置，携带回来的新位置是旧值，新读方只看新位置，旧写方的修改就丢了（评审 M6） | 走 expand / contract：新旧位置双写一个完整版本周期，再切读方 |

R4 生成器无法可靠识别，只能检测"同一世代内既删又增"并提示，所以列入 §13 的评审清单。

**冻结类型**用 `[(OptionPersistFrozen) = true]` 声明在 message 上。v0.2 的初始冻结清单 [C]：

| 类型 | 冻结原因 |
|---|---|
| `PlayerAllData` | 顶层不经 SQL；`bag_data` / `quest_data` 只是兼容副本 |
| `BagAllData`、`DynamicBagData`、`ItemEntry` | 布局层由 `bag_marshal` 重建；物品数据放 `ItemComp` |
| `QuestAllData`、`QuestScopeData` | 由 `mission_marshal` 重建；`active` 是不再写入的旧格式 |
| `MailAllData`、`MailEntry` | 目前没有写入方 |

其余从两张玩家表可达的类型，都必须通过往返一致性测试（§4.3）。

**option 的落点**：现有 option 定义在 `proto/db/proto_option.proto`，生成器通过外部模块 `github.com/luyuancpp/protooption v0.0.21` 的编译绑定读取。新增 option 有两种做法：
- 升级外部模块，需要负责人打 tag、推送；
- 生成器从描述符原始字节动态读取扩展。

**推荐动态读取。** v0.2 只需要一个 option（冻结），比 v0.1 的身份键 option 少得多。

### 3.3 世代戳放在哪

两张表消息各加一个 `uint32 persist_schema_gen`：`player_database` 用 21 号（按加列纪律取下一个空闲号，放在末尾），`player_database_1` 用 5 号。

- **戳和内容在同一份字节里**，跟着已有的所有载体走：父快照、DBTask body、MySQL 列、子表缓存、跨 zone 迁移、回档快照。不新增第二份真相。
- **Go 中间跳**不认识这个字段时也会原样保留它。
- **它本身就是一列**，go/db 读写行时自动带上。
- **取值约定**：0 表示未盖戳，即机制上线前写入的数据，或 login 的新号种子（§5.4）。**所有比较都先把 0 按 1 处理**。
- **save_barrier hash 里的 `schema_gen`**（§5.2）只是索引，不是权威。hash 不存在、或者 hash 里没有这个字段时，**一律不比较**。会出现这种情况的有：机制前的存量玩家、merge_zone 删过 hash、恢复工具重建过 hash、本地 debug_import。

---

## 4. 机制一：未知字段跟着对象走

### 4.1 为什么放弃 v0.1 的"从加载原件嫁接"

v0.1 的做法是在实体上保存加载原件，存盘时按身份键把原件里的未知字段嫁接回新拼出的消息。评审 S1 指出这个做法会**复活已删除元素的新字段**：

- 欠款还清后删除，之后 `AttachDebt` 用同一个 `currency_type` 新建欠款（`currency_system.cpp`），按 `currency_type` 嫁接，就把旧欠款的新字段挂到了新欠款上。
- 任务放弃后，`AcceptMission` 用同一个任务 ID 新建，问题相同。

`currency_type`、任务 ID、`scope`、`skill_table_id` 都是**会被复用的槽位键**，不是实例 ID。单值子消息被旧代码重置后，嫁接同样会把旧值复活。根本问题在于：嫁接把未知字段的生命周期和对象的生命周期拆开了。

### 4.2 v0.2 的做法

**原则：未知字段只存在于它所属的对象里，随对象创建、修改、删除。** 这正是 protobuf 本身的语义。PB 直存的组件已经天然如此（§1.2），只需要改造重建层：

| 编号 | 改造 |
|---|---|
| L1 | 新增组件 `PlayerPersistBaseComp`（持久数据类，只在 `InitPlayerFromAllData` 创建，存盘入口缺了它就整份拒存）。保存两张表消息**顶层**的未知字段集合，以及加载时的覆盖世代 `loadedCoverageGen`（两表戳的最大值，0 按 1 处理）。顶层的未知字段是旧代码完全没有对应逻辑的整列，旧代码不可能删除或重置它们，所以不存在复活问题 |
| L2 | 运行时结构 `CurrencyDebt` 增加 `std::string unknownFields`（`UnknownFieldSet` 的序列化）。`LoadFromProto` 时保存，`SaveToProto` 时写回。欠款删除后重建得到的是全新对象，未知字段为空，语义正确 |
| L3 | `MissionListComp` 改为整份保存（`Unmarshal` 不再只拷两个 map），它的未知字段随对象携带。`QuestAllData` / `QuestScopeData` 冻结（§3.2） |
| L4 | 布局层 `BagAllData` / `DynamicBagData` / `ItemEntry` 冻结。物品数据已经由 `instance` 整份携带 |
| L5 | `SanitizeSkillList` 替换组件时，保留原组件的顶层未知字段。删除技能的问题另见 §12 |
| L6 | 回档快照改走统一入口 |

**统一入口**：新增手写模块 `services/scene/player/system/player_persist_marshal.{h,cpp}`：

```cpp
// 存盘、退出收敛比较、跨 zone 迁移、回档快照的唯一拼装入口:比的、写的、快照的是同一个形状。
// 步骤:PlayerAllDataMessageFieldsMarshal → 填两表 player_id → 并回两表顶层未知字段(PlayerPersistBaseComp)
//      → 两表盖戳 max(kPersistSchemaGen, loadedCoverageGen)。
// 返回 false = 持久组件缺失(含 PlayerPersistBaseComp);调用方必须拒绝这次写入。
[[nodiscard]] bool MarshalPlayerForPersist(entt::entity player, Guid playerId, PlayerAllData& out);
```

接入方式：
- `MarshalPlayerForSave`（`player_lifecycle.cpp` 匿名命名空间）改为转调它。退出收敛比较和两处 `discardedProbe` 都经过 `MarshalPlayerForSave`，因此一并覆盖。
- `SnapshotSystem::CaptureAndSend` 也改为调它。
- `player_data_loader.h` 是生成产物（模板 `player_data_loader.h.tmpl`），**不改**。

盖戳在快路径比较之前完成，否则同一份数据会因为戳不同被误判为"有变化"。`last_save_seq` 依然在快路径之后写。

### 4.3 往返一致性测试：机械守住"不丢未知字段"

"哪些代码会重建对象"靠人记不住，所以用一个通用测试守住（仿照 guild / friend / match 用 `TestNoHandWrittenTipCodes` 守住 tip 发号规则的做法）。测试步骤：

1. 取一份覆盖全部持久组件的玩家夹具。
2. 用反射遍历夹具里**每一个已设置的非冻结消息节点**，各注入一个未知 varint 字段。字段号取接近上限的保留号，值是该节点的唯一标签。
3. 在测试实体上依次执行 `PlayerAllDataMessageFieldsUnmarshal` 和 `MarshalPlayerForPersist`。
4. 断言：
   - 输出里能找到全部标签；
   - 每个标签所在节点的消息类型，与注入时相同。

   按标签多重集比对，不依赖元素顺序。
5. **覆盖率断言**：从两张表可达的每一个非冻结消息类型，夹具里至少要有一个节点被注入；缺了就失败，并指出缺哪个类型。这样新增类型时，测试会强制补夹具。

新增一种会重建对象的写法时，这个测试会直接失败，并指出是哪个类型丢了字段。

### 4.4 成本

- 平时（所有节点同版本）没有未知字段：顶层未知字段集合为空，欠款的 `unknownFields` 为空串，不做反射遍历。
- 混跑期间的开销，与未知字段字节数的拷贝量成正比。

---

## 5. 机制二：覆盖世代校验

### 5.1 校验点

| 点 | 位置 | 条件 | 动作 | 观测 |
|---|---|---|---|---|
| C3 | C++ 严格提交屏障 Lua | 本次写方**加载时**的世代 < hash 里的 `schema_gen` | warn：照写并返回 2；enforce：拒写并返回 3（§5.2） | C++ 日志行 `metric=persist_schema_regression where=redis`，LogQL 告警（C++ 节点没有 Prometheus 出口，见评审 M9） |
| C4 | login 加载父快照时（`beginParentLoad` 快路径） | 父快照两表戳的最大值（0 按 1 处理）< hash 里的 `schema_gen` | warn：照常 EnterGame 并计数；enforce：拒绝 EnterGame，返回维护类 tip | `login_persist_schema_regression_total` |
| C1 / C1b / C2 / C0 | go/db 写与读 | 见 §6.2 | 延后 / 死信 / 读失败 | `db_task_deferred_total{reason=...}` 等 |

**为什么 C3 比较"加载时的世代"而不是"输出世代"**（修正评审 S2）：输出世代是 `max(本世代, 加载戳)`。如果比较输出世代，被机制前写方截断过、戳归 0 的数据，经过一个高世代 scene 时会被重新盖上高世代，截断就被"洗白"了。比较加载时的世代，才能发现"这份数据被截断过"。

### 5.2 Redis 严格提交屏障的改动

| 项 | 改动 |
|---|---|
| 参数 | 从 7 个增加到 9 个：`ARGV[8]` 是输出世代，`ARGV[9]` 是加载时的世代，都是十进制，按 `is_u32` 校验。检查 `#ARGV ~= 7` 改为 9。旧二进制自带旧脚本源码，只发 7 个参数，不受影响；`ScriptSlot` 的 SHA 由 `SCRIPT LOAD` 按源码计算 [C] |
| 比较函数 | 新增 `num_gt`：先比长度，再按字节比较（`is_u32` 已经拒绝前导 0）。现有的 `gt` 只适用于等长字符串，直接沿用会把 9 升到 10 判成倒退，前缀相同时还可能触发 Lua 运行时错误（评审 M2） |
| 校验位置 | 现有 `owner` / `pending` 校验之后、`SET` 之前。hash 不存在或没有 `schema_gen` 时不比较 |
| 写入 | warn 和 enforce 两种模式下，凡是写入成功，都在同一个 `HSET` 里写 `schema_gen = ARGV[8]` |
| 返回值与回调（评审 M1） | 现有 `OnSaved` 只把整数 1 当成提交证据，错误回包一律重试，0 走"已被废黜"分支（`redis_client.h`）。改为：1 是正常提交；2 是已提交但观测到倒退，按提交处理、照常投递 DBTask，并打日志计数；3 是因倒退拒写，新增回调，按"拒存"处理（与持久组件缺失时的 `SaveResult::kRejected` 同口径：不废黜，退出时 `DestroyDeposedPlayer` 不存盘丢弃）。需要改 `OnSaved` 和 `player_lifecycle.cpp` 的拒存分派 |
| 模式配置 | `BaseDeployConfig` 新增 `persist_schema_regression_mode: warn\|enforce`，默认 warn。改动面包括 `config.cpp` 的读取、`bin/etc/base_deploy_config.yaml`、`k8s_deploy.ps1` 手工渲染的 ConfigMap 及其契约测试、`start_game.ps1`（评审指出）。login 的 C4 用 `login.yaml` 里的同名配置项 |

**enforce 的代价**：拒存意味着这名玩家本次在线的进度不会落盘。按 §5.3 的推导，合法流程不会触发 C3 / C4，触发了就说明数据已经被截断，此时宁可不写，也不能让截断的数据覆盖 SQL。

### 5.3 为什么合法流程不会触发 C3 / C4 [C/I]

- scene 加载时的世代来自父快照。
- 父快照的最近写入者如果是 scene，两者的世代相等。
- 如果最近写入者是 login 冷加载：它要求两表 SQL ACK 和行围栏都等于 pending 才发布（`parent_commit_barrier.go`）[C]，说明 SQL 行已经包含最近一次 scene 写入，行戳 ≥ hash 记录值。
- 如果最近写入者是 login 的 backfill：Go 原样保留，戳不变。
- 新号种子：hash 还不存在。
- 引导期：存量数据的 0 和 1 等价（§3.3）。

唯一合法的例外是**回档**：回档恢复的快照戳可能更低。回档的 `RollbackFence` 至今没有被赋值，一律返回 NotImplemented。将来实现回档时，必须显式重置 `schema_gen`，并写进回档设计。

### 5.4 各写入方的要求

| 写入方 | 现状 [C] | 要求 |
|---|---|---|
| login 冷加载组装 → `snapshot.publish` | Go 原样保留 | C4 只在快路径做。冷加载组装出来的父快照的戳来自 SQL 行，并且已经过 ACK 校验 |
| login `backfillPlayerIdentity` | Go 读改写，再 CAS 写回 | 不改逻辑；加测试，确认未知字段和戳都保留 |
| login 新号种子 `new_player_seed.go` | 骨架 Marshal 后写入；`isUnownedLoginSeed` 用 `proto.Equal` 把父快照与骨架比较（包括未知字段） | **种子不盖戳**（保持 0）。如果盖了戳，login 版本混跑时不同 pod 盖的戳不同，比较就会失败，新号首次入场会一直失败（评审 M3）。由 scene 首次存盘时盖戳 |
| go/db 读回执与子表缓存 `SetNX` | 来自 SELECT | 经过 C2 后，原样包含行戳 |
| go/db 写后发布缓存 / ACK / 资产证据 | 发布的是 `msg`（body）；equal 结局用加锁读到的行（`proto.Reset` 加 `proto.Merge(current)`） | 不改。机制三不再部分写入，body 就是写入的整行 |
| go/db 补投 `saveoutbox` | 原样投递 Redis 里的 DBTask 字节 | 无（字节里已带戳） |
| `go/db/cmd/save_barrier_restore` | 要求父快照不存在；hash 存在时要求 `HLEN == 3`，还会 HSET hash | 接受 `schema_gen` 字段（把 HLEN 的判断改为字段白名单）；重建 hash 时写入 `schema_gen = 两行戳的最大值` |
| `go/db/cmd/retry_requeue` | 重放死信 | 重放的任务照样经过 §6.2 的判定 |
| `tools/merge_zone` | 按列名做 SQL 级复制；会删除 save_barrier hash | 无。戳随列复制；hash 被删后没有 `schema_gen`，按"不比较"处理 |
| `go/data_service/cmd/debug_import`（只在本地用） | 只用工具认识的列 DELETE + INSERT，并删除父快照和屏障 | 增加前置检查：行戳 > 工具世代时拒绝导入 |
| data_service 快照、回档 | 回档没有接线 | 将来实现时：原样保存字节，显式重置 `schema_gen` |
| `go/db/cmd/data_stress` | 压测工具，直接构造 body | 盖本二进制世代 |
| `go/db/cmd/verifier` | 只读比对 | 把戳当普通列比较 |

评审核实的结论 [C]：
- `java/`、`go/scene_manager`、friend、match，以及 data_service 的内部逻辑，对玩家数据都是只读；
- 旧的 Kafka 跨 zone 搬数据链已经下线；
- C++ 加载入口只有 `InitPlayerFromAllData` 一处。

每批实施时，仍要按 AGENTS §11.4 重新用 grep 清点一遍：`PlayerAllData`、`PlayerDatabase{`、`player_database_1`、`MessageFieldsMarshal`、`proto.Reset`、`protojson`、`DiscardUnknown`。

---

## 6. 机制三：go/db 不写写方不认识的列

### 6.1 为什么不是"只写改动列"，也不是"按列掩码写"

负责人原话是"只写改动列"。本设计改为"**超前就延后、倒退就拒写，其余整行写**"，理由有三条：

1. **兼容要解决的是"写方不认识的列被写成零值"**，这个条件由写方世代唯一确定。"改动列"解决的是另一个问题（减少写入量）。
2. **"只写改动列"会破坏现有的收敛保证。** 现行设计（`db-task-delivery-and-retention.md` §A）要求每次存盘按当前内存整份重写，绝不重放旧载荷。只写改动列时，一旦某次写入被围栏拒绝或进了死信，它改过的列在后续写入里不会再出现，MySQL 就会永远缺这几列，而且不报错。此外，这种做法需要 C++ 维护逐列脏标记，正是 `proto_dirty_compare.h` 文首记录的那类"忘了置脏就丢数据"缺陷。
3. **"按写方认识的列掩码写"（v0.1 的方案）在守住 P1 时不改变任何结果。** 掩码只在 body 世代 `s` < 行世代 `r` 时才生效，而这种情况本身就说明链路上有截断。即使这时用掩码保住了新列，老列 BLOB 里世代介于 `s` 和 `r` 之间的嵌套字段仍会被覆盖，同时行戳 `r` 还在声称数据完整（评审 M5）。所以直接拒写，比部分写入更正确，也更简单（KISS）。

如果将来要按改动列减少写入量，应该作为独立的性能议题，并且要有压测数据支撑（AGENTS §11.3）。

### 6.2 规则

记号：body 戳为 `s`，本实例世代为 `G`，加锁读到的行戳为 `r`。比较时 0 都按 1 处理。

| 编号 | 位置 | 条件 | 动作 |
|---|---|---|---|
| C0 | 写任务，进 SQL 之前 | 本 zone 已关闭引导期（Redis 标记 `db:persist_schema:bootstrap_closed:zone:{z}`，仿照 `db:save_protocol` 的世代标记），并且 `s` 原值为 0 | 延后，不耗预算，原因 `persist_unstamped`。这样能拦住误部署的机制前 scene（评审 M8）；它的 DBTask 不落库，SQL 保住完好数据 |
| C1 | 写任务，进 SQL 之前 | `s > G` | 延后，不耗预算，原因 `schema_too_new`。**按世代号判断，不看有没有顶层未知字段**：proto3 不序列化零值，新写方把新列清零时，body 里根本没有这个字段（评审 S3） |
| C1b | 写任务，在行内围栏事务里、围栏判定之后、`tx.Save` 之前 | `s < r` | 违反 P1。**不写**，直接进死信，原因 `schema_regression`，不重试（重试也修复不了）；告警。Redis 父快照仍保有这份数据 |
| C2 | 读任务，SELECT 之后、回填缓存与回执之前 | `r > G` | 读失败，立即回执 `Success=false, Error="schema_too_new"`，不回填缓存、不延后。login 按现有失败路径处理，玩家稍后重试。读任务按 L-04 的规则会消耗重试预算，与其排队，不如立即失败（评审 M7） |
| 其余 | | | 与现行逻辑相同：整行 `tx.Save`。行不存在时走事务外的 `Insert`，本版本认识的全部列写入消息里的值（proto3 零值），与"从没写过"等价 |

**两点说明：**
- **行戳就是 body 自带的那一列。** 因为 C1b 已经排除了 `s < r`，写入后行戳一定 ≥ 原值，不需要额外的 max 逻辑。
- **C1b 要放在 dual / legacy 两种模式共用的 `RunInTransaction` 里。** 两种模式都在这里用 `FindOneByPKForUpdate` 读当前行（`player_row_fence.go`）[C]，所以判定与写入在同一把行锁下。围栏结局 older_seq、stale_owner、equal 维持现有优先级，先于 C1b。

**延后的代价**（评审核实）：延后作用于单条任务，任务先持久化进重试队列、再提交位点，不阻塞分区；同一个 key 的后续任务照常处理（`key_ordered_consumer.go`、`placement_route.go`）[C]。在"go/db 回滚"或"违反发布顺序"期间，旧实例上同一 key 的每次存盘都会延后，重试列表会随存盘次数增长 [I]。所以 `schema_too_new` 告警阈值设为持续 5 分钟不为 0，运维手册的处置是"升回去或继续滚完"。

### 6.3 新增延后原因的接入

`deferReason`（`store_failure.go`）和 `retryBudgetExempt`（`placement_route.go`）各增加一个哨兵错误。`deterministicWriteRejectReason`（`save_outbox_note.go`）不收它，按默认的"在途重试"处理，这是对的（评审核实）。

### 6.4 DDL 顺序

沿用现有门禁：新版 go/db 启动时，库里缺 proto 声明的列就拒绝启动（`assertSchemaUpToDate`）[C]，所以 DDL 一定先于新版 go/db。新列在新版 scene 写入之前一直是默认值。

---

## 7. 混版本矩阵

假设世代 6 新增顶层列 `X` 和子消息字段 `y`。G 是 go/db 的世代。

| 场景 | 结果 |
|---|---|
| 新 scene（6）→ 新 go/db（6） | 整行写，行戳 6 |
| 旧 scene（5）接手戳为 6 的数据 → 新 go/db（6） | 对象内携带 `X`（顶层未知字段）和 `y`；`s = 6` 等于 G，整行写，不丢 |
| 旧 scene（5）接手戳为 5 的数据 → 新 go/db（6） | `s = 5`，`r ≤ 5`，整行写；`X` 写入零值，本来就没被写过，没有影响 |
| 新 scene（6）→ 旧 go/db（5）（违反顺序，或 go/db 回滚） | C1 延后并告警；回到新 go/db 后再处理。不丢 |
| 冷加载：行戳 6 → 旧 go/db（5）读 | C2 读失败，玩家稍后重试；告警 |
| 冷加载：行戳 6 → 新 go/db（6）读 → 旧 login 组装 | Go 保留 `X`、`y` 和戳 |
| 引导期关闭后，误部署机制前的 scene 接手玩家 | 它写出的父快照戳为 0，并且已经截断；它的 DBTask 被 C0 延后，SQL 不受影响；下次加载时 C4 / C3 判定为倒退（加载世代 1 < 6），enforce 模式下拒绝。数据以 SQL 为准恢复，需要人工清理父快照后走冷加载，运维手册写清步骤；机制前 scene 在线期间的进度会丢，这是误部署的代价 |
| 全部 scene 从 6 回滚到 5 | 对象内携带，数据保持戳 6；之后升回 6 不丢。**前提**：世代 6 没有扩展持久值域（§8.3） |

---

## 8. 发布与回滚顺序

### 8.1 引导期：上线机制本身

1. 台账写入世代 1 的基线；冻结清单通过生成器检查；往返一致性测试通过。
2. **引导期内不得有任何世代 > 1 的改动**，由生成器强制：台账带"引导中"标记时，拒绝追加世代。
3. 发布顺序：DDL（两表加戳列，默认 0）→ go/db 全部 zone（此时 C0 未开、`G = 1`，行为与现在相同）→ login → scene（携带、盖戳、C3 / C4 都是 warn）。
4. **判定全网再无机制前写方**：观察 go/db 侧原值为 0 的写 body 计数。这个计数只能放在 go/db 侧，因为机制前的 scene 根本不知道这个字段（评审 M8）。计数归零并持续一个完整的存盘周期后，由负责人：
   - 在每个 zone 写入引导关闭标记，打开 C0；
   - 去掉台账的"引导中"标记。
5. C3 / C4 的倒退计数持续为 0 一段时间后，切到 enforce。

### 8.2 此后每次升世代的发布顺序

`DDL（如果有新列）→ go/db 全部 zone → login → scene（可以按 zone 金丝雀）`

顺序错了，C1 / C2 只会延后并告警，不会丢数据。

可选（第 4 批）：go/db 的能力心跳 `db:capability:zone:{z}` 带上自己的世代，`k8s_deploy.ps1` 在发布 scene 前做只读预检。

### 8.3 回滚规则

| 对象 | 能否回滚 | 说明 |
|---|---|---|
| login | 可以回滚到任何带机制的版本 | Go 原样保留 |
| scene | **有条件** | 对象内携带保证未知字段不丢，但**持久值域扩展**不在台账里。比如新增包类型后回滚，`bag_marshal::Unmarshal` 会丢掉越界的物品；新增技能后回滚，`SanitizeSkillList` 会删掉查不到的技能（评审 S5）。§12 的这两处修好之前，只有"新版本没有扩展持久值域"时才能回滚 scene，发布单里要写明这一点 |
| go/db | 只能回滚到世代 ≥ 数据最高戳的版本 | 回滚得更低，会卡住存盘和冷加载（C1 / C2），但不会丢数据。处置：升回去 |
| 回到机制前的二进制 | 禁止 | 引导期关闭后，有 C0 和 C3 / C4 兜底 |

### 8.4 告警与运维手册

| 告警 | 来源 | 含义 | 处置 |
|---|---|---|---|
| `DbTaskSchemaTooNew` | go/db Prometheus | go/db 世代低于数据 | 检查 go/db 版本，升回或继续滚完 |
| `DbTaskPersistUnstamped` | go/db Prometheus | 有机制前写方 | 找到并下线该 scene |
| `PersistSchemaRegression` | go/db 死信原因、login 指标、C++ 日志（LogQL） | 链路上有人截断了数据 | 按 `where` 定位跳点；暂停该跳的发布；按运维手册清理父快照后走冷加载 |

运维手册写进 `deploy/k8s/README.md` 对应小节。Prometheus 告警文件新建 `deploy/k8s/persist-schema-alerts.yaml`。C++ 侧告警按 `docs/ops/grafana-loki-local-logs.md` 的 LogQL 方式配置。

---

## 9. 分批实施（每批 < 30 个文件，AGENTS §10.2）

| 批 | 内容 | 主要改动 | Codex 验证 |
|---|---|---|---|
| 1 | 台账、生成器（发号、冻结检查、R2 / R3、引导中标记）、冻结 option、两表戳字段、生成常量 | `proto/db/proto_option.proto`、`mysql_database_table.proto`、`bag_quest_mail_data.proto`、`player_cache.proto` 的冻结标注；`proto/persist_schema_gen.txt`；`tools/proto_generator/protogen/internal/...`（新的 persist_schema 生成器及测试）；生成产物 | `proto-gen-build`、`proto-gen-run -ConfigPath tools/proto_generator/protogen/etc/proto_gen.no-unity.yaml`、protogen 的 `go test ./...`；之后完整编译 C++ 与所有 Go module（AGENTS §4.2）。生成的 .pb 文件较多，如果文件数超过 30，按 §10.2 先报告 |
| 2 | go/db：C0 / C1 / C1b / C2、两个新延后原因、死信原因、指标、告警、引导标记读取；cmd/migrate 的 DDL；save_barrier_restore、debug_import、data_stress 的配套改动 | `go/db/internal/kafka/{player_row_fence,key_ordered_consumer,store_failure,placement_route}.go`、`go/db/internal/metrics`、`go/db/internal/barrierrestore/scripts.go`、`go/data_service/cmd/debug_import`、`deploy/k8s/persist-schema-alerts.yaml`、测试 | go/db 与 data_service 的 `go test ./...`、`go vet`；真库集成测试（自起一次性 MySQL / TiDB） |
| 3a | C++ 携带：`PlayerPersistBaseComp`、统一入口、欠款与任务列表携带、`SanitizeSkillList` 保留顶层未知字段、回档快照改入口、盖戳、往返一致性测试 | `services/scene/player/{comp,system}/...`、`modules/currency/comp/player_currency_comp.h`、`mission_marshal.cpp`、`player_skill.cpp`、`snapshot_system.cpp`、测试工程的 vcxproj / CMakeLists | `game.sln` 串行 `/m:1` 完整编译；新测试和现有存盘相关单测 |
| 3b | C++ 校验：严格屏障 Lua 9 个参数、`num_gt`、返回值 2 / 3、`OnSaved` 与拒存分派、`BaseDeployConfig` 配置项及部署渲染 | `redis_client.h`、`player_lifecycle.cpp`、`base_deploy_config` 的 proto / `config.cpp` / yaml、`k8s_deploy.ps1` 与契约测试、`start_game.ps1` | `game.sln` 完整编译；Redis 脚本测试（包括 `RedisSaveScriptsTouchOnlyDeclaredKeys`）；k8s_deploy 契约测试 |
| 4 | login：C4（`beginParentLoad` 返回 `schema_gen` 并比较）、种子保持不盖戳的测试、backfill 往返测试、`login.yaml` 配置项；可选的能力心跳与部署预检；运维手册 | `go/login/internal/logic/pkg/dataloader/...`、`go/login/etc/login.yaml`、`deploy/k8s/README.md`、可选 `tools/scripts/k8s_deploy.ps1` | login 的 `go test ./...`、`go vet` |
| 5 | §12 的两处持久值域删数据点（这是 scene 无条件回滚的前提）：查不到的技能保留但不生效、越界包类型的物品保留 | `player_skill.cpp`、`bag_marshal.cpp` 及测试 | 同 3a |

每一批完成后，在 `docs/PROGRESS.md` 追加条目，并注明 Java 版状态（§11）。

---

## 10. 测试计划

**C++（3a / 3b / 5）：**
- 往返一致性测试（§4.3），包括覆盖率断言。
- 欠款：删除后用同一个 `currency_type` 重建，新欠款**不带**旧欠款的未知字段（S1 的回归测试）；不删除时未知字段保留。
- 任务：放弃后用同一个 ID 重新接取，新任务不带旧的未知字段。
- 两表顶层未知字段往返保留；戳等于 `max`；同一份数据存两次，快路径命中。
- 严格屏障 Lua 的用例：
  - `num_gt` 的边界：9 对 10、10 对 9、1 对 19、相等；
  - hash 不存在、没有 `schema_gen`、倒退时 warn 返回 2、enforce 返回 3；
  - 参数个数不对时报错。
- `OnSaved` 对 2 和 3 的处理。

**Go（2 / 4）：**
- C0：引导期关闭后，`s = 0` 的写任务延后，不耗预算。
- C1：`s > G` 延后、不耗预算；子用例：body 里没有新列（零值），也照样延后。
- C1b：`s < r` 进死信，原因正确，SQL 没有被改动；围栏结局（older、equal）的优先级不变。
- C2：`r > G` 时读立即失败，不回填缓存。
- 用 `dynamicpb` 模拟高世代 body。
- login C4 的 warn / enforce；种子不盖戳时 `isUnownedLoginSeed` 依然成立；backfill 往返保留未知字段与戳。
- save_barrier_restore 接受 `schema_gen` 字段；debug_import 的前置检查。

**生成器（1）：**
- 新增字段追加世代；删除字段写了 reserved 时标记 retired；
- 以下改动生成失败：改类型、复用字段号、oneof 加成员、冻结类型加字段、引导中追加世代；
- 输出稳定可重复。

**真实环境（第 2、3 批之后，由 Codex 或负责人执行）：**
- 两套二进制混跑，按 §7 的矩阵逐行验证。
- 这一项不能用单测代替。它跑完之前，不能宣称"混版本安全"。

---

## 11. Java 版对齐（AGENTS §12）

- **形态** [C]：玩家状态是 `player_state` 表的单个 BLOB 列 `data`，内容是 `PlayerState` PB，读写用 `toByteArray` / `parseFrom`（`game-player-store` 的 `PlayerStore.java`、`PlayerMapper.java`）。`player` 表的其他列由固定 SQL 写入。
- **适用性**：
  - 机制三不适用：BLOB 整体写入，`player` 表的列由编译期固定的 SQL 指定。
  - 机制一（对象内携带 + 往返一致性测试）和机制二（世代戳 + 加载与写入校验）同样需要。Java 的 protobuf 默认保留未知字段，但从运行时对象重建 `PlayerState` 的路径会丢失，而且 `PlayerState` 没有戳。
- **状态**：**待做**。原因是 Java 版要按自己的存储形态实现（§12：服务端内部按 Java 惯用方式），本批只出 C++/Go 版设计。第 1 批实施时，在 Java 仓库的 `PARITY.md` 登记一行。

---

## 12. 不在本次范围、但属于同类的丢数据点

| 问题 | 现状 [C] | 建议 |
|---|---|---|
| 持久值域：旧配置表删除新技能 | `SanitizeSkillList` 删掉表里查不到的技能，下一次存盘写回 | 第 5 批：查不到就保留，但不生效 |
| 持久值域：越界包类型的物品被丢弃 | `bag_marshal::Unmarshal` 丢弃 `bag_type` 越界的条目；`capacities` 只按 `kBagTypeCount` 输出 | 第 5 批：保留越界条目，原样存回 |
| 收件箱、资产通道的载荷新字段 | 旧 scene 执行器忽略新字段，照常执行并记为 APPLIED | 载荷加版本号，或执行器遇到未知字段时 fail-closed；收件箱消费开启前必须修 |
| Kafka 控制面命令 | 遇到未知事件直接丢弃 | 新增事件类型时必须配发件箱或重投 |
| guild、mail、trade 等各自的库表 | 未核对 | 按本文的不变量逐个评估 |
| 客户端契约 | 见 `published-contract-compatibility-20261017.md` | 已有独立方案 |
| 顺带发现，待确认 | 手写的 `PlayerDatabaseMessageFieldsMarshal` 不处理已知字段 `merge_state(10)`，scene 整行写会把它清空（评审指出） | 确认这是否是有意的"首次存盘清除合服标记"；如果不是，单独处理 |

---

## 13. 风险与待负责人拍板

1. **"只写改动列"改为"超前延后、倒退拒写、其余整行写"**（§6.1）。请确认。
2. **option 的落点**：推荐生成器动态读取扩展（§3.2）。
3. **C3 / C4 先 warn 后 enforce**；enforce 的代价是被截断玩家本次在线的进度不落盘（§5.2）。
4. **引导期"禁止升世代"的时长**，取决于全网换版的速度，期间所有给持久数据加字段的需求都要等。关闭条件见 §8.1 第 4 条。
5. **冻结清单与 R2 / R4**：背包布局层、任务容器层冻结后，相关玩法要加持久字段，只能先做"携带改造"再解冻，或者把数据放进可携带的类型。这会影响以后的开发习惯，需要负责人认可。
6. **残余风险**：
   - 新旧字段语义耦合的情况无法机械检查；
   - 回档实现时必须显式重置 `schema_gen`；
   - 误部署机制前 scene 期间的在线进度会丢（§7）；
   - warn 模式期间，截断只告警、不拦截。
7. **评审清单**：凡是修改持久 proto 的 PR，都要检查：
   - 生成器是否追加了世代；
   - 是否给冻结类型加了字段；
   - 有没有字段迁移（R4）；
   - 有没有扩展持久值域（比如新的包类型、新的配置 ID 段），有的话要在发布单里写明"scene 不可回滚到此前版本"；
   - 是否在主干发号；
   - 新字段在旧写方原样携带时，语义是否仍然成立。

---

## 14. 证据索引

- **C++**：
  - `cpp/libs/services/scene/player/system/player_lifecycle.cpp`：`MarshalPlayerForSave`、`SavePlayerToRedisImpl`、`OnDbTaskDispatchSettled`、`InitPlayerFromAllData`；
  - `player_data_loader.h`：生成产物，模板 `tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl`；
  - `player_database_loader.cpp`、`player_database_1_loader.cpp`（手工维护）；
  - `cpp/libs/modules/currency/comp/player_currency_comp.h`、`modules/currency/system/currency_system.cpp`；
  - `mission_marshal.cpp`、`modules/mission/system/mission.cpp`、`bag_marshal.{h,cpp}`、`player_skill.cpp`；
  - `cpp/libs/modules/snapshot/snapshot_system.cpp`（另有 `kSnapshotSchemaVersion`，它是快照条目信封的格式版本，与本文的世代号无关）；
  - `cpp/libs/engine/infra/storage/redis_client/redis_client.h`：`kSaveWithCommitBarrierLuaScript`、`ScriptSlot`、`OnSaved`。
- **go/db**：
  - `go/db/internal/kafka/`：`key_ordered_consumer.go`（`handleDBReadOp`、`publishDBReadResult`、`handleDBWriteOp`、`runDBOp`）、`player_row_fence.go`、`player_cache_publish.go`、`store_failure.go`、`placement_route.go`、`save_outbox_note.go`；
  - `go/db/internal/logic/pkg/proto_sql/db.go` 的 `assertSchemaUpToDate`；
  - `go/db/internal/saveoutbox/generation.go`；
  - `go/db/internal/barrierrestore/scripts.go`。
- **proto2mysql**：v0.2.0（`D:\luyuan\proto2mysql`）的 `proto2mysql.go`（`saveByPrimaryKey`、`Insert`、`fieldsListSQL`）、`pbconv/convert.go`。
- **login**：
  - `go/login/internal/logic/pkg/dataloader/`：`ensure_player_all_data_async.go`、`parent_commit_barrier.go`（`beginParentLoad`、`isUnownedLoginSeed`）、`new_player_seed.go`；
  - `go/login/internal/logic/clientplayerlogin/player_class_backfill.go`。
- **其他**：`tools/merge_zone/player_rows.go`、`go/data_service/cmd/debug_import/bundle_import.go`。
- **proto**：
  - `proto/common/database/{player_cache,mysql_database_table,bag_quest_mail_data}.proto`；
  - `proto/db/proto_option.proto`；
  - `proto/message_id.txt`（台账先例）。
- **Java**：`D:\luyuan\xuanming-server-mmo\game-player-store\src\main\java\com\game\player\store\{PlayerStore,PlayerMapper,PlayerStateRow}.java`。
- **关联文档**：
  - `db-write-fencing-mysql.md`；
  - `db-task-delivery-and-retention.md`；
  - `db-task-store-unavailable-deferral.md`；
  - `ecs-component-access-rules.md`；
  - `bag-instance-layout-split.md`；
  - `data-overwrite-rollback-audit-20261018.md`；
  - `published-contract-compatibility-20261017.md`。

---

## 15. 修订记录

### v0.1（2026-10-19）

首稿。核心做法有三项：
- 从加载原件按身份键嫁接未知字段；
- Lua 比较输出世代；
- go/db 按"列 → 引入世代"做掩码写。

### v0.2（2026-10-19）

一轮只读对抗评审：8 条事实主张逐条对照代码核实，并用混版本反例攻击设计。评审给出的修改项如下，全部采纳。

| 评审项 | 问题 | v0.2 的修改 |
|---|---|---|
| S1 | 嫁接会把已删除元素的新字段复活到同键的新元素上，涉及资产路径 | 机制一改为"未知字段跟着对象走"；重建层要么携带，要么冻结；新增往返一致性测试（§4） |
| S2 | Lua 比较输出世代，截断会被"洗白" | 改为比较加载时的世代；新增 login 加载门禁 C4（§5） |
| S3 | proto3 不序列化零值，"有没有顶层未知字段"漏判新列清零 | C1 改为按世代号判断（§6.2） |
| S4 | 退役字段的残留字节会让 C1 / C4 永久卡住 | C1 不再看未知字段；C4（v0.1 的加载期拒载）随嫁接一起取消 |
| S5 | "scene 可以无条件回滚"说过头 | §8.3 改为有条件，新增第 5 批 |
| M1 | C3 的回包路径没有设计 | 定义返回值 2 / 3 及回调 |
| M2 | Lua 的 `gt` 只适用于等长字符串 | 新增 `num_gt` |
| M3 | 种子盖戳会破坏 `isUnownedLoginSeed` | 种子不盖戳 |
| M4 | 恢复工具要求 `HLEN == 3`；hash 缺 `schema_gen` 的语义没定义 | 恢复工具接受新字段；缺字段时不比较 |
| M5 | `s < r` 时掩码部分写入，行戳"撒谎"；与 P4 矛盾 | 改为直接拒写进死信；P4 写明引导期 warn 的例外 |
| M6 | 字段迁移不在规则里 | 新增 R4 |
| M7 | C2 与读任务预算冲突 | C2 改为读立即失败 |
| M8 | 引导期结束条件不可机械判定；可能出现两个"世代 6" | 0 戳计数放在 go/db 侧；新增 C0；只在主干发号 |
| M9 | C++ 节点没有 Prometheus 出口 | C++ 告警走日志行加 LogQL |
| 低风险项 | INSERT 的零值语义、`PlayerAllData` 不应作为根、`QuestAllData.active` 应冻结、"平时零开销"的说法、改动面低估 | 逐项修正 |
