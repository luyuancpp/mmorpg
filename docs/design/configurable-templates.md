# 战斗策略 / 活动 / 玩法模板可配置化（含玩家自定义模板）

> **状态**：设计稿 v0.1（2026-10-19，Claude）。**未落码、未编译、未测试**（AGENTS.md §10.1）。负责人评审通过后按 §10 分批实施，每批交给 Codex 验证。
> **来源**：5 路现状调研（配表管线 / 战斗 / 活动 / 玩法 / Java 版）→ 3 套整体方案（表驱动原子组合 / Lua 脚本 / 节点图 + 事件-条件-动作触发器）→ 2 个视角交叉评审（正确性·一致性·双版本；策划易用·运维·性能）。玩家自定义模板（§8）另做了 4 路调研和 2 视角评审。
> **证据口径**：[C] 表示对照 2026-10-19 工作区代码读过；[I] 表示推断。工作区同时有其他会话在改，行号会漂移，正文以函数名为主、行号只作提示。
> **相关**：[turn-based-battle-server.md](turn-based-battle-server.md)（回合引擎与确定性）、[pve-team-size-matching.md](pve-team-size-matching.md)（队列 key 语义）、[activity_maintenance_auto_shift.md](activity_maintenance_auto_shift.md)（顺延）、[player-features-ui.md](player-features-ui.md)（活动列表接口）、[ecs-component-access-rules.md](ecs-component-access-rules.md)、[mixed-version-persist-field-protection.md](mixed-version-persist-field-protection.md)（新旧版本混跑的持久字段）、[table-api-reference-zh.md](table-api-reference-zh.md)、[tip-code-axis.md](tip-code-axis.md)、`data/AGENTS.md`（配表 schema 规则）。

---

## 0. 结论速览

1. **三个领域统一分三层**：

   | 层 | 谁负责 | 是什么 | 例子 |
   |---|---|---|---|
   | 原子（积木） | 程序，代码里注册的有限集合 | 条件、目标选择、行动、事件源、规则参数 | "自身血量低于 X""血最少的敌人""放技能 S""杀怪事件" |
   | 模板（规则） | 策划，填表 | 原子怎么组合：优先级规则、阶段、计数器、档位 | "三阶段 Boss 打法""累计击杀活动""3v3 竞技规则" |
   | 实例 | 策划 / 运营，填一行；或**玩家在策划给定范围内自建**（§8） | 模板 + 参数 + 时间 | "山魈王用打法 900""国庆这一期""我的好友 3v3 房规则" |

   纪律只有一条：**程序决定"能做什么"，策划决定"怎么组合"，玩家只能在策划画的圈里选值**。发奖、扣钱、计分这类资产操作只能是代码里的原子，配置与玩家模板只能引用，不能实现。
2. **技术路线：表驱动原子组合**。不引入 Lua 或其他通用脚本；不用现有 exprtk 表达式列承载策略；v1 不做行为树 / 节点图。理由见 §2。
3. **战斗策略**：`BattleAtomKind` 原子字典 + `BattleStrategy` / `BattleStrategyPhase` / `BattleStrategyRule`，一条规则一行"条件1 且 条件2 → 行动 + 目标"。接缝只有 `TurnBattleEngine::FillDefaultActions` 一处（§4）。
4. **活动**：模板 / 实例 / 奖励档位三层；阶段由 (实例起点, 偏移, now) 纯函数推导；触发器是扁平行"事件 + 过滤 → 加计数器"；奖励只走"档位 + 领取"（§5）。
5. **玩法**：`PlayMode` + `BattleRuleSet` 纯参数，替掉引擎里 `IsPveMatch()` 一个布尔撑起来的一堆规则；battle 按 `CreateBattleRequest` 里本来就有的 `(match_mode, battle_config_id)` 自查表，不加新 wire 字段（§6）。
6. **玩家自定义模板**（类网易狼人杀自定义板子）：可以做，但只开放"对局规则"和"自动战斗策略"两类，**活动与奖励类绝不开放**；自定义对局一律不发奖、不计分（§8）。
7. **兼容纪律**：所有新列的 0 值 = 现行行为；v1 不新增任何随机数抽取点；存量同种子回放逐字节不变。
8. **先还债再加功能**：战斗配表指纹默认只告警、跨语言战斗金样没建、Java 已落后 C++ 三处战斗修正——这三件不先处理，可配置化只会把两版越拉越远（§10 第 1 步）。

---

## 1. 现状

### 1.1 战斗

- [C] 回合战斗跑在 battle 节点的纯逻辑确定性引擎 `TurnBattleEngine`：输入快照 + 指令 + 种子，输出事件流；随机只走种子 RNG（`mt19937_64`），**消费顺序就是语义**（`turn_battle_constants.h` 的 `RollPercent` 注释段）。Java 版 `game-battle-engine` 逐位照搬。
- [C] 非玩家单位的出手决策全部在 `TurnBattleEngine::FillDefaultActions`：怪物、超时 / 挂机玩家一律"普攻 + target 0"，结算时由引擎 RNG 随机挑存活敌人；只有宝宝（`BATTLE_ACTOR_TYPE_PET`）按快照顺序取第一个能放的技能，目标写死 `enemies.front()`。
- [C] 怪物不带技能（`AppendMonsterActor` 不设 `skill_table_ids`），`MonsterTable` 只有属性和掉落列。设计文档自述"怪物 AI 用技能未做（只普攻）"（turn-based-battle-server.md §15.4）。
- [C] 没有目标策略、技能优先级、阶段转换、Boss 机制。
- [C] 技能 = 固定列 + `damage` 表达式 + `effect[]` 挂 buff id；buff 按 `buff_type` 走手写 switch（`TurnBattleEngine` 的 buff 周期效果段、`buff.cpp`），回合侧只有 8 种类型生效。buff 类型编号至少有 4 份手抄副本（`buff_table.proto` 注释、`buff.h`、`turn_battle_constants.h`、Java `BattleConstants.java`）。
- [C] 散落常量：普攻基础伤害 10、PVP 伤害系数 0.3、逃跑概率、暴击 ×2、连击 / 反震 50%、基础命中 100、PVP 道具限 5 次、缺省回合上限 300（均在 `turn_battle_constants.h`）。
- [C] battle 节点所有房间共用一个 muduo loop 线程（`battle_room_manager.h` 注释），每回合结算是同步 CPU 段。单局规模：每队玩家 ≤5（`kMaxBattleTeamSize`）、怪物 ≤10（`kMaxScaledPveMonsterCount`），连宝宝约 20 个 actor。文档里没有每回合 CPU 预算（capacity-model-hok-scale.md 写 battle 池"未建模"）。
- [C] Lua 5.4 + sol2 在代码里只剩一行 `thread_local sol::state tlsLua` 的声明，没有调用方，也没有接进构建。

### 1.2 活动

- [C] **两套互不相通的模型**：
  - scene（C++）：活动 = `mission_type == 2` 的任务行 + `ActivitySchedule` 一个绝对时间窗 `[start, end)`（UTC 毫秒，不自动顺延）。进度存在 `player_database.mission_component`，完成位永久有效，没有日 / 周重置。活动 15/16/17 都未排期。
  - go/guild：`GuildActivity.type` 取 1/2/3，每个 type 一个专用 RPC 和事务；个人次数按游戏日（UTC+8 05:00）计，帮会进度按档期计。
- [C] `mission_type == 2` 魔法数散在 `player_activity_schedule.cpp`、`player_mission.cpp`、`player_feature_snapshot.cpp` 多处。
- [C] 条件事件源只有杀怪 / 升级 / 完成任务三类（`PlayerMissionSystem` 的 `HasProgressSource` 白名单）；限时条件（`valid_duration ≠ 0`）直接拒绝；奖励只有物品、没有货币。
- [C] 不可参与原因是写死的中文串，没用 tip 码；`PlayerActivityInfo` 的 name / description / icon_key 没有配表来源，始终为空。
- [C] 没有活动服务（`ActivityNodeService` 只是预留）、没有阶段流程、没有活动计分接线；C++ 侧没有游戏日实现（Go 有 `go/shared/gameday`，Java 已对齐）。

### 1.3 玩法

- [C] 没有统一的玩法模板，三套类型枚举各自 switch：scene_manager 的 `SceneType`（只决定节点池和销毁策略）、match 的 `MatchMode`（凑满人数、分队、评分）、帮会活动 type。
- [C] 引擎只靠 `IsPveMatch()` 一个布尔切换刷怪、能否逃跑、PVP 伤害系数 0.3、道具限 5 次。胜负判定 `UpdateOutcome` 对所有模式相同："全灭，或回合打满判进攻方负"——这条 PVE 规则漏到了 PVP，match 侧专门打了 `RatingDrawRoundCap` 补丁。
- [C] match 的人数口径写了两遍（`joinqueuelogic.go` 与 `matcher.go` 的 `requiredPlayers`）；PVE 人数档放在 yaml 的 `PveTeamSizeByConfigId`，没用 `DungeonTable.max_team_size`；C++ 手抄了 MatchMode 数值 4/5。
- [C] "副本"实际全部走 battle 节点回合制；实时场景副本没接线（`SceneInfoComp.dungeon_config_id` 生产环境恒为 0），`MirrorTable` 没有任何读取方。

### 1.4 配表管线的能力边界

- [C] 导表器是 `tools/data_table_exporter`（不是 `tools/proto_generator`，后者是 RPC 生成器）。权威 schema 是 `data/schema/<sheet>_table.proto` + `cfg_*` option。
- [C] 只支持五种字段形态：标量、定长数组、集合、map、一层子消息数组（`schema_proto.py` 的 `_infer_shape`）。**不支持 oneof、enum、单个或嵌套子消息，没有 JSON 列**——一列放不下"不同类型参数结构不同"的多态参数。`data/AGENTS.md` 说"proto 里可以直接写 enum"与实现不符（解析器会拒绝 enum 块），需另行勘误。
- [C] 导表期 fail-closed 校验：FK / GFK 结构和取值、主键重复、tip 引用、protobuf 类型与溢出。**没有**枚举取值、数值区间、条件外键（"type=3 时某列必填且是外键"）这几类；这些靠各服务运行期手写，口径还不统一（Go guild 坏表拒启，C++ `PlayerEquipSystem::ValidateTables` 只打 ERROR）。
- [C] 已知会静默丢数据的两处：repeated 标量列里的 0 / -1 / 空串会被当空值丢掉（`excel_reader.py`），下标随之错位；float / double 单元格解析失败时静默返回 None。
- [C] 子消息字段上写 `cfg_fk` 不生效（`_element_subfields` 只读 `cfg_col`）。`cfg_composite` 词表和模板都支持，但 0 张表在用，首次使用要单独验证。
- [C] **热更没落地**：C++ / Go / Java 都生成了 `ReloadTables`，生产代码没有任何调用方。C++ 是逐表替换、旧快照当场析构，单表失败直接 `LOG_FATAL`；Go 的 `ReloadTables` 用裸赋值替换包级全局变量。改表 = 重新导表 + 滚动重启。
- [C] 常量生成链路是现成的：`constants_name` 列（`cfg_owner = "constants_name"`，先例 `actoractionstate_table.proto`）→ `constants_gen.py` → C++ `cpp/generated/table/code/constants/*.h` → Java 经 ContractSync 得到 `TableConstants`。注意生成的只是平铺 constexpr，**没有 `kCount`**。
- [C] 每加一张新表，程序都要改 `cpp/generated/table/table.vcxproj`（逐文件显式列出，CMakeLists 由它生成）。所以"新表"是程序工作，只有"新行"才是策划自助。

### 1.5 双版本

- [C] 配置表数据与 schema 是两版共享的客户端契约（AGENTS.md §12）。Java 用 ContractSync 同步同一批 `.pb`、manifest 和 schema，访问代码由自己的 `TableSchemaReader` 生成，加载时重做 sha256 / FK / tip / 表达式校验，fail-closed，每局绑定开局时的配表快照。
- [C] 战斗跨语言金样没建（Java `PARITY.md` 记作"待 mmorpg"），两版已经漂移：C++ 的 PVE 人数档、装备属性（连击 / 反击 / 反震 / 抗异常 / hit_kind）、抗异常修正，Java 都待跟。
- [C] 表达式列：C++ 用 exprtk（`random()` 走全局 `rand()`，每次求值都重新编译，不检查编译失败），Java 手写子集解析器，Go 完全不支持。
- [C] 活动和玩法逻辑在本仓大量落在 Go（go/guild、go/match），所以"两版"实际是 C++、Go、Java 三种运行时。

---

## 2. 选型：为什么是表驱动原子组合

三套整体方案的评审打分（1–10，两个评审视角取均值，保留到 0.5）：

| 方案 | 战斗策略 | 活动 | 玩法 | 决定性问题 |
|---|---|---|---|---|
| A 表驱动原子组合 | **7.5** | 6 | **7** | 加新原子要写代码（且两版各写一份）；这是本方案的边界 |
| B Lua 脚本做模板 | 3.5 | 4 | 5.5 | 两版解释器语义不一致（C++ PUC 5.4 有整数子类型，Java 只有 5.2 语义的 LuaJ）；共享 `lua_State` 的 GC 受同 loop 其他房间影响，破坏逐字节回放；Lua 未接进构建；LuaJ 大概率达不到 Java 版"≥2 万 star"选型规则 |
| C 节点图 + 事件-条件-动作 | 6 | 6.5 | 6.5 | xlsx 里拼树难读难 diff，可视化编辑器要到很后期；示例里漏算了随机数消耗点 |

采用 A 的主干，嫁接 C 的三样东西：**原子字典带参数规格列**（导表期通用校验）、**DecisionTrace**（线上排障）、**每局绑定编译快照**。活动的数据模型取 C 的"模板 / 实例 / 档位"三层，但触发器做减法（扁平行，不要条件图 / 动作图）。

不选 B 的其他理由：活动的事实源只有三类，脚本能多出来的表达力很有限，却把玩家资产路径交给两套解释器；脚本实际是程序在写，不是策划。

不选行为树的理由：回合制每回合只做一次瞬时决策，用不到行为树的 Running 状态和黑板；优先级规则表最容易读、最容易 diff。

**重新评估的条件**：原子数量真的失控（同类原子 > 30 且仍频繁新增），并且跨语言战斗金样已经建好、两版在同一版本上逐字节一致。

**原子准入纪律**：每个新原子至少要有 2 个真实配置实例在用才允许加，防止字典表慢慢演变成一门写得很别扭的脚本语言。

---

## 3. 共用底座

### 3.1 原子字典表

每个领域一张字典表：`BattleAtomKind`、`ActivityEventKind`、`PlayModeKind`。字典是编号的唯一来源，`constants_name` 列生成三端具名常量，替掉"枚举只写在注释里、四份手抄"的做法。凡是引用原子的列一律 `cfg_fk` 到字典表，"种类不存在"在导表期 fail-closed 拦下（FK 校验把 0 / -1 / 空视为未填）。

字典表直接带**参数规格列**，由导表器的通用校验器按规格校验，不为每个 kind 单写 Python：

```proto
// 形状示意：每个领域的字典表都带这几列
uint32 category  = 2;   // 闭集：条件 / 目标 / 行动 / ……，校验"这一列只能填哪类原子"
int64  p1_min = 3; int64 p1_max = 4;
int64  p2_min = 5; int64 p2_max = 6;   // min = max = 0 表示不用这个参数，填了非 0 即报错
string ref_table = 7;   // 需要引用的表（"Skill" / "Buff" / 空），导表器据此做条件外键
bool   consumes_rng = 8;// 会消费引擎 RNG（只对战斗字典有意义）
int32  constants_name = 9001 [(cfg_owner) = "constants_name"];
int32  design_note    = 9002 [(cfg_owner) = "designer"];
```

### 3.2 参数列口径（硬约束）

多态参数一律写成"**类型列 + 具名标量参数列 + 单独的引用列**"：`xxx_kind` + `xxx_p1` / `xxx_p2`（int64，统一用整数或万分比）+ `skill_id` / `buff_id` 这类具名 FK 列。组合关系用主表 + 子表表达：子表自带 id，`parent_id` 同时标 `cfg_fk` 与 `cfg_index`（先例 `attributedimension_table.proto`），顺序放在显式的 `order` / `priority` 列（§11.6：集合下标不得承载业务语义）。

禁止的写法：

| 写法 | 原因 |
|---|---|
| repeated 槽位存参数 | 0 / -1 / 空串会被丢弃，下标错位，而且无报错 |
| double / float 参数列 | 解析失败静默丢格；跨语言浮点 1 ulp 风险 |
| exprtk 表达式列 | `random()` 走 `rand()` 绕过种子 RNG；每次求值重编译；Go 不支持 |
| STRUCT_LIST 子字段上挂 FK | 子字段上的 `cfg_fk` 不生效；半空槽会被并进上一槽 |
| `map<string,string>` 参数包 | 展开成键值字符串列，FK 校验不了，两套校验器（Python 导表器 + Java `TableSchemaReader`）都要新写 |

### 3.3 代码侧分发

每个领域一张**平铺的处理器常量表**，每行 `{生成的 kind 常量, category, Validate, Eval/Apply}`，用 constexpr 做唯一性检查（§11.2：普通枚举 + 平铺常量表，不用 X-macro，不建继承树；参考 `guid_segment_registry`）。

因为生成的常量没有 `kCount`，"字典行 ↔ 处理器行一一对应"**不能**用 `static_assert` 守住，只能在启动期核对：字典删了一行，生成常量随之消失，C++ / Java 编译期报错；字典多了一行而没写处理器，启动期拒绝启动。

### 3.4 三层校验

1. **导表期**（主力）：现有 FK / PK / tip fail-closed，再在 `orchestrator.py` 的 `run()` 校验槽里挂一个**按字典规格的通用校验器**：列角色（这一列只能填哪个 category）、参数区间、条件外键、不用的参数必须为 0。失败则产物不落盘，策划在导表那一步就能看到哪张表哪行哪列错了。Java `TableSchemaReader` 同步实现同一套规则。
2. **启动期**（最后一道防线）：C++ 放在节点的 `TableLoadHandler`（scene / battle 的 `main.cpp`），Go 照 `go/guild/internal/activity/config.go` 的 `ValidateTables` 纯函数 + `logx.Must` 拒启，Java 放在各领域加载器。注意：battle 现在的 `TableLoadHandler` 只打 `LOG_ERROR`，改成拒启会把爆炸半径放大到整个 battle 池——所以错误必须尽量在导表期就拦下，启动期只兜底。
3. **运行期**：战斗靠 `CheckActionPrerequisites` 与 `ExecuteSkill` 出手前复校验、失败降级；活动照 guild `PickForWrite` 的写法，只接受当前处于开放状态的实例。

### 3.5 事件与计时

- 不新建事件总线。战斗引擎是纯逻辑库（零网络、零 ECS、零 timer、引擎内不打日志），条件在回合开始时轮询求值。
- scene 侧条件事实只走 `ConditionEvent` → `PlayerMissionSystem::OnCondition` 这一个入口；活动在它旁边**直接调用**，不走 dispatcher 订阅（ecs-component-access-rules：必需步骤直接调用，dispatcher 只做可有可无的通知）。
- 战斗里的冷却、持续、阶段一律按回合计，不起 timer。
- 活动的开放状态和阶段由纯函数 `StateOf(表行, now)` 惰性算出（先例 `PlayerActivityScheduleSystem::BuildInfo` 的纯计算重载、guild `rules.go` 的 `StateOf`），不为每个活动起定时器。`now` 每个请求只取一次，作为参数传入。
- 结算、封榜、全服发奖这类必须有人主动推进的阶段，由拥有者服务的巡检幂等推进（先例 guild 巡检器）。

### 3.6 版本、指纹与热更

- 模板在加载期**编译**成不可变的派生索引，运行时只持有 id 或 `shared_ptr<const 编译集>`，**不存表行指针**（data/AGENTS.md：C++ 热更时旧快照当场析构）。
- 每个战斗房间开局时绑定当时的编译快照，与 Java"每局绑定开局配表快照"对齐，为将来整批热更铺路。
- 战斗相关的新表一律**追加到** `BattleTableFingerprint` 的末尾（不能插中间），Java 按同样顺序追加；每张新表都会改变指纹，scene / match / battle 必须同批发布。
- **指纹默认只告警，不拦截**：match `TableFingerprintMode` 默认 `warn`（`config.go` 与 `match_service.yaml`），battle 默认 `FingerprintMode::Warn`（`battle_room_manager.cpp`）。"新表进指纹"在当前配置下保证不了表版本一致——必须先排计划切到 `enforce`（§10 第 1 步）。
- v1 不做热更。热更作为独立决策，前提三条：整批编译成功后才原子替换；每局绑定开局快照；启动日志打出 `manifest.json` 的 version / digest（现在只有 Java 消费 manifest）。

### 3.7 灰度与止血

- 所有新列的 0 值 = 现行行为：`Monster.strategy_id = 0`、`PlayMode` 无匹配行 = 现行规则、`ActivityInstance.enabled = false`。新旧版本并存窗口内逐位兼容，存量回放基线不平移。
- 新实例靠 `enabled` + 排期窗逐行打开。v1 没有按 zone 或按玩家比例的灰度（活动表没有 zone 维度）。
- battle 节点加一个**节点级开关**（环境变量，先例 `BATTLE_TABLE_FINGERPRINT_MODE` 覆盖），强制所有怪物按 `strategy_id = 0` 处理，用来止血。它和改表一样要重启，但不用重新导表。
- Go 的 `killswitch` 只能当 RPC 止血阀；它出故障时 fail-open，**不能**拿来判定活动是否开放或谁有发奖资格。

### 3.8 可观测性

- battle 和 scene 两个 C++ 进程**没有 Prometheus 端点**（`battle_room_manager.cpp` 注释："由日志侧提取计数"）。每次决策打一行日志会随"房间数 × 怪物数 × 回合数"爆炸。
- 做法：进程内按低基数维度聚合计数（`atom_kind`、`outcome`、`activity_event_kind`、`result`），每 60 秒打一行 `metric=battle_ai_decisions ...` 结构化日志，写法照 `battle_settlement_pending_backlog`。label 不带 `player_id`、`guild_id`、`strategy_id`、实例 id（AGENTS.md §9）。
- 战斗求值器返回 **DecisionTrace**（走过的阶段 / 规则 id 序列），引擎本身不打日志；battle 节点按 debug 开关输出，用于回答"Boss 为什么放了这招"。
- 导表时额外产出一份**可读的规则渲染报告**（如"策略 900 / 阶段 2 / 规则 10：若存活敌人 ≥ 3 则施放 横扫(5002)"），供评审和 diff。

---

## 4. 战斗策略模板

### 4.1 模型

- **原子**（代码注册）：条件、目标选择、行动三类；v1 不做效果原子，阶段效果复用 buff。
- **模板 = 实例**：`BattleStrategy` 一行，下挂 `BattleStrategyPhase`（阶段，只进不退）和 `BattleStrategyRule`（规则，priority 小的先判）。
- **挂载**：`MonsterTable` 新增 `strategy_id`，0 = 现行写死行为（普攻 + target 0 + 结算期 RNG 选目标）。一份策略可以挂给多种怪。
- **分层**（借 §11.6 的思路）：Monster 表回答"它是什么"（属性、掉落），策略表回答"它怎么出手"。怪物的技能集**不另开** `Monster.skill` 列，由策略引用的 `skill_id` 派生，避免同一件事两份来源。
- **复用同一参数**：规则的阈值参数允许引用策略实例上的具名参数槽（如 `BattleStrategy.param1..4`），"同一套打法只改一个阈值"不必复制整份策略。

### 4.2 表结构（示意，字段号以落码时为准）

```proto
// data/schema/battleatomkind_table.proto —— 战斗原子编号的唯一来源
message BattleAtomKindTable {
  option (cfg_sheet) = "BattleAtomKind"; option (cfg_source_file) = "BattleAtomKind.xlsx"; option (cfg_primary_key) = "id";
  uint32 id = 1;              // 原子编号，上线后不复用
  uint32 category = 2;        // 1 条件 2 目标 3 行动（闭集）
  int64  p1_min = 3; int64 p1_max = 4;
  int64  p2_min = 5; int64 p2_max = 6;   // min = max = 0 表示不用
  string ref_table = 7;       // "Skill" / "Buff" / 空
  bool   consumes_rng = 8;    // v1 只有 RandomEnemy 为 true，且只许配普攻
  bool   player_selectable = 9; // 是否允许玩家自定义策略选用（§8.4），默认 false
  int32  constants_name = 9001 [(cfg_owner) = "constants_name"];
  int32  design_note    = 9002 [(cfg_owner) = "designer"];
}

// data/schema/battlestrategy_table.proto —— 策略主表
message BattleStrategyTable {
  uint32 id = 1;
  uint32 fallback_target_kind = 2 [(cfg_fk) = "BattleAtomKind"]; // 全部规则落空时普攻的目标；0 = 旧行为
  int64  param1 = 3; int64 param2 = 4; int64 param3 = 5; int64 param4 = 6; // 规则可引用的具名阈值
  int32  design_note = 9001 [(cfg_owner) = "designer"];
}

// data/schema/battlestrategyphase_table.proto —— 阶段子表
message BattleStrategyPhaseTable {
  uint32 id = 1;
  uint32 strategy_id = 2 [(cfg_fk) = "BattleStrategy", (cfg_index) = true];
  uint32 phase_order = 3;                                // 同策略内 1..N 连续，显式
  uint32 enter_kind  = 4 [(cfg_fk) = "BattleAtomKind"];  // 进入条件（category = 条件）；第 1 阶段必须 0
  int64  enter_p1 = 5; int64 enter_p2 = 6;
  uint32 enter_buff_id = 7 [(cfg_fk) = "Buff"];         // 进入时挂给自己；0 = 不挂
}

// data/schema/battlestrategyrule_table.proto —— 规则子表（条件 → 行动）
message BattleStrategyRuleTable {
  uint32 id = 1;
  uint32 phase_id = 2 [(cfg_fk) = "BattleStrategyPhase", (cfg_index) = true];
  uint32 priority = 3;                                   // 阶段内唯一，小者先判
  uint32 cond1_kind = 4 [(cfg_fk) = "BattleAtomKind"]; int64 cond1_p1 = 5; int64 cond1_p2 = 6;
  uint32 cond2_kind = 7 [(cfg_fk) = "BattleAtomKind"]; int64 cond2_p1 = 8; int64 cond2_p2 = 9; // AND；0 = 恒真
  uint32 action_kind = 10 [(cfg_fk) = "BattleAtomKind"];  // Attack / CastSkill / Defend
  uint32 skill_id    = 11 [(cfg_fk) = "Skill"];           // 仅 CastSkill 必填，其余必须 0（导表期条件校验）
  uint32 target_kind = 12 [(cfg_fk) = "BattleAtomKind"]; int64 target_p1 = 13;
}

// monster_table.proto 纯追加（现有字段 1–10、9001）
uint32 strategy_id = 11 [(cfg_fk) = "BattleStrategy"];   // 0 = 现行普攻随机

// proto/battle/battle_data.proto —— 引擎内部状态，不进客户端可见的 BattleActorState
message BattleAiState { uint64 actor_id = 1; uint32 phase_order = 2; }
```

v1 原子集合（每个都要有 ≥2 个真实实例才加）：

| 类别 | 原子 | 参数 | 是否消耗 RNG |
|---|---|---|---|
| 条件 | `SelfHpBelowPermyriad` | p1 = 万分比 | 否 |
| 条件 | `RoundEveryN` | p1 = N，p2 = 起始回合 | 否 |
| 条件 | `AliveEnemyCountAtLeast` | p1 = 人数 | 否 |
| 目标 | `RandomEnemy` | — | 是（复用现有 target 0 + 结算期 RNG，**只许配普攻**） |
| 目标 | `LowestHpEnemy` / `HighestHpEnemy` | — | 否，同分按 (hp, actor_id) 升序 |
| 目标 | `FirstEnemy` / `Self` / `LowestHpAlly` | — | 否 |
| 行动 | `Attack` / `CastSkill` / `Defend` | — | 否 |

### 4.3 运行时

**C++（battle 节点，唯一接缝 `TurnBattleEngine::FillDefaultActions`）**：

1. actor 是 MONSTER 且 `strategy_id ≠ 0` 时：先推进阶段（只进不退，越过的每个阶段按 `phase_order` 依次挂 `enter_buff`），再按 priority 逐条求值。
2. 条件为真的规则构造 `BattleAction`，送进现有的 `CheckActionPrerequisites`；通过就采用，不通过看下一条；全部落空走 `fallback_target_kind` 或旧路径。
3. 出手时 `ExecuteSkill` 照常复校验，失败降级。
4. 新模块 `cpp/libs/services/battle/system/battle_strategy.{h,cpp}`：纯逻辑，不 include scene，不打日志。内容是平铺处理器常量表 + `CompiledBattleStrategy`（加载期编译出的只读索引，阶段和规则已排序，附派生技能集）。
5. `BattleDataProvider` 新增 `FindStrategy(id)`；生产实现在 `TableLoadHandler` 里编译一次，单测用 `MemoryBattleDataProvider` 喂内存数据。
6. `AppendMonsterActor` 在 `strategy_id ≠ 0` 时把派生技能集写进 `actor.skill_table_ids`，这样 `CheckActionPrerequisites` 的"技能必须在快照列表里"、冷却、沉默判定都直接复用。
7. 阶段状态存成 `BattleAiState` proto（turn_battle_engine.h：引擎状态由 proto 承载，不写平行 struct）。

**Java（game-battle-engine）**：在 `TurnBattleEngine.fillDefaultActions` 同一位置接入；`BattleData` 增加 `findStrategy`；`TableBattleData` 加载期编译并按同一口径 Validate；`BattleTableFingerprint` 按同序追加；原子编号用 `TableConstants`。

### 4.4 确定性硬约束

- **v1 不新增 RNG 抽取点**：规则按优先级确定性选取；目标同分按 (hp, actor_id) 升序；"随机敌人"只给普攻用，走现有 target 0 路径。`turn_battle_constants.h` 里记录的消费顺序不变。
- **v1 禁止任何作用于敌方的 buff 原子**：给敌方挂 buff 会走 `AddBuffToActor → RollAilmentResist`，目标有抗性时调用 `RollPercent` 消耗随机数；而抗异常是 Java 还没追平的 C++ 修正，两版会在同一配置下分叉。v1 阶段 buff 只许挂给自己（同队施加不触发抗异常掷骰）。等 Java 追平、并把消耗点登记进 Java battle-engine-spec §8.4 之后再开放。
- **阶段事件必须放在事件组清零之后**：`FillDefaultActions` 在 `ResolveCurrentRound` 里早于 `currentGroupId = 0` 执行。阶段推进若在里面发 buff 事件，会和第一个行动组撞号（都是 1）。要么调整清零顺序，要么把阶段推进移到清零之后。
- 同优先级权重随机是**新的 RNG 抽取点**：要在 `turn_battle_constants.h` 与 Java spec §8.4 登记位置并刷新基线，作为单独决策，不进 v1。

### 4.5 热路径上限

battle 所有房间共用一个 loop。上限：每策略阶段 ≤ 8、每阶段规则 ≤ 16、每规则条件 ≤ 2。单局约 20 个单位，每回合求值是万次量级的整数比较，不分配内存。导表期校验上限；Codex 跑满配基准后把结果写回本节。

### 4.6 示例：三阶段 Boss「山魈王」（Monster 9001，策略 900）

| 阶段 | 进入条件 | 进入时 | 规则（priority 小先判） |
|---|---|---|---|
| 1 | — | — | p10：`RoundEveryN(3,1)` → 放横扫（AOE，不耗蓝）；p20：→ 普攻 `LowestHpEnemy` |
| 2 | `SelfHpBelowPermyriad(6000)` | 给自己挂回血 buff（type 40，回合侧已生效） | p10：`AliveEnemyCountAtLeast(3)` → 横扫；p20：→ 毒刺打 `LowestHpEnemy` |
| 3 | `SelfHpBelowPermyriad(2500)` | — | p10：`RoundEveryN(2,0)` → 重击 `LowestHpEnemy`；p20：→ 普攻 `RandomEnemy` |

走一遍：开局 `AppendMonsterActor` 把 {横扫, 毒刺, 重击} 写进 `skill_table_ids`。第 1 回合 `RoundEveryN(3,1)` 为真，横扫过校验被采用。第 2 回合落到 p20，普攻血最少的玩家，同血取 actor_id 小者，不耗 RNG。HP 降到 55% 后的下一回合开始时推进到阶段 2，挂回血 buff，`BattleAiState.phase_order = 2`。毒刺冷却中就回落 fallback。若一回合内 HP 从 70% 掉到 20%，下回合依次挂上阶段 2、3 的进入 buff，直接进阶段 3。Boss 被眩晕时照旧跳过行动。

注意：Monster 表没有法力列，`CheckSkillCost` 会拦下有消耗的技能——策划必须给 Boss 配无消耗技能，或另行补怪物法力列。

### 4.7 待拍板

- 派生技能集写进 `skill_table_ids`，而它同时是"客户端可见的本局可用技能列表"——Boss 后期阶段的技能会在开局就暴露给客户端。接受，还是另开一个服务端内部列表？
- 宝宝（`Pet.strategy_id`）是否同批接入，替换"取第一个可用技能"。

---

## 5. 活动模板

### 5.1 模型

按"它是什么"和"它何时开"拆开（§11.6 的思路）：

- **模板** `ActivityTemplate`：规则本身——周期口径、准入、阶段列表、计数器、触发器。
- **实例** `ActivityInstance`：一期排期——模板 id、enabled、UTC 毫秒 `[start, end)`，口径沿用 `ActivitySchedule`。
- **档位** `ActivityRewardTier`：挂在实例上，"计数器达到 N 领 Reward X"。同一套规则换节日、换奖励、换门槛，不必复制模板；"再开一期"只填实例一行加档位。

范围：

- v1 只做**个人维度**，跑在 C++ scene 和 Java game-scene。
- 帮会活动（`GuildActivity.type` 1/2/3）保持现有 Go switch 不动，等出现第 4 种帮会活动再迁（YAGNI）。
- 现有"任务型活动"（`mission_type == 2` + `ActivitySchedule`）保留，与新活动并存，`GetActivityList` 合并输出两者；之后再把活动 15/16/17 迁成新模型并退掉魔法数。

### 5.2 表结构（示意）

```proto
// activityeventkind_table.proto —— 活动事件原子字典（映射到 ConditionEvent 的类别）
message ActivityEventKindTable {
  uint32 id = 1;                 // 杀怪 / 升级 / 完成任务 …… 只收录已有事实源的类别
  uint32 condition_category = 2; // 对应 Condition.condition_category
  int32  constants_name = 9001 [(cfg_owner) = "constants_name"];
}

// activitytemplate_table.proto
message ActivityTemplateTable {
  uint32 id = 1;
  uint32 period_kind = 2;        // 0 整档期 1 游戏日 2 游戏周（UTC+8 05:00 / 周一 05:00）
  uint32 entry_level_min = 3;    // v1 准入只做等级门槛（现行 CheckAccept 只支持等级）；0 = 无门槛
  int32  design_note = 9001 [(cfg_owner) = "designer"];
}
message ActivityPhaseTable {
  uint32 id = 1;
  uint32 template_id = 2 [(cfg_fk) = "ActivityTemplate", (cfg_index) = true];
  uint32 seq = 3;                // 显式顺序
  uint32 phase_kind = 4;         // 1 进行 2 领奖 3 展示（导表期区间校验）
  uint64 offset_ms = 5;          // 相对实例开始；本阶段在下一阶段起点或实例结束时结束；必须递增
}
message ActivityCounterTable {
  uint32 id = 1;                 // 存档按计数器 id 记，上线后不复用
  uint32 template_id = 2 [(cfg_fk) = "ActivityTemplate", (cfg_index) = true];
  uint64 cap = 3;
}
message ActivityTriggerTable {   // 扁平行：事件 + 过滤 → 加计数器；不要条件图 / 动作图
  uint32 id = 1;
  uint32 template_id = 2 [(cfg_fk) = "ActivityTemplate", (cfg_index) = true];
  uint32 active_phase_id = 3 [(cfg_fk) = "ActivityPhase"];  // 按阶段 id 引用
  uint32 event_kind = 4 [(cfg_fk) = "ActivityEventKind"];
  repeated uint32 event_filter = 5 [(cfg_slots) = 5];       // 各格 ANY；全空 = 不过滤；语义同 Condition.condition1
  uint32 counter_id = 6 [(cfg_fk) = "ActivityCounter"];
  uint32 amount_kind = 7;        // 1 取事件 amount 2 固定 amount_value
  uint64 amount_value = 8;
}
message ActivityInstanceTable {
  uint32 id = 1;
  uint32 template_id = 2 [(cfg_fk) = "ActivityTemplate"];
  bool   enabled = 3;
  uint64 baseline_start_at_ms = 4;   // UTC 毫秒，左闭右开，与 ActivitySchedule 同口径
  uint64 baseline_end_at_ms = 5;
  string name = 6; string desc = 7; string icon_key = 8;   // 填 PlayerActivityInfo 现在恒空的三字段
}
message ActivityRewardTierTable {
  uint32 id = 1;                 // 存档按档位 id 记已领，上线后不复用
  uint32 instance_id = 2 [(cfg_fk) = "ActivityInstance", (cfg_index) = true];
  uint32 counter_id = 3 [(cfg_fk) = "ActivityCounter"];
  uint64 threshold = 4;          // 导表期校验 ≤ 计数器 cap
  uint32 reward_id = 5 [(cfg_fk) = "Reward"];
  uint32 claim_until_phase_id = 6 [(cfg_fk) = "ActivityPhase"]; // 可领截止阶段
}
```

导表期额外校验：阶段 offset 递增、档位阈值 ≤ cap、同模板启用实例的时间窗不重叠、触发器引用的计数器属于同一模板。

### 5.3 进度存储（硬约束）

- **不要嵌进 `MissionListComp`**。[C] `mission_marshal.cpp` 的 `Unmarshal` 只逐项恢复 missions、mission_begin_time、completed、claimable，新字段每次加载都会丢；滚动升级时旧 scene 重新序列化 `mission_component` 也会抹掉它。再配上"缺周期键就当新周期清零"，玩家每次重登都能在同一游戏日里重做、重领——**这是资产复制漏洞**。
- 推荐：新持久结构做成 `player_database` 的**顶层新字段** `PlayerActivityComp`：

  ```proto
  message PlayerActivityComp { map<uint32, ActivityProgress> instances = 1; }   // key = instance_id
  message ActivityProgress {
    uint64 period_key = 1;                 // 当前进度所属周期（游戏日 / 周键，或整档期 = instance_id）
    map<uint32, uint64> counters = 2;      // key = counter_id
    map<uint32, uint64> claimed_at_ms = 3; // key = tier_id；值 = 领取成功的 UTC 毫秒
    map<uint32, uint64> carry_claimable = 4; // 换期时保留的"可领未领"档位：tier_id → 达成时的 period_key
  }
  ```

  [C] 玩家数据在 data_service 按字段存成独立 Redis 键 `player:{id}:<field>`，旧节点不认识新字段就不会覆盖它。[I] go/db 写 MySQL 时会不会清掉调用方没带的列未核实——依赖 [mixed-version-persist-field-protection.md](mixed-version-persist-field-protection.md) 的"go/db 不写写方不认识的列"落地，或在本功能上线前单独确认。
- 按 ECS 规则：`PlayerActivityComp` 属于持久数据，只在玩家加载时创建；缺了就拒绝服务（返回 `kServiceUnavailable`），不补建（同 `player_feature_snapshot.cpp` 的口径）。读路径（列表、快照）不改组件集合。

备选方案（更快、但改动面更大）：复用 Mission + `ClaimMissionReward(scope = 2)`，客户端协议不变。前提是先改掉 `player_mission.cpp` 里写死 scope 0 的 5 处——`CheckPlayer` 只放行 `kPlayerMission`、`ApplyFollowUps` 的自动领奖和链式接取、`Initialize` 只创建 scope 0 容器、`OnCondition` 只推进 scope 0——否则 scope 2 的自动领奖会去 scope 0 领、失败后只留一行 WARN；周期键仍不能放 `MissionListComp`。本文推荐独立结构，因为活动的"计数器 + 档位"与任务的"单目标 + 完成位"不是同一个模型。

### 5.4 阶段与周期

- `PhaseOf(instance, phases, nowMs)` 是纯函数，写法仿照 `PlayerActivityScheduleSystem::BuildInfo` 的纯计算重载；重启不漂移，将来加"按开服日锚定"只是多一种锚点（目前仓里没有开服时间这个事实源，需另补）。
- 周期键只取自游戏日 / 游戏周切点：把 `go/shared/gameday` 的 DayKey / WeekKey 移植成 C++ 纯函数，**三版（C++、Go、Java）共用一份对拍向量**。
- 换期只在写入口惰性执行（`period_key` 与当前不同时）；读路径按"已换期"计算展示，不改存档。
- 时间统一 UTC 毫秒、`[start, end)`；服务端是开放状态的唯一事实源，客户端只负责按 UTC+8 显示。
- 停服顺延按 [activity_maintenance_auto_shift.md](activity_maintenance_auto_shift.md)：基线不变、生效时间全量函数化重算，不在旧 end 上增量加。

### 5.5 领奖（硬约束）

1. 触发器**不直接发物**，只加计数器；奖励一律走"档位 + 领取"。
2. 领取在提交点重新核对：阶段未过 `claim_until_phase_id`、计数器达到阈值、该档未领、实例就是当前期（或在 `carry_claimable` 里）。
3. 核对通过后走 `BagService::AddItems`，新增一个 TX 类型；**发放成功才写 `claimed_at_ms`**，与"发放成功后才清领取权"同序。背包满回 tip 码，状态保持可领。
4. **换期不得吞掉未领奖励**：换期只清计数器；已达成未领的档位转入 `carry_claimable`，在其 `claim_until_phase_id` 截止前仍可领；截止后由巡检转邮件补发（与 guild 活动"奖励绝不跳过"口径一致）。邮件发附件前目标 zone 的 go/db 须已切 `Fencing.Mode=dual`（mail-system.md）。
5. 新增客户端 RPC `ClaimActivityReward`（客户端可见协议，两版同批改）；不可参与 / 领取失败原因在 `Tip.xlsx` 新开 `activity_error` 组发号，替掉写死的中文串。

### 5.6 示例：「国庆·七日讨伐」

- 模板 T100：`period_kind = 0`（整档期），`entry_level_min = 20`；阶段 P1「进行」offset 0，P2「领奖」offset 6 天；计数器 C1「讨伐数」cap 9999；触发器 TR1：`active_phase = P1`，`event_kind = 杀怪`，`event_filter = [9001, 9002]`，加 C1，取事件 amount。
- 实例 I2026：template T100，enabled，2026-10-01 05:00（UTC+8）起 7 天。
- 档位：C1 ≥ 50 → Reward 7001；≥ 150 → 7002；≥ 300 → 7003；`claim_until_phase = P2`。

走一遍：玩家在副本击杀 9001，battle 结算经 Kafka 回到 scene，`ApplySettlement` 构造 `ConditionEvent{杀怪, [9001], 1}`。`PlayerMissionSystem` 照旧处理；`PlayerActivitySystem` 按倒排索引找到 TR1，判断 `PhaseOf = P1`、过滤命中、等级 ≥ 20，C1 加 1、置脏、随下一次存盘落库。第 7 天进入 P2，触发器不再计数，档位仍可领。领 50 档：提交点复核 → `AddItems` 成功 → 写 `claimed_at_ms`。

### 5.7 不在本期范围

- **限时累充**：结构完全一样（换 `event_kind`），但本仓没有充值事实源（支付节点只是预留）。必须先由支付链路按订单号幂等地发 `ConditionEvent`，这属于玩家资产 fail-closed 路径，单独设计。
- **排行型活动**：接 go/rank，新增 `ActivityRankReward` 表替代 Freeze 请求里传入的档位，经 mail 发奖。
- 跨服活动、按 zone 排期、按开服日锚定、对话 / 使用物品 / 交互类事件源（事件源先要有真实写点）。

---

## 6. 玩法模板

### 6.1 模型

- **模板**：`PlayModeKind` 字典一行，对应代码注册的凑局处理器：`InstantSolo`（PVE 单人即开）、`QueueFifo`（按 队伍数 × 每队人数 先到先凑满）、`QueueRatedSnake`（按评分蛇形分队，由 5v5 写法推广到任意每队人数）、`Challenge`（点名切磋）、`GuildActivityBattle`（帮会历练）。
- **实例**：`PlayMode` 一行，把 match_mode、battle_config_id、人数区间、是否计分、规则集串起来。
- **规则集**：`BattleRuleSet` 纯参数，替掉 `IsPveMatch()` 的各个读点与散落常量。

### 6.2 表结构（示意）

```proto
// playmodekind_table.proto —— 凑局处理器字典
message PlayModeKindTable {
  uint32 id = 1;
  uint32 team_count = 2;   // 处理器要求的阵营数：1 = PVE，2 = 对战（启动期与实例比对）
  int32  constants_name = 9001 [(cfg_owner) = "constants_name"];
}

// playmode_table.proto —— 玩法实例；(match_mode, battle_config_id) 唯一（启动期校验，或首次验证 cfg_composite 后改为复合键）
message PlayModeTable {
  uint32 id = 1;
  uint32 kind = 2 [(cfg_fk) = "PlayModeKind"];
  uint32 match_mode = 3;             // proto MatchMode 数值（客户端契约）
  uint32 battle_config_id = 4 [(cfg_fk) = "Dungeon"];
  uint32 team_size_min = 5; uint32 team_size_max = 6;   // ≤ 5（D14）；替代 yaml PveTeamSizeByConfigId
  bool   rated = 7;                  // 替代 isRatedMode
  uint32 rule_set_id = 8 [(cfg_fk) = "BattleRuleSet"];
  bool   customizable = 9;           // 是否允许玩家自定义规则覆盖（§8.3），默认 false
}

// battleruleset_table.proto —— 引擎规则参数（进战斗配表指纹）
message BattleRuleSetTable {
  uint32 id = 1;
  bool   spawn_monsters = 2;               // 现 IsPveMatch 的刷怪分支
  bool   flee_allowed = 3;
  uint32 damage_scale_permyriad = 4;       // 10000 = 1.0；3000 = 现 PVP 0.3
  uint32 item_use_cap = 5;                 // 0 = 不限（现 PVE）；5 = 现 PVP
  uint32 max_rounds = 6;                   // 0 = 沿用 Dungeon.time_limit 换算（现行）
  uint32 round_cap_outcome = 7;            // 1 进攻方负（现行） 2 平局
  bool   scale_monsters_by_players = 8;    // 现 PVE 人数档
  uint32 deadline_seconds = 9;             // 0 = 沿用 match 全局 BattleMaxDurationSeconds
}
```

**伤害系数用整数万分比**：先算 `3000 / 10000.0` 再乘，与字面量 `0.3` 是同一个 double，确定性不变。

### 6.3 查表与下发（硬约束）

- **battle 自己查表**：`CreateBattleRequest` 已经带了 `battle_config_id`（字段 2）和 `match_mode`（字段 5）。battle 按这两个值在自己加载的 `PlayMode` 表里查规则集，查不到就按 match_mode 推导出与现行逐位相同的内置规则。
- **不加 `play_mode_id` 这样的新 wire 字段**：按 proto3 规则，旧 battle 会忽略未知字段，用旧规则开局，新玩法（如"30 回合打满判平"）会**静默失效**。
- **不把规则挂在 Dungeon 行上**（`Dungeon.play_rule_id`）：那会把"这是哪个副本"和"这局怎么打"写进同一行，同一副本想做普通 / 挑战两个变体只能复制 Dungeon 行（违反 §11.6 的分层思路）。
- 滚动升级：新 `PlayMode` 行只在 battle 全量升级后才启用；指纹切到 enforce 后，表批次不一致会被拦截。

### 6.4 match 侧

- match 现在**不加载任何配表**（`LoadTables` 只出现在 guild、login、player_locator、scene_manager）。接表照 `go/guild/internal/svc/servicecontext.go` 的写法，k8s 也要同步部署表数据目录。
- 启动期建 `(match_mode, battle_config_id) → PlayMode` 映射并 fail-closed 校验（`ValidateTables` + `logx.Must`），启动日志打出 manifest version / digest。
- 用 `PlayMode.kind` 处理器替掉 `joinqueuelogic.go`、`matcher.go`、`gather.go`、`rating.go` 里的 MatchMode switch；队列 key 维持 `(mode, config)`，语义一变就要整批替换实例（pve-team-size-matching.md）。
- 删 yaml 的 `PveTeamSizeByConfigId`（它同时被 team 的 `StartTeamMatch` 整队开战使用，k8s ConfigMap 也是逐字搬运，要一起改）；`round_cap_outcome` 上线后删 `RatingDrawRoundCap` 补丁。
- guild 历练结算写死要求 `match_mode == PVE_TEAM`，改为校验 PlayMode。
- **不加 `PlayMode.activity_id`**：那等于要求 go/match 再实现一遍活动状态机（第三、第四份实现）。限时开放先由 match 的 yaml 配置，或等活动服务落地后再接。

### 6.5 胜负规则

v1 只用 `round_cap_outcome` 枚举。出现"击杀指定单位即胜"这类真实需求时再加 `win_on_monster_id` 列；出现第三种需求时再考虑条件树。胜负判定**不得消耗随机数**，`RollDrops` 之前的随机序列保持不变。

### 6.6 示例：周末 3v3 回合竞技

- `BattleRuleSet 21`：不刷怪、不可逃跑、伤害 ×0.3（3000）、道具 5 次、30 回合、打满判平。
- `PlayMode 301`：kind = `QueueRatedSnake`，match_mode = 3V3，battle_config_id = 竞技场副本行，每队 3..3，rated，rule_set 21。
- 走一遍：客户端 `JoinQueue(3V3, X)`（协议不变）→ match 查到 PlayMode 301 → 凑满 2 × 3 = 6 人、按评分蛇形分队 → `CreateBattleRequest` 照旧只带 match_mode + battle_config_id → battle 自查到规则集 21 → 结算后 match-rating 计 Elo，不再需要 `RatingDrawRoundCap`。
- 前提：v1 框架已把 5v5 的蛇形分队推广成按每队人数参数化（3V3 现在在 `JoinQueue` 走 default 被拒）。

### 6.7 不在本期范围：实时场景玩法

夺旗、占点、限时刷怪这类实时玩法需要先补三样基础：场景玩法组件在 `SceneLifecycle::CreateScene` 唯一创建点 emplace；`GameplaySystem` 按 view 遍历挂进 `World::Update`，单帧不超过 2ms（scene-node-threading-model.md）；进场 / 离场钩子直接调用，不走 dispatcher。另立里程碑。

---

## 7. 硬约束汇总

| # | 约束 | 违反后果 | 出处 |
|---|---|---|---|
| H1 | 所有新列 0 值 = 现行行为，存量同种子回放逐字节不变 | 回放基线平移，两版对拍失效 | turn_battle_engine.h 确定性约束 |
| H2 | v1 不新增 RNG 抽取点；同分按 (hp, actor_id)；`RandomEnemy` 只配普攻 | 随机序列平移，两版分叉 | turn_battle_constants.h 消费顺序 |
| H3 | v1 禁止对敌施 buff 的原子 | `RollAilmentResist` 掷骰 + Java 未追平 → 两版分叉 | `AddBuffToActor` |
| H4 | 阶段事件在事件组清零之后发 | group_id 撞号 | `ResolveCurrentRound` |
| H5 | AI 运行态只放服务端内部 `BattleAiState`，不进 `BattleActorState` | 改客户端契约，泄漏 Boss 阶段 | player_battle.proto |
| H6 | 战斗新表追加到指纹末尾，指纹切 enforce | 表版本不一致照常开局 | battle_table_fingerprint、match config |
| H7 | 活动周期状态不进 `MissionListComp`，用顶层新字段 | 重登丢周期键 → 重复领奖 | mission_marshal `Unmarshal` |
| H8 | 换期只清进度，不吞未领奖励 | 玩家资产损失 | mission.cpp"发奖成功前保留领取权" |
| H9 | 触发器不直接发物；领奖提交点复核，发放成功才置已领 | 双发 / 漏发 | player_mission 领奖顺序 |
| H10 | 玩法规则由 battle 按已有字段自查，不加新 wire 字段 | 旧 battle 静默跑旧规则 | proto3 未知字段语义 |
| H11 | 参数列用具名标量 int64，不用 repeated 槽位 / double / 表达式 | 静默丢数据、确定性破坏 | excel_reader.py、table_expression.h |
| H12 | 不存表行指针，只存 id 或编译快照 | 热更后野指针 | data/AGENTS.md |
| H13 | 指标进程内聚合、周期打日志，label 不带高基数 id | 日志爆炸 / Prometheus 高基数 | battle 无 Prometheus 端点；AGENTS §9 |
| H14 | 处理器表与字典一一对应在启动期核对（生成常量无 kCount，不能 static_assert） | 漏注册的原子运行期才暴露 | constants_gen.py |
| H15 | tip 码只由导表器从 `Tip.xlsx` 发号；不可参与原因不写中文串 | 客户端文案错乱 | AGENTS §4.4、§7.5 |
| H16 | 玩家自定义模板只能在策划声明的范围内收紧 / 调整，自定义对局不发奖、不计分 | 刷经验 / 掉落 / 评分 | §8 |
| H17 | 两版同批：客户端可见 proto、tip、配表与 Java 同批改并在 Java `PARITY.md` 登记 | 两版契约分叉 | AGENTS §12 |

---

## 8. 玩家自定义模板

（本节由玩家自定义模板的专项调研与评审结果填写，见下文。）

---

## 9. 双版本分工

| 领域 | C++ / Go 版 | Java 版 |
|---|---|---|
| 配表与导表器 | Python 导表器加通用规格校验器、时间字符串列 | `TableSchemaReader` 同步实现同一套规格校验 |
| 战斗策略 | battle 节点 `battle_strategy` 模块、`FillDefaultActions` 接入、指纹追加 | game-battle-engine `fillDefaultActions` 同位置接入、`BattleTableFingerprint` 同序追加 |
| 玩法 | go/match 接表与处理器；C++ battle 规则集读点 | game-match 队列 / gather / 评分；game-battle-engine 规则读点；game-guild 历练校验 |
| 活动 | C++ scene `PlayerActivitySystem`、gameday 移植、`ClaimActivityReward` | game-scene 对应解释器、存储同名字段、gameday 对拍向量 |
| 玩家自定义模板 | 见 §8 | 见 §8 |

每完成一步，在 Java 仓 `PARITY.md` 登记一行，本仓 `docs/PROGRESS.md` 条目注明对应 Java 版本号。

---

## 10. 落地顺序

每一步都交给 Codex 编译、测试；Claude 不执行构建与测试（AGENTS.md §10.1）。

**第 0 步：文档**（本文）。另行勘误 `data/AGENTS.md` 里"proto 里可以直接写 enum"的说法。

**第 1 步：还债，不写新功能**

1. 指纹从 `warn` 切 `enforce` 的灰度方案：match `TableFingerprintMode`、battle `battle_table_fingerprint_mode` / `BATTLE_TABLE_FINGERPRINT_MODE`。先在 dev 环境 enforce，再按 zone 逐步切。
2. 建跨语言战斗金样：在本仓导出同种子用例的事件 hex 作为夹具，Java 回放比对（Java battle-engine-spec §13.9 的 G1–G14 场景）。**需用户同意后再做**。
3. Java 追平 C++ 已漂移的三处：抗异常修正、PVE 人数档、装备属性（Java `PARITY.md` 已登记待跟）。

**第 2 步：导表器能力**

- `orchestrator.py` 挂按字典规格的通用校验器（列角色、区间、条件外键、未用参数为 0）；Java 同步。
- 新增时间字符串列：cfg option 声明"UTC+8 墙钟串 → UTC 毫秒"，替代手填毫秒戳（填错一位就差好几天，而导表期只能查 start < end）。
- 导表时输出规则渲染报告。

**第 3 步：战斗策略 MVP（单阶段）**

- 表：`BattleAtomKind`、`BattleStrategy`、`BattleStrategyRule`（Phase 表先只允许一行阶段，或第 4 步再建）。
- 原子：条件 3 个、目标 4 个、行动 2 个（§4.2）。
- 改动点：`Monster.strategy_id`、`FillDefaultActions` 接入、`AppendMonsterActor` 派生技能集、`BattleDataProvider.FindStrategy`、DecisionTrace、聚合指标、启动期校验、指纹追加；Java 同批。
- 验收：① `strategy_id = 0` 时现有全部同种子用例逐字节不变；② 每个原子有用例，坏表在导表期失败；③ C++ 导出新用例的事件 hex，Java 回放一致。

**第 4 步：阶段状态机**：`BattleStrategyPhase` + `BattleAiState`；阶段只在回合开始推进，事件放在组清零之后（H4）。可选：宝宝接入。

**第 5 步：玩法 v1**：`PlayModeKind`、`PlayMode`、`BattleRuleSet`；match 接表；`IsPveMatch` 读点改读规则集；删 `PveTeamSizeByConfigId` 与 `RatingDrawRoundCap`；3V3 处理器。前提：第 1 步第 3 项完成，否则规则集在两版起点不同。

**第 6 步：活动 v1（个人维度）**：五张活动表 + `ActivityEventKind`；`PlayerActivityComp` 顶层字段与 DB 列；gameday 移植与三版对拍；`ClaimActivityReward` 与 `activity_error` tip 组；`GetActivityList` 合并输出。同批把**活动实例表做成第一张可热更的叶子表**：在 scene loop 线程上整表替换、只按 id 读、解析失败回退旧快照而不是 `LOG_FATAL`，这样改排期不用重启 scene。

**第 7 步：玩家自定义模板**：见 §8 的分期。

**之后各自独立决策**：配表整批原子热更；`buff_type` switch 换成效果原子字典并合并四份 buff 枚举副本；帮会活动迁入模板；排行型活动；实时场景玩法框架。

---

## 11. 待拍板

1. 是否接受 Boss 全部技能在开局即对客户端可见（§4.7）。
2. 活动进度存储：独立 `PlayerActivityComp` + 新 RPC（推荐），还是复用 Mission scope 2 以免改客户端协议（§5.3）。
3. 是否同意第 1 步建跨语言战斗金样（需用户同意）。
4. 指纹切 enforce 的时间表。
5. 玩家自定义模板的开放范围与第一期（§8）。

## 12. 未核实项

- LuaJ 的 GitHub star 数与指令计数 hook 的实际行为（不选 B，未深究）。
- go/db 写盘时是否会清掉调用方没带的列（§5.3 依赖 mixed-version-persist-field-protection 落地或单独确认）。
- 回档的帮会闸只检查 guild；活动奖励若走 mail 或 rank 跨存储发放，回档时是否会重复，未核实。
- 每回合 CPU 开销与 battle 节点容量没有基准，需 Codex 跑满配基准后补 §4.5。
