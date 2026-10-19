# 新旧版本混跑时的玩家持久字段保护

> **状态**：设计稿 v0.1（2026-10-19，Claude）。**未落码、未编译、未测试**（AGENTS.md §10.1）。负责人评审通过后按 §9 分批实施，每批由 Codex 验证。
> **起因**：2026-10-18 的发布能力核对（灰度 / 不停服 / 金丝雀）结论之一：新旧版本混跑时会丢玩家持久字段。负责人 2026-10-19 要求先补这一块，具体三件事：①存盘保留不认识的字段；②Redis 父快照记录写方版本并校验；③go/db 只写写方认识的列。要求先出设计，再动代码。
> **范围**：只覆盖玩家数据链：C++ scene 内存 → Redis 父快照 `PlayerAllData:{id}` → Kafka `db_task_zone_*` → go/db → MySQL 两张玩家表；以及 login 冷加载、回档快照和离线工具这些旁路。guild、mail、trade 等服务各自的库表，以及客户端契约兼容，都不在本文范围内（§12）。
> **证据口径**：[C] 表示对照 2026-10-19 工作区代码读过；[I] 表示推断。行号以当日工作区为准，相关文件正被其他会话修改，行号会漂移，所以正文以函数名为主。

---

## 0. 结论速览

1. **根因只有一个**：链路上的某些环节会**从本版本认识的字段重新拼一份数据**，然后整份写回。新版本加的字段因此在旧版本手里被抹掉。会丢字段的环节有三类：C++ 存盘重拼（§1.3 表里 L1–L5）、go/db 只读写本版本认识的列（L6、L7），以及没有任何检测手段（L8）。
2. **用一条不变量统一三件事**。每份玩家持久数据带一个**覆盖世代**（`persist_schema_gen`），表示"这份数据里，这个世代及以前定义的字段都完整"。数据每经过一道读写，只有两种合法结局：
   - 完整保留，输出的世代 ≥ 输入；
   - 拒绝或延后处理。
   **绝不输出截断的副本。** 只要守住这一条，新版读到数据时缺失的新字段就一定是"从来没写过"，按默认值处理是对的，不需要额外的怀疑逻辑。
3. **三个机制**：
   - **机制一（嫁接）**：C++ 存盘时，把加载原件里本版本不认识的字段嫁接回新拼出的消息。只有加载到的数据里真有不认识的字段才启用，平时（所有节点同版本）不做任何额外工作。效果是旧版 scene 可以安全接手新数据，**scene 的二进制因此可以回滚**。
   - **机制二（覆盖世代校验）**：在四个点校验覆盖世代。旧版 go/db 遇到它保存不了的数据就延后，不耗重试预算；Redis 严格提交屏障拒绝让覆盖世代倒退的写；scene 加载时兜底。
   - **机制三（列掩码）**：go/db 只写写方认识的列。注意是按"写方认识的列"写，**不是按"改动过的列"写**，理由见 §6.1。
4. **世代号由生成器发**。仿照 `proto/message_id.txt` 增加台账文件 `proto/persist_schema_gen.txt`，生成器比对持久消息的字段集合，自动升世代号。生成器同时检查"这个改动旧写方能不能安全携带"，不能携带的改动在生成期直接失败（§3.2）。
5. **上线顺序**：先上线机制本身，期间不允许升世代号。之后每次升世代号都按"DDL → go/db 全部 zone → scene / login（可以金丝雀）"的顺序发。scene 和 login 可以回滚到任何带机制的版本；go/db 不能回滚到低于在用列世代的版本，回滚了也只会延后、告警，不会丢数据（§8）。
6. **Java 版**：存储形态不同（单个 BLOB），但存在同类风险，需要单独做，本批只登记（§11）。

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
go/db：Unmarshal(body) → 行内围栏事务 → proto2mysql tx.Save
  ▼
MySQL player_database / player_database_1
  （每个顶层字段一列；子消息字段按二进制 PB 存成 BLOB）

冷加载（父快照缺失时）：
login → go/db 读任务 → SELECT 本版本认识的列 → 回执字节 → login 组装父快照并发布
```

### 1.2 已经能保留未知字段的环节 [C]

| 环节 | 为什么能保留 |
|---|---|
| Go 侧的中间跳（login 冷加载组装、backfill 读改写、go/db 写入前的 Unmarshal） | Go protobuf 默认保留未知字段；`proto.Marshal` 会原样写回 |
| proto2mysql 的子消息列 | `pbconv.SerializeFieldValue` 对 MessageKind 用 `proto.Marshal`（`pbconv/convert.go`），子消息内部的未知字段随 BLOB 存下 |
| C++ 中直接以 PB 形式存放的组件（Transform、PlayerUint64Comp、PlayerSkillListComp、LevelComp、各类账本等） | 加载时 `emplace<Comp>(player, message.xxx())` 拷贝了整份子消息（包括未知字段），存盘时 `CopyFrom(*comp)` 再带回（`player_database_loader.cpp` 的 Unmarshal / Marshal） |
| 物品实例 | `ItemEntry.instance = 15` 承载整份 `ItemComp`（包括未知字段），见 `bag_marshal.cpp` 的 `ItemToEntry` / `ItemFromEntry` |
| 生成的总入口 | `PlayerAllDataMessageFieldsMarshal` 从传入的 `message` 拷一份 candidate 再改（`player_data_loader.h`）。如果传入的是加载原件，顶层的未知字段本来就能留下 |

### 1.3 会丢字段的环节 [C]

| 编号 | 位置 | 丢什么 | 触发条件 |
|---|---|---|---|
| L1 | `SavePlayerToRedisImpl` 用 `std::make_shared` 新建空的 PlayerAllData 交给 `MarshalPlayerForSave`；`OnDbTaskDispatchSettled` 等收敛比较也是 `PlayerAllData current;`（`player_lifecycle.cpp`） | PlayerAllData、player_database、player_database_1 三层的顶层未知字段，也就是**新增的整列** | 旧版 scene 接手新数据后的第一次存盘：EnterScene 之后的首存一定真写 |
| L2 | `PlayerCurrencyComp::SaveToProto` 先 `clear_debts()`，再逐字段重建（`player_currency_comp.h:50-65`） | `CurrencyDebtEntry` 的新字段 | 每次存盘。这是**资产路径** |
| L3 | `mission_marshal::Marshal` 先 `out.Clear()` 再重建；`Unmarshal` 只拷 `missions` 和 `mission_begin_time` | QuestAllData、QuestScopeData、MissionListComp 的顶层新字段 | 每次加载或存盘 |
| L4 | `bag_marshal::Marshal` 重建 BagAllData、DynamicBagData、ItemEntry 这些布局层 | 布局层的新字段（实例层由 instance 保住） | 每次存盘 |
| L5 | `SnapshotSystem::CaptureAndSend` 直接调 `PlayerAllDataMessageFieldsMarshal(player, allData)`，`allData` 是新建的空消息（`snapshot_system.cpp`） | 同 L1，丢在回档快照里 | 登录 / 登出快照 |
| L6 | go/db 写：`tx.Save` 只 UPDATE 本版本认识的全部非主键列（proto2mysql `saveByPrimaryKey`） | 新版 go/db 收到旧写方的 body 时，**把新列写成零值**；旧版 go/db 收到新写方的 body 时，写不了新列，新列停在旧值 | 混版本窗口内的每次存盘 |
| L7 | go/db 读：`FindOneByWhereClause` 只 SELECT 本版本认识的列（`proto2mysql.go` 的 `fieldsListSQL`）；之后 `SetNX` 回填子表缓存、发布读回执（`key_ordered_consumer.go` 的 `handleDBReadOp` / `publishDBReadResult`） | 旧版 go/db 读回的数据里没有新列，冷加载出来的就是截断副本 | 旧版 go/db 给父快照缺失的玩家做冷加载 |
| L8 | 全链路 | 没有写方版本，也没有字段覆盖信息 | 上面任何一种丢失都**零报错** |

同类但不属于"未知字段"的丢失，比如旧配置表导致 `SanitizeSkillList` 删掉表里查不到的技能，见 §12。

### 1.4 后果

- **不能金丝雀**：金丝雀的意思就是新旧版本混跑，而现在混跑会丢字段。
- **不能回滚**：新版本跑过一段时间，再回滚 scene，所有在线玩家的新字段都会被旧 scene 的第一次存盘抹掉。
- **丢了也不知道**：L8。

---

## 2. 目标与不变量

**P1：覆盖世代单调传播（核心不变量）。** 每份玩家持久数据带一个覆盖世代 `c`。数据每经过一跳读写（scene 存盘、login 组装、go/db 写 SQL 或读 SQL、离线工具），只能二选一：

- 完整保留输入里的全部字段，包括本跳不认识的，输出世代 `c' = max(本跳世代, c)`；
- 拒绝或延后，不产出任何副本。

**推论**：只要所有跳都守住 P1，数据里缺失的、世代大于 `c` 才引入的字段，就一定是从来没被写过，读方按默认值处理是对的。新版读方因此不需要"怀疑旧数据"的逻辑。

**P2：** 旧版 scene 接手带新字段的数据时完整携带，所以 scene 和 login 的二进制可以回滚。

**P3：** go/db 不写写方不认识的列；无法完整保存的任务，延后到能保存的实例去处理。

**P4：** 违反 P1 的情况一律 fail-closed，并配有低基数指标和告警。player_id 不进 label。

**非目标**：
- 不支持"旧写方修改与新字段语义耦合的数据"。字段作者必须保证新字段可以被原样携带，做不到就走 §3.2 的不可携带流程；
- 不解决配置表版本错位（§12）；
- 不支持 Redis Cluster 跨槽（沿用现行"同一 Redis 实例"前提）。

---

## 3. 核心概念：持久 schema 世代

### 3.1 世代号与台账

- **世代号**：`uint32`，从 1 开始单调递增，覆盖所有从持久根消息可达的字段。持久根是 `player_database`、`player_database_1` 和 `PlayerAllData`，生成器按 `OptionIsPlayerDatabase` 和 `PlayerAllData` 自动收集。
- **台账**：`proto/persist_schema_gen.txt`，仿照 `proto/message_id.txt` / `event_id.txt`：由生成器维护，提交进仓库，PR 里能直接看到 diff。格式如下：
  ```text
  1=baseline:<规范化字段集合的 sha256>
  2=player_database.foo_component(21):message:FooComp
  2=CurrencyDebtEntry.interest(9):uint64
  ```
  每行记录一个字段路径、字段号、类型，以及它是在哪个世代引入的。
- **生成器的行为**：每次 proto 生成时计算可达字段集合，与台账最后一个世代比较。
  - 只有新增 → 追加 `N+1` 的行；
  - 删除字段但写了 `reserved N;` → 允许，台账保留原行，标记为 retired；
  - 改字段号、改类型、复用字段号 → 生成失败（与 AGENTS §4.3 一致）。
- **生成产物**（都在 `generated` 输出树里，不手改）：
  - C++ 头文件：`constexpr uint32_t kPersistSchemaGen`；
  - Go 包 `go/shared/generated/persistschema`：常量 `Gen`，以及两张玩家表"列名 → 引入世代"的映射 `ColumnIntroducedGen`，供 go/db 做列掩码（§6）。
- **并行分支**：两个分支各自加字段，都会生成 `N+1`，合并时台账冲突。解决办法是重新跑生成器，它会给出 `N+1`、`N+2`，与 message_id.txt 冲突的处理方式相同。

### 3.2 可携带性检查（生成期强制）

旧写方能安全携带一个新字段的前提，是这个字段在旧写方手里能被**原样留下，或者能被准确找回**。以下改动做不到这一点，生成器直接报错，不允许进入世代台账：

| 规则 | 不可携带的改动 | 原因 | 正确做法 |
|---|---|---|---|
| R1 | 给"只能经过无身份键的 repeated 路径到达"的消息类型加字段 | 旧写方重建 repeated 元素时，嫁接找不到对应元素 | 给这个 repeated 字段声明身份键（R4），或者改走 §11.6 的实例层 |
| R2 | 给已有的持久 oneof 加新成员 | 旧写方设置另一个成员时会和携带回来的未知成员冲突，解析结果取决于字段顺序 | 新字段放在 oneof 外面，或者走破坏性变更流程（整批停机换版，或 expand/contract） |
| R3 | 改字段号、改类型、复用字段号 | 违反 AGENTS §4.3 | 用 reserved |
| R4 | 从持久根可达的 repeated message 字段，既没有声明身份键，也没有声明"冻结" | 无法判断能不能嫁接 | 加 `[(OptionPersistKey) = "<字段名>"]`，或者加 `[(OptionPersistKeyless) = true]`（声明后元素类型冻结，以后不许加字段） |

map 字段的身份键就是 map 的 key，不需要声明。repeated 标量不需要身份键：新增的 repeated 标量字段，整体作为上一层的未知字段携带。

身份键的现有候选（实施时由生成器逐个列出确认）[C]：
- `BagAllData.items` / `DynamicBagData.items`：`item_uuid`；
- `BagAllData.dynamic_bags`：`bag_id`；
- `QuestAllData.scopes`：`scope`；
- `QuestAllData.active`：`config_id`；
- `CurrencyComp.debts`：`currency_type`；
- `PlayerSkillListComp.skill_list`：`skill_table_id`；
- 宠物、各类账本里的 repeated 字段由生成器列出后逐个指定；
- `MailAllData` 现在没有写入方，声明冻结。

**option 的落点**：现有自定义 option 定义在 `proto/db/proto_option.proto`，生成器通过外部模块 `github.com/luyuancpp/protooption v0.0.21` 的编译绑定读取。新增 option 有两条路：
- 升级这个外部模块的版本。这需要负责人打 tag、推送，Claude 不做；
- 生成器改为从描述符原始字节动态读取扩展，不依赖编译绑定。

**推荐第二条**，实施第 1 批时确认。

### 3.3 世代戳放在哪

在两张表消息里各加一个 `uint32 persist_schema_gen`：
- `player_database` 用 21 号，按加列纪律取下一个空闲号，放在 message 末尾；
- `player_database_1` 用 5 号。

不另开 Redis 旁路键，也不在 PlayerAllData 里单独加字段。理由：

- **戳和内容同在一份字节里**，跟着已有的全部载体走：Redis 父快照、DBTask body、MySQL 列、go/db 子表缓存、跨 zone 迁移、回档快照。不需要给任何载体加第二份真相（DRY）。
- **Go 中间跳不认识这个字段也会保留它**（§1.2）。机制上线前的旧版 C++ scene 会把它丢掉，戳变成 0，这正好暴露"这一跳截断了"。
- **它本身是一列**，go/db 读写行时自动带上，§6 的"行戳"不需要额外查询。
- **取值约定**：0 表示"机制上线前写入"，一律按世代 1 处理。引导期不允许升世代（§8.1），所以存量数据的 0 和 1 等价。

### 3.4 每一跳输出的世代

| 这一跳 | 输出的世代 |
|---|---|
| C++ scene 存盘（有机制一） | `max(kPersistSchemaGen, 加载时两表戳的最大值)` |
| login 组装 / backfill（Go 原样保留） | 不改戳，原样透传 |
| go/db 写 SQL | 行戳取 `max(行上的戳, body 戳)`（§6.2） |
| go/db 读 SQL | 原样返回行戳；如果本实例保存不了这份数据，就延后（§5.1 的 C2） |
| 离线工具 | 只做列级 SQL 复制或只改元数据列的工具保持原戳；重建数据的工具必须满足"工具世代 ≥ 数据戳"，否则拒绝执行（§5.4） |

---

## 4. 机制一：C++ scene 存盘保留未知字段（嫁接）

### 4.1 设计要点

**新组件 `PlayerPersistBaseComp`**，属于持久数据类（`ecs-component-access-rules.md`），只在加载边界 `InitPlayerFromAllData` 里创建，每名玩家都有：

```cpp
// 持久数据类:只在 InitPlayerFromAllData 创建,之后不替换、不摘除。存盘入口缺它就整份拒存(fail-closed)。
struct PlayerPersistBaseComp
{
    // 加载数据上两张表 persist_schema_gen 的最大值;0 按 1 处理(§3.3)。
    uint32_t loadedCoverageGen{1};
    // 只有加载数据里确实存在未知字段时才持有加载原件;否则为空,平时不占内存。
    std::shared_ptr<const PlayerAllData> unknownBase;
};
```

- **加载时**用反射遍历一次加载数据。只要任何一层有未知字段，就把加载原件存进 `unknownBase`，并计数 `persist_schema_carry_newer_total`。遍历成本与消息大小成正比；按 `last_persisted_snapshot_comp.h` 记录的约 5 KB 估算，远小于一次存盘的序列化开销 [I]。
- **原件只读，加载后不刷新。** 旧代码碰不到它不认识的字段，所以这些字段的值在整个在线期间不会变。被旧代码删掉的元素，它的新字段随元素一起消失，这是正确的。

**统一存盘入口**：新增手写模块 `services/scene/player/system/player_persist_marshal.{h,cpp}`，提供：

```cpp
// 存盘、退出收敛比较、跨 zone 迁移、回档快照的唯一拼装入口:比的、写的、快照的是同一个形状。
// 返回 false = 持久组件缺失或嫁接遇到不可携带的数据;调用方必须拒绝这次写入。
[[nodiscard]] bool MarshalPlayerForPersist(entt::entity player, Guid playerId, PlayerAllData& out);
```

它的内部步骤：
1. 调 `PlayerAllDataMessageFieldsMarshal`；
2. 填两表的 `player_id`；
3. 如果 `unknownBase` 不为空，执行嫁接；
4. 给两表盖戳 `max(kPersistSchemaGen, loadedCoverageGen)`。

现有的 `MarshalPlayerForSave`（`player_lifecycle.cpp` 的匿名命名空间）改为转调这个入口。`SnapshotSystem::CaptureAndSend` 也改为调它（修 L5）。`player_data_loader.h` 是生成器按 `player_data_loader.h.tmpl` 生成的产物，**不改**。

### 4.2 嫁接算法

```text
Graft(base, out):
  # 1) 本层未知字段:out 本层没有未知字段时才并入,避免组件已经保留过一份、再并入一份造成重复
  #    (repeated 未知字段重复会让元素翻倍)
  if base.unknown 非空 and out.unknown 为空:
      out.unknown.MergeFrom(base.unknown)
  # 2) 已知的 message 类型字段,逐个下钻
  for f in out 的描述符里的 message 类型字段:
      单值:base 和 out 都有这个字段时 → Graft(base.f, out.f)
            base 有、out 没有 → 跳过(旧代码清掉了它,不复活)
      map:按 key 配对,两边都有的条目 → Graft
      repeated + 有身份键:按身份键配对,两边都有的元素 → Graft;
            base 里有、out 里没有的元素不复活;out 里新增的元素不处理
      repeated + 冻结:不下钻(R4 保证元素类型没有新字段)
```

**用到的能力**：工程使用 protobuf 全量运行时，`proto/` 里没有 `LITE_RUNTIME`；仓库已有反射工具 `engine/core/utils/proto/message_util.cpp`、`proto_dirty_compare.h` [C]。身份键表由生成器从 R4 的 option 产出，C++ 侧是一张常量表，不运行时解析 option。

**加载期兜底（§5.1 的 C4）**：如果在冻结元素或无身份键的元素内部发现未知字段，说明 R1 / R4 被绕过了。处理是**拒绝加载**，返回可重试的失败，计数 `persist_schema_uncarriable_total`，打 ERROR。不能"加载后再拒存"：那样玩家在线期间的进度最后全丢。也不能"照存"：那是静默丢字段。

### 4.3 对现有丢失点的效果

| 编号 | 效果 |
|---|---|
| L1 | 顶层未知字段由第 1 步补回 |
| L2 | `debts` 按 `currency_type` 配对补回 |
| L3 | 按 `scope` 和 map key 配对补回 |
| L4 | 按 `item_uuid` / `bag_id` 配对补回 |
| L5 | 改走统一入口 |

**已知局限**：元素在两个 repeated 容器之间移动时（比如物品从包 A 换到包 B），元素级的新字段会丢，因为配对只在同一个 repeated 路径内进行。**作者规则**：每件物品的新数据一律放进 `ItemComp`（instance 整份保留），不要加在 `ItemEntry` 这一布局层上（AGENTS §11.6）。这条写进生成器的报错提示，也写进 §13 的评审清单。

### 4.4 快路径与收敛比较

- `dirty_save::IsEqual` 比较的是统一入口的输出（已嫁接、已盖戳），和落盘快照的形状一致，所以快路径照常命中 [I]。
- 盖戳在快路径比较**之前**完成。如果放在之后，同一份数据会因为戳不同被误判为"有变化"。
- `last_save_seq` 依然在快路径之后写，现行顺序不变。

---

## 5. 机制二：覆盖世代校验

### 5.1 校验点

| 点 | 位置 | 条件 | 动作 | 指标与告警 |
|---|---|---|---|---|
| C1 | go/db 写任务，在 `handleDBWriteOp` 进 SQL 之前 | body 解码后**顶层**有未知字段（即本实例不认识的整列） | 延后，**不耗重试预算**：新增延后原因 `schema_too_new`，接入 L-04 的 `deferReason` / `retryBudgetExempt` | `db_task_deferred_total{reason="schema_too_new"}`；告警：持续 10 分钟不为 0 |
| C2 | go/db 读任务，SELECT 之后、回填缓存和发布回执之前 | 行戳 > 本实例世代 | 同 C1，延后不回执 | 同上，读写共用原因 |
| C3 | C++ 严格提交屏障 Lua（`kSaveWithCommitBarrierLuaScript`） | 本次覆盖世代 < `save_barrier` hash 里已记录的 `schema_gen` | warn 模式：照写，并返回"已写但倒退"的区分码；enforce 模式：拒写，scene 按拒存处理并保留实体 | `persist_schema_regression_total{where="redis"}` |
| C4 | C++ 加载（§4.2） | 在冻结元素或无身份键的元素内发现未知字段 | 拒绝加载（可重试） | `persist_schema_uncarriable_total` |

**C1 只看顶层未知字段，不比较世代号**：子消息里的未知字段会随 BLOB 原样保存（§1.2），旧版 go/db 能完整保存，不必延后。这样判断最精确，也不需要旧二进制知道未来的台账。

**C2 只能比较世代号**：旧版 go/db 的 SELECT 根本看不到它不认识的列，无从判断这些列里有没有数据。只要行戳大于本实例世代，就保守地延后。这意味着：只增加了子消息字段的世代，C2 也会让旧版 go/db 延后读。代价只出现在"go/db 回滚到低版本"这一种情况，属于有意的 fail-closed（§8.3）。

### 5.2 Redis 严格提交屏障的改动

- 脚本参数从 7 个增加到 8 个：`ARGV[8]` 是本次覆盖世代（十进制，按 `is_u32` 校验）。`#ARGV ~= 7` 的检查改为 8。旧二进制自带旧脚本源码，只发 7 个参数，不受影响（`redis_client.h` 里 ScriptSlot 按源码求 SHA）。
- 在 `save_barrier` hash 里增加字段 `schema_gen`。比较放在现有的 `owner` / `pending` 校验之后、`SET` 之前；按字节比较，沿用现有的 `gt` 函数，不转 double。通过后，和 `pending` 在同一个 `HSET` 里写入。
- **hash 里的 `schema_gen` 只是索引**，是"最近一次严格 scene 写入的覆盖世代"，权威仍是父快照字节里的两表戳（§3.3）。只有严格提交屏障脚本维护它。login 的冷加载发布和 backfill 都原样透传戳，它们的输出世代 ≥ hash 记录值（§5.3），所以不需要维护这个索引。
- **默认 warn 模式**（`BaseDeployConfig` 新增 `persist_schema_regression_mode: warn|enforce`）。实测零误报后改为 enforce，与配表指纹的 warn / enforce 先例一致。理由是 C3 拒写会让玩家无法存盘；虽然按 §5.3 的推导，合法流程不会触发 C3，但上线时先观测再拦截更稳妥。

### 5.3 为什么合法流程不会触发 C3 [I]

- scene 输出 = `max(本世代, 加载戳)`，加载戳来自父快照。
- 父快照的最近写入者如果是 scene，加载戳就等于 hash 记录值。
- 如果是 login 冷加载：login 要求两表 SQL ACK 都等于 pending 才发布（严格屏障），说明 SQL 行已经包含最近一次 scene 写入，行戳 ≥ hash 记录值。
- 如果是 login backfill：Go 原样保留，戳不变。
- 新号种子：hash 还不存在，不做比较。

**唯一的合法例外是回档**：回档恢复的快照戳可能更低。现在回档的 `RollbackFence` 还没有赋值、一律返回 NotImplemented（2026-10-18 审计）。将来实现回档时，必须显式重置 `schema_gen`，并写进回档设计。

### 5.4 其他写入方的要求

| 写入方 | 现状 [C] | 要求 |
|---|---|---|
| login 冷加载组装 `EnsurePlayerAllDataInRedisAsync` → `snapshot.publish` | 把 go/db 的读回执 `proto.Unmarshal` 进子消息，再 Marshal 发布；Go 保留未知字段 | 不改逻辑。加测试：带未知字段和戳的回执原样进入父快照 |
| login `backfillPlayerIdentity`（`player_class_backfill.go`） | 读父快照 → Unmarshal → 修改 → Marshal → CAS 写回 | 不改逻辑。加测试：未知字段和戳不丢 |
| login 新号种子 `new_player_seed.go` | 由骨架 Marshal 后写入 | 骨架盖本二进制世代（login 也引用生成的常量） |
| login `role_appearance_backfill.go` | 只读 | 无 |
| go/db 读回执与子表缓存 `SetNX`（`publishDBReadResult`） | 来自 SELECT | 经过 C2 后原样包含行戳 |
| go/db 写后发布缓存、ACK，以及 equal 结局（`proto.Reset(msg); proto.Merge(msg, current)`） | equal 时来自加锁读到的行；其余情况来自 body | 发布内容改为**提交后的整行**（§6.3） |
| go/db 补投 `saveoutbox` | 原样投递 Redis 里的 DBTask 字节 | 无（字节里已带戳） |
| `go/db/cmd/save_barrier_restore` | 只改两行的围栏元数据，要求父快照不存在 | 无。补一条前置检查：工具世代 ≥ 行戳，否则拒绝执行 |
| `tools/merge_zone` | 按列名做 SQL 级复制，执行前比较两库列集合 | 无（列级复制原样保留戳） |
| data_service 快照、回档 | 回档没有接线 | 将来实现时：原样保存字节，回档显式重置 `schema_gen`（§5.3） |
| `go/db/cmd/data_stress` | 压测工具，直接构造 body | 盖本二进制世代 |
| `go/db/cmd/verifier` | 只读比对 | 比对时把戳当普通列 |
| C++ 跨 zone 迁移携带 PlayerAllData | 经存盘链路 | 实施第 3 批时复核它是否全部经过统一入口；有旁路就并入统一入口 |

这张表是静态清点结果。实施每一批时，都按 AGENTS §11.4 重新用 grep 清点一遍：`PlayerAllData`、`PlayerDatabase{`、`player_database_1`、`MessageFieldsMarshal`、`proto.Reset`、`protojson`、`DiscardUnknown`。

---

## 6. 机制三：go/db 按写方已知列写（列掩码）

### 6.1 为什么不是"只写改动列"

负责人原话是"只写改动列"。本设计改为"**只写写方认识的列**"，理由如下：

1. **兼容需要的是后者。** 混版本丢字段的原因，是写方把它不认识的列写成了零值。"写方不认识的列"由写方世代唯一确定，是精确条件。"改动列"是另一个问题（减少写入量），解决不了兼容。
2. **"只写改动列"会破坏现有的收敛保证。** 现行设计（`db-task-delivery-and-retention.md` §A）要求每次存盘按当前内存**整份重写**，绝不重放旧载荷。这样只要后一次写入成功，前面丢失的写入就自动被覆盖。如果只写改动列，一旦某次写入被围栏拒绝或进了死信，它改过的列在后续写入里不再出现，MySQL 就会永远缺这几列，而且没有任何报错。
3. **"只写改动列"要求 C++ 维护逐列脏标记。** 这正是 `proto_dirty_compare.h` 文首记录的"忘了置脏就丢数据"那类缺陷。

如果将来确实要按改动列减少写入量，应该作为独立的性能议题处理，并且有压测数据支撑（AGENTS §11.3）。

### 6.2 规则

设 body 戳为 `s`（0 按 1 处理），本实例世代为 `G`，加锁读到的行戳为 `r`：

| 情况 | 动作 |
|---|---|
| body 顶层有未知字段 | C1 延后（§5.1） |
| `s ≥ G` | 与现行逻辑相同：整行 `tx.Save` |
| `s < G` | `tx.UpdateFieldsByPK(target, cols...)`。`cols` 包括：`ColumnIntroducedGen[列] ≤ s` 的列、两个围栏列、戳列。proto2mysql v0.2.0 已经有这个接口（`proto2mysql.go` 的 `UpdateFieldsByPK`），不需要升级依赖 [C] |
| 行不存在（INSERT 路径） | 与现行逻辑相同。写方不认识的列取 DDL 默认值，这与"从没写过"等价 |
| 任何情况下的行戳 | 写入 `max(r, s)` |
| `s < r` | 违反 P1（链路某处截断过数据）。仍按 `s < G` 的掩码写，从而保住更新世代的列；同时计数 `persist_schema_regression_total{where="godb"}` 并告警，因为这说明链路上有截断，需要排查 |

**掩码要放在现有的行内围栏事务里**（`player_row_fence.go` 的 `RunInTransaction`，FOR UPDATE 读到 current 之后）。dual 和 legacy 两种围栏模式都在这个事务里读当前行，所以行戳取 max、选用掩码都和围栏判定处于同一把行锁下，不会出现检查与写入之间的竞态。非玩家表不受影响。

将来 `SaveConditional`（proto2mysql v0.3.0）替换过渡原语时，掩码作为它的列集合参数，语义不变。

### 6.3 发布内容

写入成功后，发布缓存、SQL ACK 和资产证据的内容，改为"**提交后的整行**"：取加锁读到的 current，用本次写入的列覆盖。现在发布的是 body（equal 结局除外），而掩码写入时 body 缺少更新世代的列，直接发布 body 会造出一份截断的缓存。在 `s ≥ G` 的常规情况下，提交后的整行就等于 body，行为不变。

### 6.4 DDL 顺序

沿用现有门禁：新版 go/db 启动时如果库里缺 proto 声明的列，就拒绝启动（`proto_sql/db.go` 的 `assertSchemaUpToDate`）[C]。所以 DDL 一定先于新版 go/db。旧版 go/db 看到多出来的列只打 SCHEMA-DRIFT 日志。在新版 scene 写出新列之前，这些列一直是默认值，旧版 go/db 不受影响。

---

## 7. 混版本矩阵

假设世代 6 新增顶层列 `X`，以及子消息字段 `y`。go/db 已按顺序先升级，下表也列出顺序被违反时的结果。

| 写方 → 处理方 | 结果 |
|---|---|
| 新 scene(6) → 新 go/db(6) | 整行写；行戳 6 |
| 旧 scene(5) 接手戳为 6 的数据 → 新 go/db(6) | 嫁接带回 `X`（顶层未知字段）和 `y`；body 戳 6，`s ≥ G`，整行写，不丢 |
| 旧 scene(5) 接手戳为 5 的数据（这名玩家从没被新版写过）→ 新 go/db(6) | body 戳 5，`s < G`，掩码不写 `X`；`X` 在行上本来就是默认值，没有影响 |
| 新 scene(6) → 旧 go/db(5)（违反顺序，或 go/db 回滚） | body 顶层有未知的 `X`，C1 延后；告警；回到新 go/db 后处理。不丢数据 |
| 新 scene 只新增了 `y`（世代 6 没有新列）→ 旧 go/db(5) | 没有顶层未知字段；`y` 随 BLOB 原样存下；行戳 6，正常写 |
| 冷加载：行戳 6 → 旧 go/db(5) 读 | C2 延后，login 等待超时，玩家稍后重试；告警 |
| 冷加载：行戳 6 → 新 go/db(6) 读 → 旧 login 组装 | Go 保留 `X`、`y` 和戳；父快照戳 6 |
| 机制上线前的旧 scene（引导期） | 会丢字段、戳归 0；所以引导期禁止升世代（§8.1） |
| scene 回滚：全部 scene 从 6 回到 5 | 每名玩家由旧 scene 接手并嫁接；数据保持戳 6；之后再升回 6，不丢数据 |

---

## 8. 发布与回滚顺序

### 8.1 引导期：上线机制本身（第 0 期）

1. 台账写入世代 1 的基线。所有 repeated 字段补齐身份键或冻结声明，生成器检查通过。
2. **引导期内不得有任何世代 > 1 的改动**，由生成器强制：台账存在"引导中"标记时，拒绝追加世代。
3. 发布顺序：
   - DDL：两张表加戳列，默认 0；
   - go/db：所有 zone，C1、C2 和掩码随之上线，此时 `G = 1`，行为与现在相同；
   - login；
   - scene：嫁接、盖戳、C3 warn、C4。
4. **全量完成后**：确认全网不再有机制上线前的二进制。确认依据：版本清单，加上 `persist_schema_gen = 0` 的写入计数归零（scene 侧的 0 戳写入计数）。然后由负责人去掉"引导中"标记。
5. 观察一段时间 C3 的计数为 0，再把 `persist_schema_regression_mode` 切到 enforce。

### 8.2 此后每次升世代的发布顺序

`DDL（如果有新列）→ go/db 所有 zone → login → scene（可以按 zone 金丝雀）`

这个顺序依赖人工纪律，C1、C2 是兜底：顺序错了只会延后加告警，不会丢数据。部署脚本可以增加只读预检：比较目标 zone 的 go/db 世代与即将发布的 scene 世代。go/db 启动时会写能力心跳，可以在心跳里加上世代（仿照 `db:capability:zone:{z}`），列为第 4 批的可选项。

### 8.3 回滚规则

| 对象 | 能否回滚 | 说明 |
|---|---|---|
| scene / login | 可以回滚到任何带机制的版本 | 嫁接保证携带，Go 中间跳天然保留 |
| go/db | 只能回滚到世代 ≥"在用的最高列世代"的版本 | 回滚得更低时，涉及新列的写被 C1 延后，涉及新世代行的读被 C2 延后，并告警。处理办法：升回去，或者等这批任务。**不会丢数据，但会卡住存盘和冷加载**，runbook 要写清楚 |
| 回到机制上线前的二进制 | 禁止 | 那等于回到会丢字段的状态 |

### 8.4 告警与运维手册

| 告警 | 含义 | 处置 |
|---|---|---|
| `DbTaskSchemaTooNew` | go/db 世代低于数据 | 检查 go/db 版本，升回或继续滚完 |
| `PersistSchemaRegression` | 链路某处截断了数据 | 按 where 标签定位跳点；立即暂停该跳的发布 |
| `PersistSchemaUncarriable` | 生成期规则被绕过 | 阻断发布，修 proto |

运维手册写进 `deploy/k8s/README.md` 的对应小节，告警文件新建 `deploy/k8s/persist-schema-alerts.yaml`。

---

## 9. 分批实施（每批 < 30 个文件，AGENTS §10.2）

| 批 | 内容 | 主要文件 | Codex 验证 |
|---|---|---|---|
| 1 | 台账、生成器、option、身份键标注、生成常量（C++ 与 Go）、戳列 proto | `proto/db/proto_option.proto`、`proto/common/database/mysql_database_table.proto`、`bag_quest_mail_data.proto` 等标注、`proto/persist_schema_gen.txt`、`tools/proto_generator/protogen/internal/...`（新增 persist_schema 生成器及测试）、生成产物 | `proto-gen-build`、`proto-gen-run`（`-ConfigPath ...proto_gen.no-unity.yaml`）；protogen 的 `go test ./...`；之后完整编译 C++ 与所有 Go module（AGENTS §4.2） |
| 2 | go/db：C1、C2、掩码、行戳取 max、发布提交后整行、新延后原因、指标、告警；cmd/migrate 的 DDL | `go/db/internal/kafka/{player_row_fence,key_ordered_consumer,store_failure}.go`、`go/db/internal/metrics`、`go/db/etc/db.yaml`、`deploy/k8s/persist-schema-alerts.yaml`、测试 | go/db `go test ./...`、`go vet`；真库集成测试（自己起一次性 MySQL 或 TiDB，见本机真库回归的环境坑） |
| 3 | C++：`PlayerPersistBaseComp`、统一入口、嫁接、盖戳、C4、SnapshotSystem 改入口、严格屏障 Lua 增加第 8 个参数（warn/enforce）、配置项 | `services/scene/player/{comp,system}/...`、`snapshot_system.cpp`、`redis_client.h`、`base_deploy_config` 的 proto 与 yaml、对应 vcxproj 和 CMakeLists、测试 | `game.sln` 串行 `/m:1` 完整编译；新增单测、`currency_test`、`RedisSaveScriptsTouchOnlyDeclaredKeys` 等现有 Redis 脚本测试 |
| 4 | login 测试补齐、data_stress 盖戳、save_barrier_restore 前置检查、可选的能力心跳与部署预检、运维手册 | `go/login/...` 测试、`go/db/cmd/...`、`tools/scripts/k8s_deploy.ps1`（可选）、`deploy/k8s/README.md` | login、go/db 的 `go test`；k8s_deploy 契约测试 |

每一批完成后，在 `docs/PROGRESS.md` 追加条目，并注明 Java 版状态（§11）。

---

## 10. 测试计划

**C++ 单测（第 3 批）：**
- 测试专用的两版 proto（`cpp/tests/persist_compat_test/` 下的 `v1` 和 `v2` 包，消息名相同、字段集合不同），覆盖以下情况：
  - 顶层未知字段；
  - 单值子消息内的未知字段；
  - 有身份键的 repeated：配对、新增、删除；
  - map；
  - 组件已经保留过未知字段（不重复并入）；
  - 冻结元素内出现未知字段时 C4 拒载；
  - oneof 不在支持范围内（作为反例记录）。
- 用 v2 构造 PlayerAllData，在 v1 下执行"加载 → 存盘"往返，断言序列化字节里的未知字段齐全、两表戳为 `max`。
- `CurrencyDebtEntry` 的新字段在 `SaveToProto` 重建后被补回。
- 快路径：同一份数据存两次，第二次命中跳过。
- 严格屏障 Lua：`schema_gen` 倒退时，warn 模式照写并返回区分码；enforce 模式拒写；第 8 个参数非法时报错；旧的 7 参数调用被新脚本拒绝。

**Go 单测（第 2、4 批）：**
- 用 `dynamicpb` 构造 v2 body：顶层有未知字段 → 延后、不耗预算、计数正确。
- 行戳 > G 的读任务 → 延后。
- `s < G` 时 UPDATE 只出现掩码列（假库断言 SQL）。
- 行戳取 max。
- `s < r` 时计数。
- 发布内容是提交后的整行。
- login 冷加载组装和 backfill 往返保留未知字段与戳。

**生成器测试（第 1 批）：**
- 新增字段 → 追加世代；
- 删除字段但写了 reserved → retired；
- 改类型、复用字段号、给 oneof 加成员、给无键元素加字段、缺少身份键声明 → 失败；
- 输出稳定可重复。

**真实环境（第 2、3 批之后，Codex 或负责人执行）：**
- 双版本二进制混跑：v1 和 v2 两套 scene 与 go/db，按 §7 矩阵逐行验证。
- 需要两套构建产物，具体口径另行给出。这一项不能用单测代替，在它跑完之前，不能宣称"混版本安全"。

---

## 11. Java 版对齐（AGENTS §12）

- **Java 版的形态** [C]：玩家状态是 `player_state` 表的单个 BLOB 列 `data`，内容是 `PlayerState` 的 PB（`game-player-store` 的 `PlayerStore.java` 用 `toByteArray` / `parseFrom` 读写，`PlayerMapper.java` 用 `INSERT … ON DUPLICATE KEY UPDATE data = VALUES(data)` 写入）。`player` 表的其他列由固定 SQL 写入。
- **适用性**：
  - 机制三（列掩码）**不适用**：BLOB 整体写入，`player` 表的列由编译期固定的 SQL 指定，旧版不会碰新列。
  - 机制一（保留未知字段）和机制二（世代戳与校验）**同样需要**：Java protobuf 解析时默认保留未知字段，但如果 Java scene 从运行时对象重建 PlayerState，就会出现同类丢失；`PlayerState` 里也没有世代戳。
- **状态**：**待做**。原因是 Java 版要按自己的存储形态设计（AGENTS §12：服务端内部按 Java 惯用方式实现），本批只出 C++/Go 版设计。实施第 1 批时，在 Java 仓库 `PARITY.md` 登记一行"混版本持久字段保护：mmorpg 设计 v0.1，Java 待做"。

---

## 12. 不在本次范围、但属于同类的丢数据点

| 问题 | 现状 [C] | 建议 |
|---|---|---|
| 旧配置表删除新技能 | `PlayerSkillSystem::SanitizeSkillList` 把表里查不到的技能直接删掉，下一次存盘写回（`player_skill.cpp`） | 改为"查不到就保留、不生效"，与背包、任务"查不到也保留"的口径统一。另起一批处理 |
| 背包类型越界 | `bag_marshal::Unmarshal` 丢弃 `bag_type` 越界的条目，并打 WARN | 同上 |
| 收件箱、资产通道的载荷新字段 | 旧版 scene 执行器忽略新字段，照常执行并记为 APPLIED 终态 | 给载荷加版本号，或者要求执行器遇到未知字段时 fail-closed。收件箱消费现在默认关闭（`consumer_enabled=false`），开启前必须修 |
| scene / gate 的 Kafka 控制面命令 | 遇到未知事件直接丢弃 | 新增事件类型时必须配发件箱或重投 |
| guild、mail、trade 等各自的库表 | 没有核对 | 按本文的不变量逐个服务评估 |
| 客户端契约兼容 | `published-contract-compatibility-20261017.md` | 已有独立方案 |

---

## 13. 风险与待负责人拍板

1. **"只写改动列"改为"只写写方认识的列"**（§6.1）。请确认。
2. **option 的落点**（§3.2）：推荐生成器动态读取扩展，避免外部模块升级版本；如果选择升级外部模块，需要负责人打 tag、推送。
3. **C3 的默认模式**：推荐先 warn、后 enforce（§5.2）。
4. **引导期"禁止升世代"的时长**：取决于全网换版的速度。期间所有给持久数据加字段的需求都要等。
5. **R1、R2 的严格度**：比如"给 oneof 加成员"这类改动从此不能直接做，需要走破坏性变更流程。这会影响以后的玩法开发习惯，需要负责人认可。
6. **残余风险**：
   - 跨容器移动的元素会丢失元素级新字段（§4.3），靠作者规则约束；
   - 新字段与旧字段语义耦合时，旧写方的修改可能和新字段的含义不一致，靠字段作者自律，本设计无法机械检查；
   - 回档实现时必须显式重置 `schema_gen`。
7. **评审清单**：凡是修改持久 proto 的 PR，都要检查：
   - 生成器是否追加了世代；
   - 新 repeated 字段有没有声明身份键；
   - 每件物品的新数据是否放在实例层；
   - 新字段在旧写方原样携带时，语义是否仍然成立。

---

## 14. 证据索引

- C++ 存盘：
  - `cpp/libs/services/scene/player/system/player_lifecycle.cpp`：`MarshalPlayerForSave`、`SavePlayerToRedisImpl`、`OnDbTaskDispatchSettled`；
  - `player_data_loader.h`：生成产物，模板是 `tools/proto_generator/protogen/internal/template/player_data_loader.h.tmpl`；
  - `player_database_loader.cpp`、`player_database_1_loader.cpp`；
  - `cpp/libs/modules/currency/comp/player_currency_comp.h`；
  - `mission_marshal.cpp`、`bag_marshal.{h,cpp}`、`player_skill.cpp`；
  - `cpp/libs/modules/snapshot/snapshot_system.cpp`；
  - `cpp/libs/services/scene/player/comp/last_persisted_snapshot_comp.h`；
  - `cpp/libs/engine/core/utils/proto/proto_dirty_compare.h`。
- Redis：`cpp/libs/engine/infra/storage/redis_client/redis_client.h` 的 `kSaveWithCommitBarrierLuaScript`、`RedisSaveCommitIntent`。
- go/db：
  - `go/db/internal/kafka/key_ordered_consumer.go`：`handleDBReadOp`、`publishDBReadResult`、`handleDBWriteOp`、`buildCacheKey`；
  - `player_row_fence.go`、`store_failure.go`（L-04）；
  - `go/db/internal/logic/pkg/proto_sql/db.go` 的 `assertSchemaUpToDate`；
  - `go/db/internal/saveoutbox/generation.go`（世代门禁先例）。
- proto2mysql：v0.2.0 与 `feat/conditional-save` 分支（`D:\luyuan\proto2mysql`），`proto2mysql.go` 的 `UpdateFieldsByPK`、`fieldsListSQL`，以及 `pbconv/convert.go` 的 `SerializeFieldValue`。
- login：
  - `go/login/internal/logic/pkg/dataloader/{ensure_player_all_data_async,parent_commit_barrier,new_player_seed}.go`；
  - `go/login/internal/logic/clientplayerlogin/{player_class_backfill,role_appearance_backfill}.go`。
- proto：
  - `proto/common/database/{player_cache,mysql_database_table,bag_quest_mail_data}.proto`；
  - `proto/db/proto_option.proto`；
  - `proto/message_id.txt`（台账先例）。
- Java：`D:\luyuan\xuanming-server-mmo\game-player-store\src\main\java\com\game\player\store\{PlayerStore,PlayerMapper,PlayerStateRow}.java`。
- 关联文档：
  - `docs/design/db-write-fencing-mysql.md`；
  - `db-task-delivery-and-retention.md`；
  - `db-task-store-unavailable-deferral.md`；
  - `ecs-component-access-rules.md`；
  - `bag-instance-layout-split.md`；
  - `data-overwrite-rollback-audit-20261018.md`；
  - `published-contract-compatibility-20261017.md`。
