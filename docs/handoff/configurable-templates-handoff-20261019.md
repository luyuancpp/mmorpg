# 交接：战斗策略 / 活动 / 玩法模板可配置化（含玩家自定义模板）——从设计到落码（2026-10-19）

> **接手第一步**：读本文 §0–§3，先请负责人回答 §3 的授权与拍板问题，再按 §4 的批次落码。
> **唯一设计依据**：[configurable-templates.md](../design/configurable-templates.md)（v0.2，约 1120 行）。本文只记"做到哪、下一步怎么做、有哪些坑"，设计细节一律以设计文档为准，两者冲突时以设计文档为准。
> **为什么交接**：负责人换账号继续（2026-10-19）。本会话只做到设计 v0.2 与全文源码核对，**代码一行都没写**。

---

## 0. 给新账号的开场提示词（可直接复制）

```
你接手"战斗策略 / 活动 / 玩法模板可配置化（含玩家自定义模板）"的落码。
先走 AGENTS.md 会话启动门禁，然后读 docs/handoff/configurable-templates-handoff-20261019.md，
再读 docs/design/configurable-templates.md（设计 v0.2，唯一依据；重点 §0、§3、§7、§10，以及要做的那一步对应的章节）。
第一步：按交接 §3 请负责人确认授权范围（改动超过 30 个文件要先授权，AGENTS §10.2）和待拍板项，
确认后按交接 §4 的批次顺序落码；推荐第一批是设计 §10 第 2 步（导表器通用规格校验器 + 日期字符串列）。
约束：你只写代码，不编译、不测试、不跑协议生成、导表或冒烟（交 Codex），每批给出 Codex 验证命令和通过标准；
不 commit / push；同一工作区有别的会话在写，改文件前先 git status / git diff、只改自己的段落，提交时别 git add -A；
docs/PROGRESS.md 只追加。两版都要做（AGENTS §12）：Java 仓 D:\luyuan\xuanming-server-mmo 同批改并在 PARITY.md 登记。
全部用中文，运行期日志与错误文本用英文。
```

---

## 1. 背景与负责人原话

- **2026-10-19**：「对了 我游戏中想弄 战斗策略模版可配置，活动模版可配置，玩法模版可以配置，这种一般怎么弄呢」——会话先做了 5 路现状调研 + 3 套方案 + 2 视角评审，结论是"表驱动原子组合，不上 Lua"。
- **2026-10-19**：「把这套方案写成 docs/design/configurable-templates.md，包含三个领域的表结构、硬约束和落地顺序，并在 docs/README.md 登记」。
- **2026-10-19**：「我想就是玩家可以自定义模版，就和网易的狼人杀一样，自定义模版可以么，全部细节记录到文档」——设计 §8 由此而来（又做了 4 路调研 + 2 视角评审，v0.1 → v0.2 修订 30 处）。
- **2026-10-19**：「做完了吗，代码实现完毕了吗？你只实现代码，协议生成 编译冒烟交给codex，如果你做完了告诉我可以关闭窗口」——会话答复：只完成了设计文档，代码未开工；全量实现远超 30 个文件，需先授权并拍板若干项（见 §3）。**负责人尚未回答这些问题**。
- **2026-10-19**：「加个交接文档吧，我换账号去做」——即本文。
- **长期**：「你只实现代码，协议生成 编译冒烟交给 codex」。项目**尚未上线**（AGENTS.md 门禁、`docs/design/db-write-fencing-mysql.md` 文末「上线状态」），混跑 / 滚动升级类约束上线前不阻塞。

---

## 2. 现状

| 项 | 状态 |
|---|---|
| 设计文档 `docs/design/configurable-templates.md` | **v0.2 定稿**。经过：①第一轮 5 路调研 + 3 方案 + 2 评审；②§8 玩家自定义模板 4 路调研 + 1 设计 + 2 评审（修订记录 §8.13）；③全文 6 路只读核对员对照源码核对 407 条现状断言、修正 36 处（§8.13 末尾"全文源码核对"） |
| README 登记 | 已在 `docs/README.md`「玩法系统」分类登记 |
| 提交状态 | 设计文档的**早期版本**与 README 登记行已被其他会话 / 负责人快照提交带进库（`git log -- docs/design/configurable-templates.md` 可见 `85a457a55` 等），**最新 v0.2 仍是工作区未提交改动**。本会话没有做任何提交 |
| 代码 | **0 行**。`data/schema` 下 BattleAtomKind / BattleStrategy* / PlayMode* / BattleRuleSet / Activity* / PlayerTemplate* 等新表都还不存在；导表器没有通用规格校验器；C++ / Go / Java 均未动 |
| Java 版 | 未开工，`PARITY.md` 未登记 |
| PROGRESS | 已追加一条指向本交接的条目（2026-10-19） |

---

## 3. 开工前必须请负责人决定的事

### 3.1 授权范围（AGENTS §10.2：预计改动 30+ 文件要先停下等授权）

全量实现涉及 C++ battle / scene、Go match / guild / 新全局服务、Python 导表器、proto、十几张新表，以及 Java 仓 8 个模块，远超 30 个文件。请负责人按批授权，建议问法：

> "按设计 §10 的顺序，先授权第 2 步（导表器，约 10 个文件），做完验证后再授权第 3 步（战斗策略 MVP）？还是一次授权多步？"

### 3.2 待拍板项（会直接影响代码怎么写）

| # | 问题 | 设计出处 | 推荐 |
|---|---|---|---|
| 1 | 是否同意建**跨语言战斗金样**（C++ 导出同种子事件 hex，Java 回放比对） | §10 第 1 步第 2 项 | 同意。没有它，"两版同输入同输出"只能靠人工对照 |
| 2 | 指纹 `warn → enforce` 的切换时间表 | §3.6、§10 第 1 步第 1 项 | 先 dev 环境 enforce，再按 zone 切 |
| 3 | 接受 Boss 全部技能在开局就对客户端可见吗（派生技能集写进 `skill_table_ids`） | §4.7 | 接受；否则要另开服务端内部列表 |
| 4 | 宝宝是否与怪物策略同批接入 | §4.7 | 第 4 步再做 |
| 5 | 活动进度：独立 `PlayerActivityComp` + 新 RPC，还是复用 Mission scope 2 | §5.3 | 独立结构 |
| 6 | 自定义对局 `CreateBattleRequest` 加字段 14（H10 唯一例外） | §8.6.2、§8.12 第 1 条 | 同意，由 battle 建房前白名单 fail-closed |
| 7 | CUSTOM 局是否"无损"（不扣药、不回写 HP、不走阵亡回满） | §8.12 第 2 条 | 无损 |
| 8 | 评分 PVP 里是否允许玩家策略 | §8.12 第 3 条 | 默认禁用 |
| 9 | 其余 §8.12 第 4–17 条 | §8.12 | 落到对应阶段时再问 |

---

## 4. 推荐落码顺序（每批交 Codex 验证，验证通过再开下一批）

> 依据设计 §10。§8（玩家自定义模板）是第 7 步，**前面至少有 4 个没开工的步骤**（§10 第 1、2、3、5 步），不能提前。

### 批 A：设计 §10 第 2 步——导表器能力（推荐第一批，无待拍板项）

**目标**：在导表期 fail-closed 拦下"列角色错、参数越界、条件外键缺失、不用的参数非 0"，并支持"UTC+8 墙钟字符串 → UTC 毫秒"的时间列。后面每一步的导表期校验都靠它。

**开工前要先补的设计细节**（设计文档只写到"字典表带规格列 + 通用校验器"，没写引用列怎么声明）：
- 引用原子的列如何声明自己的角色和参数绑定。建议新增 cfg option，例如在 `cond1_kind` 上写 `[(cfg_atom_kind) = "BattleAtomKind", (cfg_atom_role) = 1, (cfg_atom_params) = "cond1_p1,cond1_p2", (cfg_atom_ref) = "skill_id"]`，校验器据此查字典行的 `category` / `p1_min..p2_max` / `ref_table`。先把这个 option 形状写回设计 §3.1 / §3.4，再落码。
- 时间列 option 的形状，例如 `[(cfg_time) = "utc8_wall_clock"]`，单元格写 `2026-10-01 05:00`，导表期转成 UTC 毫秒并校验格式。
- 新 option 要在扩展号段 60001–60199 里取号（`cfg_options.proto` 注释）。

**涉及文件（预估，约 10 个）**：
- `data/schema/cfg_options.proto`（新 option 声明）
- `tools/data_table_exporter/core/schema_proto.py`（解析新 option）
- `tools/data_table_exporter/core/orchestrator.py`（`run()` 的 fail-closed 校验槽挂新校验器）
- 新增 `tools/data_table_exporter/core/atom_spec.py`（通用规格校验器）
- `tools/data_table_exporter/core/excel_reader.py`（时间字符串列的转换；注意现有 `_convert_cell` 对 float 解析失败会静默返回 None，新列必须报错）
- 新增 `tools/data_table_exporter/tests/test_atom_spec.py`、补 `test_schema_proto.py`
- Java 仓：`game-table-codegen` 的 `TableSchemaReader`（读新 option）与 `TableSourceGenerator`（生成加载期同口径校验，与 `checkForeignKeys` / `checkTipReferences` 同处），运行时辅助在 `game-table` 的 `com.game.table.load`
- 若需样例表验证：用 `data/Test.xlsx` / `test_table.proto` 这类测试表，不要提前建正式业务表

**交 Codex**（工作目录仓库根；失败保留首个错误完整输出，不改断言、不连续重试）：
1. `cd tools/data_table_exporter && py -m pytest -q` —— 全绿，新用例在列：坏字典引用、越界参数、未用参数非 0、条件外键缺失、时间串格式错各至少一条失败用例，且失败时产物不落盘。
2. `py tools/data_table_exporter/tools/sandbox_export.py --out <临时目录> --compare` —— 现有表产物逐字节不变（新校验器对存量表零影响）。
3. `py tools/data_table_exporter/tools/verify_schema_parity.py` —— 通过。
4. Java 仓：`./mvnw -B -pl game-table-codegen,game-table -am test` —— 全绿。

### 批 B：设计 §10 第 1 步——还债（需 §3.2 第 1、2 项拍板）

1. 指纹 enforce 灰度方案：match `TableFingerprintMode`（`go/match/internal/config/config.go`、`go/match/etc/match_service.yaml`），battle `FingerprintMode` / `BATTLE_TABLE_FINGERPRINT_MODE`（`cpp/nodes/battle/logic/battle_room_manager.cpp`）。
2. 跨语言战斗金样（Java battle-engine-spec §13.9 的 G1–G14 场景）：本仓 `cpp/tests/turn_battle_engine_test/` 导出事件 hex 夹具，Java 回放比对。
3. Java 追平 C++ 已漂移的**两处**：PVE 人数档（`19a3b8980`）、装备属性（`10cca6b66`，含连击 / 反击 / 反震 / 抗异常与 `BATTLE_EVENT_RESIST` / hit_kind）。Java `PARITY.md` 的"回合制战斗确定性引擎"行已登记待跟。

### 批 C：设计 §10 第 3 步——战斗策略 MVP（单阶段）

见设计 §4 与 §10 第 3 步。要点：
- 表：`BattleAtomKind`、`BattleStrategy`、`BattleStrategyRule`；`Monster.strategy_id`（monster_table.proto 现有 1–10、9001，追加 11）。
- 唯一接缝 `TurnBattleEngine::FillDefaultActions`；新模块 `cpp/libs/services/battle/system/battle_strategy.{h,cpp}`（纯逻辑，回合结算路径不打日志）；`BattleDataProvider.FindStrategy`；`AppendMonsterActor` 写派生技能集；DecisionTrace；进程内聚合指标；新表追加到 `BattleTableFingerprint::ComputeFrom` **末尾**。
- 新表要改 `cpp/generated/table/table.vcxproj` 与 CMakeLists（逐文件显式列出）。
- Java `game-battle-engine` 同批：`TurnBattleEngine.fillDefaultActions` 同位置接入，`BattleTableFingerprint` 同序追加。
- 验收：`strategy_id = 0` 时现有全部同种子用例逐字节不变；每个原子有用例；坏表导表期失败；C++ 导出新用例事件 hex，Java 回放一致。MSBuild 串行 `/m:1`。

### 之后

第 4 步阶段状态机 → 第 5 步玩法 `PlayMode` / `BattleRuleSet`（match 接表）→ 第 6 步活动 v1 → 第 7 步玩家自定义模板 P1（官方板子 + 带板切磋，不建模板服务、不开自由文本）→ P2 自建房 → P3 模板服务与分享码 → P4 自动战斗策略（宝宝优先）→ P5 自由文本与内容安全 → P6 可选项。每步细节、文件位置与验收见设计 §4–§6、§8.10、§10。

---

## 5. 硬约束速查（全表见设计 §7 H1–H19）

- 所有新列 0 值 = 现行行为，存量同种子回放逐字节不变（H1）。
- v1 不新增 RNG 抽取点；同分按 (hp, actor_id)；`RandomEnemy` 只配普攻（H2）。
- v1 禁止对敌施 buff 的原子，且 `CastSkill` 不得引用 `effect[]` 含毒 / 冰冻 / 眩晕 / 沉默类 buff 的技能——会经 `ApplySkillToTarget → AddBuffToActor → RollAilmentResist` 消耗随机数（H3）。`CastSkill` 规则的 `target_kind` 不得为 0（AOE 也要非 0 目标）。
- AI / 玩家策略运行态只放服务端内部结构，不进客户端可见的 `BattleActorState`（H5、H18）。
- 活动周期状态不进 `MissionListComp`（`mission_marshal.cpp` 的 Unmarshal 只恢复已知字段，同版本内也会丢，导致重复领奖）（H7）。
- 换期只清进度，不吞未领奖励；触发器不直接发物（H8、H9）。
- 玩法规则由 battle 按 `CreateBattleRequest` 已有的 `(match_mode, battle_config_id)` 自查表，唯一例外是玩家覆盖层字段 14（H10）。
- 参数列用具名标量（策划表 int64，玩家可见 proto 一律 uint32 且 < 2^21），不用 repeated 槽位 / double / exprtk 表达式（H11、§8.3）。
- 自定义对局一律 `MATCH_MODE_CUSTOM = 7`，无奖、无损、不计分，四层代码写死（H16、§8.7 F1–F11）。
- tip 码只由导表器发号（H15）。
- 两版同批（H19）。

---

## 6. 核对过的代码事实与坑（落码时直接用）

- **导表器**是 `tools/data_table_exporter`，不是 `tools/proto_generator`（后者是 RPC 生成器）。只支持五种字段形态，不支持 oneof / enum / 嵌套子消息 / JSON 列；子消息字段上的 `cfg_fk` 不生效；repeated 标量列里的 0 / -1 / 空串会被丢弃；`cfg_composite` 0 张表在用（首次使用要单独验证）。生成的常量没有 `kCount`，"字典 ↔ 处理器表"只能启动期核对。
- **热更未落地**：C++ / Go 生成了 `ReloadTables` 但生产无调用方；Java 没有这个函数，只有不可变快照 `ConfigTables.load()`。
- **战斗**：`FillDefaultActions` 在 `ResolveCurrentRound` 里早于 `currentGroupId = 0`，在其中发事件会与本回合行动组撞号或乱序，而且它现在不接收 `TurnResultS2C`，要先改签名（设计 §4.4）。引擎 `Initialize` 阶段有 4 处 `LOG_ERROR`，回合结算路径不打日志。
- **玩家档字段号**：`player_database` 的 21（`persist_schema_gen`）、22（`mystery_merchant`）已占用，下一个空闲号 23；新顶层字段经 go/db `savePlayerRow` 的 `tx.Save` **整行写**，`player:{id}:*` 不在生产读写链上。
- **tip 段**：30000 已被 `mystery_merchant_error` 占用；建议 `activity_error` = 31000、`player_template_error` = 32000（以导表器自检为准）。
- **协议现有最大字段号**（核对过）：`MatchMode` 0–6；`CreateBattleRequest` 1–13；`CreateBattleResponse` 1–2；`BattlePlayerSnapshot` 20；`BattlePetSnapshot` 12；`BattleSettlementData` 15；`ChallengePlayerRequest` 1–3；`ChallengeInviteS2C` 1–5；`SpectateBattleRecord` 1–2。
- **deadline**：旧路径按 `BattleMaxDurationSeconds = 300`（`gather.go`），全球权威路径在 `authority.Store.claimTx` 里写死 `DeadlineMs = n + 300000`，改 deadline 两处都要动。
- **结算**：阵亡即满血满蓝复活是所有对局的通用逻辑（`ApplySettlementToEntity`、`PetSystem::ApplyBattleSettlement`），不分局类型；`ApplySettlementToEntity` 是唯一结算入口（含离线 pending 补应用）。
- **任务**：`CheckAccept` 现在没有任何玩家准入门槛（等级门槛是新增能力）；任务是"多目标进度 + 完成位 / 待领位"；`player_mission.cpp` 有 5 处写死 scope 0。
- **match 不加载任何配表**（`LoadTables` 只在 guild、login、player_locator、scene_manager）；PVE_TEAM 队列 key 带人数段 `size<N>`。
- **battle / scene 没有 Prometheus 端点**；`deploy/k8s/asset-channel-alerts.yaml` 的 LogQL 目前只是值班固定查询（Loki ruler 未接线）。
- **全球权威恢复路径**只持久化 `activityContext`，做 CUSTOM 时 `MatchFormationRecord` 必须加规则与座次（设计 §8.6.1）。
- **battle 建房副作用在回包之前**（`HandleCreateBattle` 先推 `BattleStartS2C`、发 `SendBattleConfirmedEvent`），所以 fail-closed 只能放在 `EmplaceRoom` 之前（设计 §8.6.2）。

---

## 7. 本会话的工作流产物

调研、方案、评审、核对的原始输出都在本会话的临时目录和 `~/.claude/projects/.../subagents/workflows/` 下（`wf_f8a30d40-720` 调研与方案、`wf_93994ad6-d81` 玩家自定义模板、`wf_fe710838-acd` 全文核对），**新账号读不到**。结论已全部写进设计文档：选型打分与理由见 §2，评审修订见 §8.13，核对修正见 §8.13 末尾。需要原始证据时直接按设计文档里的文件 / 函数名回源码查。

---

## 8. 工作区注意

- 同一工作区（main，不开 worktree）有多个会话同时写未提交改动，负责人也会把整个工作区快照提交（本设计文档的早期版本就是这样入库的）。改文件前 `git status` + `git diff`，只改自己的段落；交付说明里写"未提交"之前先 `git log -1 -- <文件>` 核对。
- 提交时按自己改过的文件清单 add，不要 `git add -A`。
- `docs/PROGRESS.md` 只追加（`.gitattributes` 配了 `merge=union`）。
- 每批落码同时在 Java 仓 `PARITY.md` 登记一行，本仓 PROGRESS 条目注明对应 Java 版本号。
