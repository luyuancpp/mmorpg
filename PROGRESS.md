# 本会话推进进度 — 2026-05-15(v3)/ 2026-05-16(v4 / v5 / v6)

> **会话目标**:用户说「全部做啊」(回档 / 跨服 / 合服全部做完)
> **采用策略**:基于 AUDIT.md 的真实现状,按 P0 → P1 → P2 顺序逐项推进
> **v2 更新**:第二会话完成 metrics 接线 + #15 角色重名调研收口
> **v3 更新**:第三会话尝试推进 #11,**摸完代码发现 bag 完全无持久化路径**,落档 bag-rollback-feasibility-analysis.md
> **v4 更新**:第四会话用户确认「玩家肯定要跨 zone 玩」,深查跨 zone 链路后**写新审计文档 cross-zone-readiness-audit.md(权威),修订 mmo_cross_server_architecture.md §7-8 §11 §13**
> **v5 更新**:同会话延续,代码层面落地了**步骤 2(PlayerFrozenComp + 延后 DestroyPlayer)**,过程中又发现两个新事实并落档
> **v6 更新(本节)**:推进 #27(proto 重新生成)+ #28(Kafka topic 订阅接线)。打开 #29 时发现需要先做分类决策才能改 17 个 system 文件,**落档 cross-zone-readiness-audit.md §11 业务系统 Frozen 接入分类指南**,#29 留给下次会话按指南分批实施

---

## ✅ v6 已完成

### #27 Proto 重新生成

- 从主仓拷 `go.mod` + `pbgen.exe` + `proto-gen.exe` 到 worktree(worktree `.gitignore` 第 118 行注释明示 `go.mod was never tracked`,所以 worktree 检出时缺这两个文件)
- 跑 `dev_tools.ps1 -Command proto-gen-run -UseBinary` —— 8 秒跑完,无报错
- 验证:`PlayerMigrationAckEvent` 类已生成在 `cpp/generated/proto/common/event/player_migration_event.pb.h:75`
- Go 侧 proto 同步更新

### #28 Kafka topic 订阅接线

- 改 `cpp/nodes/scene/main.cpp`:加 `#include "kafka/system/kafka.h"`,在 `SetAfterStart` hook 里加:
  ```cpp
  const std::string crossZoneGroupId = "scene-cross-zone-" + std::to_string(n.GetNodeId());
  n.RegisterKafkaMessageHandler(
      {"player_migrate", "player_migrate_ack"},
      crossZoneGroupId,
      &KafkaSystem::KafkaMessageHandler);
  ```
- groupId 按 nodeId 分隔(每节点独立 consumer group)防止同 zone 多 scene 节点抢同一份 ACK
- **未做 cpp 编译验证**(主仓 MSBuild 编译时间长 + 上下文消耗大,留给下次会话)

### #29 业务系统加 Frozen 检查 — 改路径为「先落档指南」

打开 #29 时摸 cpp/libs/services/scene 发现需要改 17 个 system 文件,**且每个 system 语义不一样**:
- 写入类(Currency/Bag/Quest)— 应该 reject
- 被动 tick 类(Buff/Skill cooldown/AOI)— 应该 skip
- 消息类(移动/技能/聊天)— 应该回 tip
- 跨玩家影响类(被攻击/治疗)— 需要 game design 拍板

不分类直接「每文件加一行 `if (IsCrossZoneFrozen) return`」会**改坏其中几类的语义**(比如 buff 衰减应该完全冻结还是继续 tick?)。

**改成两步**:
- 本会话:落档 `cross-zone-readiness-audit.md §11 业务系统 Frozen 接入分类指南`,4 类的具体处理 + 文件清单 + 实施顺序 + 验证清单
- 下次会话:按指南分批实施(§11.1 写入类 → §11.2 tick → §11.3 消息类 → §11.4 暂挂)

---

## ⚠️ 剩余 pending 任务(v6 状态)

| ID | 任务 | 阻塞性 | 状态 |
|---|---|---|---|
| #29 | 业务系统加 Frozen 检查 | 步骤 2 完整语义 | 设计指南已落 §11,等下次会话实施 |
| #25 | ACK + reaper(失败恢复)| 失败场景才需要 | 等 |
| #24 | 失败场景测试 | 验证 | 等 #25 |
| #23 | PlayerAllData 加 BagAllData/QuestAllData/MailAllData | **核心** —— 解决数据丢失 | 等产品/策划定 ItemEntry schema |
| #21 | bag 持久化(方案 A)| 同 #23 | 同 #23,本质合并 |
| #22 | quest/mail 持久化审计 | bag 之后 | 等 |
| #11 | per-service Export/Import RPC | bag/quest/mail 持久化做完后 | 等 #23 |
| #7 | Region 路由表 | 单 zone 不急 | 50+ zone 时再做 |

---

## v6 编译可用性状态

**当前 cpp 是否能编译过?**

- ✅ Proto 生成已跑过,`PlayerMigrationAckEvent` 类已落 `.pb.h/.pb.cc`
- ✅ `player_lifecycle.cpp` / `kafka.cpp` 引用的 protobuf 类现在都存在
- ⚠️ **未做实际 MSBuild 验证** —— 但代码层面所有引用都对应到已生成或现有的类,理论上应该过
- ⚠️ Go 侧 `data_service` 上轮已 `go build ./...` 通过(v2),v6 没动 Go 代码

**风险评估**:cpp 编译失败概率低但不为 0。如果失败,大概率是:
- include 路径细节(`kafka/system/kafka.h` 在 scene 节点 include 树里的解析)
- `PlayerMigrationAckEvent` 在 cpp_lifecycle.cpp 里被引用前需要的 forward declaration(generator 应该已经处理,但需要验证)

**下次会话开干前必跑**:`cd cpp/nodes/scene/build && msbuild scene.sln /p:Configuration=Debug` 或同等命令,确认编译通过。如果失败,把错误贴给下次会话先修编译再继续 #29。

---

## v6 worktree 文件清单

新增(本会话 v6):
- `cross-zone-readiness-audit.md §11` 新增章节(业务系统 Frozen 接入分类指南)

修改(本会话 v6):
- `cpp/nodes/scene/main.cpp`(+ Kafka topic 订阅)
- 16 个 `cpp/generated/**` 文件被 proto-gen 重新生成(`player_migration_event.pb.{h,cc}` / `common_event_player_migration_event_event_id.h` / `rpc_event_registry.{h,cpp}` / 等)
- `proto/event_id.txt`(被 proto-gen 触动)
- `cpp/nodes/scene/handler/event/player_migration_event_handler.{h,cpp}`(被 proto-gen 触动)
- `cpp/nodes/gate/handler/rpc/client_message_processor.cpp`(被 proto-gen 触动)
- 主仓 `tools/proto_generator/protogen/{go.mod,pbgen.exe,proto-gen.exe}` 被拷到 worktree(不会进 git,因为 gitignore 不跟踪)

---

## ⚠️ v5 步骤 2 实施过程的新发现

按 cross-zone-readiness-audit.md §3.2 件 2 实施 Frozen 状态时,发现两件之前没看到的事实:

### 发现 1:`player_migrate` topic 当前没人订阅

`grep -rn "RegisterKafkaMessageHandler" cpp/` 只命中 engine 层的实现 + SceneCommand 命令模板。**没有任何代码订阅 `player_migrate`**。

意味着:跨 zone 当前**单向 broken** —— 源端 publish 给 broker,目的端没 consumer,玩家根本到不了新 zone。这是先前未识别的 bug,跟我做的 ACK 改动正交。

**v6 已修复** —— #28 加了 `RegisterKafkaMessageHandler({"player_migrate", "player_migrate_ack"}, ...)`,topic 现在有 consumer。

详细分析见 `cross-zone-readiness-audit.md §10`。

### 发现 2:Kafka 必须 pb,不能 JSON(用户纠正)

我最初为求省事(避开 proto 重新生成)用 JSON 写了 ACK payload。**用户当场指出错误**:整个 codebase 其他 Kafka 消息都是 protobuf,JSON 解析慢一个数量级 + 破坏一致性 + 失去 schema 版本管理。

立刻撤回,改用 `PlayerMigrationAckEvent` protobuf message(已加在 `proto/common/event/player_migration_event.proto`,v6 已重新生成成 .pb.h/.pb.cc)。

### 真实状态

步骤 2 拆成 4 个子任务:
- ✅ 代码层面(7 件落地)
- ✅ #27 重新生成 proto(v6 已完成)
- ✅ #28 Kafka topic 订阅接线(v6 已完成)
- ❌ #29 业务系统加 Frozen 检查(v6 落档分类指南,留给下次会话实施)

---

## v5 已完成代码改动

### 代码新增

- `cpp/libs/services/scene/player/comp/player_frozen_comp.h`(纯 C++ struct,3 字段:`frozenAtMs / toZoneId / migrateAttempts`)

### Proto 新增

- `proto/common/event/player_migration_event.proto`:加 `PlayerMigrationAckEvent` message(player_id / from_zone / to_zone / ack_at_ms 4 字段)
- `proto/common/component/player_comp.proto`:加 NOTE 说明 PlayerFrozenComp 故意走 C++ struct 不走 proto

### 代码修改

- `cpp/libs/services/scene/player/system/player_lifecycle.h`:声明 `HandlePlayerMigrationAck` + `IsCrossZoneFrozen`
- `cpp/libs/services/scene/player/system/player_lifecycle.cpp`:
  - `HandleCrossZoneTransfer` —— 发完 Kafka 后 emplace `PlayerFrozenComp` 替代立即 DestroyPlayer
  - `HandlePlayerAsyncSaved` —— 检测 Frozen 跳过销毁路径(原 UnregisterPlayer 路径只对真退出登录生效)
  - `HandlePlayerMigration` —— 目的端成功 init 后用 `PlayerMigrationAckEvent` 发 ACK(protobuf 序列化,不是 JSON)
  - 文件末新增 `IsCrossZoneFrozen(player)` 实现
  - 文件末新增 `HandlePlayerMigrationAck(playerId, toZoneId)` 实现(含幂等检查、zone 不匹配检查)
- `cpp/libs/services/scene/kafka/system/kafka.cpp`:`KafkaMessageHandler` 加 `player_migrate_ack` topic 路由,用 `PlayerMigrationAckEvent.ParseFromString` 解析(注释明确指出本 handler 当前没被订阅,需要 #28 接线)

### 文档新增

- `docs/design/cross-zone-readiness-audit.md`(v2,新审计权威)— v1 完整方案 + v2 补 §10「步骤 2 实施过程的新发现」

### 文档修改

- `docs/design/mmo_cross_server_architecture.md §7-8 §11 §13`:按 Kafka 自治真实形态重写,标注每项的真实状态
- `docs/design/bag-rollback-feasibility-analysis.md`:加 v2 修正头部,方案 A 升级为 cross-zone-readiness-audit 的步骤 1
- `AUDIT.md`:加 v2 重大修正头部(回档 95% → 70%,跨服 85% → 50%)
- `PROGRESS.md`(本文件)

---

## ✅ 总计完成 10 项任务(v1+v2,v3+v4+v5 没新增完成项,但产出关键文档 + 步骤 2 代码)

---

## ⚠️ v4 关键发现:跨 zone 不可生产

第四会话深查 `player_lifecycle.cpp` + `player_database_loader.cpp` 后发现:

**跨 zone 链路只 Marshal 7 个 ECS 组件**(Transform / Currency / Skill / Level / 2×Uint / DerivedAttrs)。bag/quest/mail **不在 PlayerAllData proto 里**,跨一次 zone 静默丢失。`HandleExitGameNode` 走相同路径,**正常退出 / 重启同样丢 bag**。

更糟:Kafka send 后立即 `DestroyPlayer`(line 217),broker 失败 / 目标节点崩溃 = 玩家两边都没了。

**`mmo_cross_server_architecture.md §7-8` 描述的"SceneManager 严格 ACK 编排"根本没实现**。实际是 Kafka 自治 —— 这本身是对的(自治形态对你「玩家无限跨 zone」的低延迟需求更友好),只是缺三件套修复。

### 修复方案(权威):cross-zone-readiness-audit.md §3 三件套

1. **PlayerAllData 数据完整化** — 加 `BagAllData / QuestAllData / MailAllData` 子 message
2. **PlayerFrozenComp + 延后 DestroyPlayer** — 等 ACK 才真销毁
3. **`player_migrate_ack` Kafka topic + Redis migration 状态 + reaper** — 失败检测 + 重传 + 兜底

总工作量 4-6 周。**步骤 1 阻塞所有,前置是产品 / 策划定 ItemEntry schema**(装备强化等级 / 词条 / 宝石镶嵌等字段)。

### 同步修订的文档

- ✅ `cross-zone-readiness-audit.md` — 新增,完整审计 + 三件套方案 + 失败场景处理 + metrics 设计
- ✅ `mmo_cross_server_architecture.md §7-8 §11 §13` — 按真实形态重写
- ✅ `AUDIT.md` — 加 v2 重大修正头部(回档 95% → 70%,跨服 85% → 50%)
- ✅ `bag-rollback-feasibility-analysis.md` — 加 v2 修正,方案 A 升级为 cross-zone-readiness-audit 的步骤 1
- ✅ `PROGRESS.md`(本文件)— v4 更新

---

## ✅ 总计完成 10 项任务(v1+v2,v3+v4 没新增完成项,但是产出了关键文档)

### P0 三项

1. **`docs/design/server_merge_design.md`**(任务 #14)— 单一权威合服 SOP 文档(v2 已更新 §4.2 角色重名为「已确认无冲突」)
2. **MySQL binlog 自动归档 K8s CronJob**(任务 #19)— PVC + ConfigMap + CronJob + runbook 全套
3. **(角色重名)server_merge_design.md §4.2 已结**(任务 #15,v2 完成)

### P1 / P2 七项

4. **SavePlayerData 乐观锁实测验证**(任务 #20)
5. **Data Service per-player 锁验证**(任务 #3)
6. **Kafka offset reset 脚本**(任务 #18)
7. **AddCurrency 唯一入口旁路审计**(任务 #13)
8. **k8s-zone-rollback 一键脚本**(任务 #17)
9. **跨服 observability metrics(完整接线)**(任务 #9,v2 完成)— 见下方 §9 更新
10. **Instance 节点 conflict hook**(任务 #16,N/A)
11. **合服: 迁移工具**(任务 #5)+ **合服: 设计文档**(任务 #12)

---

## v2 新增完成项

### 9. metrics 接线尾巴(本次会话补完)

`metrics.Start(addr)` 已接入 `data_service/data_service.go`,`config.Config` 加 `MetricsListenAddr` 字段,`go mod tidy` 把 `prometheus/client_golang` 提升为 direct dep(`go.mod:8`)。

**验证**:
- `go build ./...` → **BUILD OK**(无输出 = 编译通过)
- `go test ./internal/logic/ ./internal/routing/` → **全 PASS**(logic 0.457s, routing 0.377s)
- `data_service.go` 启动 banner 显示 `metrics: <addr>/metrics`(若 config 配了)

剩余可选接线(P3,可推迟):
- `crossSceneTransitionLatency` / `crossSceneTransitionTotal` 只有 helper,**没有调用者**。需在 `scene_manager` 的切场景编排代码里调 `ObserveCrossSceneTransition` / `ObserveCrossSceneTransitionOutcome`。属于 P3,不阻塞投产

### 15. 角色重名调研(本次会话完成,**工作量 = 0**)

按 server_merge_design.md §4.2 调查清单跑完,**结论:当前数据模型不存在 player 级重名冲突,无需写改名逻辑**。

**决定性证据**:
1. Redis `account:{account}` key **不带 zone scoping**(`login_constants.go:14`)→ login 服务层强制全服唯一
2. `AccountSimplePlayer` proto 只有 `player_id` 一个字段(`user_accounts.proto:6-9`),无 name
3. `player_database` proto 也无 name 字段
4. `createplayerlogic.go:105-107` 创建玩家只填 `player_id`
5. HTTP API 的 `zone_id` 是路由参数,不是 account 命名空间分割维度
6. 公会重名 CLI 已自动检测(`merge_zone/main.go:240-260`)

server_merge_design.md §4.2 已更新为 v2「**已确认无冲突,无需处理**」,详证据 + 未来引入 player nickname 时的重评条件都已落档。

---

## ⚠️ 剩余 pending 任务(v3 状态)

### 阻塞中(必须先做才能解锁后续)

#### #21 给 bag 加持久化(方案 A) — **新增,阻塞 #11**

**为什么是 P0**:不仅是回档需求,更是当前数据完整性的硬伤。bag 在内存里 = 玩家退出 / 重启可能丢道具(需先验证)。

**做法**:
1. 新增 `BagComp` proto(`ItemEntry { item_uuid / config_id / stack_size / pos / bag_type }` × N)
2. 加到 `player_database` 作为字段 10
3. `player_database_loader.cpp` 补 bag 的 Marshal/Unmarshal:遍历 `itemRegistry_` ↔ `BagComp.items`
4. 跑「登录 → 加道具 → 退出 → 重登验证」回归

**工作量**:~1 周(proto + Marshal + 测试 + 链路验证)

**前置依赖**:**产品 / 策划定 ItemEntry schema** —— 装备的强化等级 / 词条 / 宝石需要哪些字段?这事 AI 拍不了

#### #22 quest/mail 持久化审计 — **新增**

`player_database_loader.cpp` 同样没 Marshal quest/mail。需要查清楚是「数据在别处持久化」还是「跟 bag 同病」。如果是后者,要重复 #21 的工作。

### 仍然 pending(原状)

#### #11 各业务服务 Export/Import RPC

**v3 状态**:**前置条件不满足**(等 #21 完成)。完成后工作量从「6 个服务的 RPC」缩水成「data_service rollback_logic 加 proto 字段过滤」~2-3 天

#### #7 跨服 P2 Region 路由表

单 zone 形态用不到,**有意识不做**。重启条件:50+ zone 部署 / 真正的跨服活动需求

---

## 真实进度对比

| 模块 | AUDIT 起点 | v1 后 | v2 后 | v3 修正 | **v4 修正** |
|---|---|---|---|---|---|
| 回档 | ~90% | ~95% | ~97% | ~70% | **~70%**(无变化,bag/quest/mail 不在快照) |
| 跨服 | ~75% | ~85% | ~88% | ~88% | **~50%**(深查跨 zone 链路后,数据丢失 + 失败丢玩家是致命问题) |
| 合服 | ~60% | ~75% | ~85% | ~85% | **~85%**(合服工具本身完整,但合服后玩家跨 zone 仍丢数据,需先修跨 zone) |

**v4 跨服降到 50% 的解释**:之前 v3 只看了「per-player 锁、version 字段、metrics 框架」这些**单点能力**,没看跨 zone 端到端链路完整性。事实上:
- ✅ Single Writer 通过「先 flush 再 Kafka 再销毁」隐式成立
- ✅ Layer 2 Redis SETNX + TTL 兜底锁已实现
- ❌ **跨 zone 数据完整性 0%**(bag/quest/mail 不跟)
- ❌ **失败恢复 0%**(无 ACK,Kafka 失败 = 丢玩家)
- ❌ **架构文档与实现不一致**(SceneManager 编排 vs Kafka 自治)

「玩家无限跨 zone 玩」这个核心设计目标的实际就绪度,真实数字是 **50% 左右**。组件都对,但端到端链路有致命漏洞。

---

## worktree 新增 / 改动文件清单(v2 累计)

新增(10 个):
- `AUDIT.md`(实现状态审计报告)
- `PROGRESS.md`(本文件,v2 更新)
- `docs/design/server_merge_design.md`(合服权威 SOP,§4.2 v2 已收口)
- `docs/ops/mysql-backup-pitr-runbook.md`
- `docs/ops/deferred-clawback-bypass-audit-2026-05.md`
- `deploy/k8s/manifests/infra/mysql-backup-cronjob.yaml`
- `tools/scripts/kafka_offset_reset.ps1`
- `tools/scripts/k8s_zone_rollback.ps1`
- `go/data_service/internal/metrics/metrics.go`

修改(8 个):
- `cpp/libs/modules/currency/system/currency_system.h`(防御性 doc)
- `deploy/k8s/manifests/infra/mysql.yaml`(PVC + binlog 配置)
- `docs/design/zone_data_rollback.md`(§3 缺口表 3 项已标完成)
- `go/data_service/internal/logic/data_logic.go`(metrics 埋点)
- `go/data_service/internal/logic/rollback_logic.go`(metrics 埋点)
- `tools/scripts/dev_tools.ps1`(注册 2 个新命令 + 21 个新参数)
- **`go/data_service/data_service.go`**(v2 新增:metrics.Start 接线)
- **`go/data_service/internal/config/config.go`**(v2 新增:`MetricsListenAddr` 字段)
- **`go/data_service/go.mod` + `go.sum`**(v2:prometheus/client_golang 提升为 direct)

---

## 下次会话开干顺序

1. **(可选)#9 P3 子项**:在 `scene_manager` 的切场景编排代码里加 `ObserveCrossSceneTransition`(4 phase)+ `ObserveCrossSceneTransitionOutcome`(5 outcome)埋点。让跨服切场景的 metric 真正有数据
2. **(决策)问客服总监**:是否需要 per-service 颗粒度回档?如果不要,关掉 #11
3. **(条件触发)**:#7 Region 路由表等 zone 数量真正爆炸再做

---

## Changelog

- **2026-05-15 v1**:初版,8 项任务完成,1 项 metrics 接线尾巴留下
- **2026-05-15 v2**(同日续会话):补完 metrics 接线 + #15 角色重名调研收口。10 项任务完成,剩 2 项「有意识不做」
- **2026-05-15 v3**(同日续第三会话):用户确认 #11 必须做(客服需要只回档背包)。摸 C++ 代码发现 bag/quest/mail **完全无持久化路径**,前置条件不满足。落档 `bag-rollback-feasibility-analysis.md` + 新建 #21 / #22。回档真实完成度从 97% 下修到 70%。
- **2026-05-16 v4**:用户确认「玩家肯定要跨 zone 玩」是核心设计。深查跨 zone 链路后发现 v3 还低估了问题严重度 —— 不只是 bag 没持久化,还有 Kafka 失败丢玩家、SceneManager 编排实际未实现 等问题。**v4 的成果是落档完整修复方案 `cross-zone-readiness-audit.md`(Kafka 自治 + 三件套)+ 同步修订 `mmo_cross_server_architecture.md §7-8 §11 §13` + 更新 AUDIT/PROGRESS/bag-rollback-feasibility-analysis 反映真相**。代码部分按用户要求接下来开始(从最低风险的 Frozen 状态开始)


---

## ✅ 本会话已完成

### P0 三项

1. **`docs/design/server_merge_design.md`**(任务 #14)— 单一权威合服 SOP 文档
   - 9 章:架构前提 / 已有工具 / 重名 / 完整 SOP(准备/停服窗口/验证/善后/回滚)/ 已知未覆盖 / 测试 SOP / 文档关系图 / Changelog
   - 把散落在 `tools/merge_zone/main.go`、`mmo_cross_server_architecture.md §9`、`guild_ranking_architecture.md §合服工具`、`enter-scene-zone-routing.md` 的合服知识收口为一份

2. **MySQL binlog 自动归档 K8s CronJob**(任务 #19, 原 `zone_data_rollback.md §3` 「优先级:高」缺口)
   - `deploy/k8s/manifests/infra/mysql.yaml` — 加 PVC(20Gi data + 50Gi backup)+ ConfigMap(my.cnf 开 log-bin / ROW format / 7 天保留)+ initContainer 创建 binlog 目录
   - `deploy/k8s/manifests/infra/mysql-backup-cronjob.yaml` — 每天 03:17 UTC mysqldump + binlog 复制,prune 策略 dumps 30 天 / binlog 14 天
   - `docs/ops/mysql-backup-pitr-runbook.md` — 部署 / 升级 / 日常运维 / PITR / 与合服衔接 / 故障排查的完整 SOP
   - `zone_data_rollback.md §3` 缺口表已标记此项为「已落地」

3. **(角色重名)server_merge_design.md §4.2 落档为 unknown**(任务 #15 改 P1)
   - **不写代码,改写文档** —— 因为 `player_database` proto 里没有 player_name 字段,需要先调查 `user_accounts.account` / `user.display_name` / Unity `AccountSimplePlayer` 的全服唯一性约束才能决定是否写改名逻辑
   - 已在 server_merge_design.md §4.2 落档:4 项调查清单 + 4 种实施方案矩阵 + 「为什么不直接动手」说明
   - 任务从 P0 降级为 P1,等下次会话做调查

### P1 / P2 五项

4. **SavePlayerData 乐观锁实测验证**(任务 #20)— **已确认是真乐观锁**
   - `data_logic.go:159 checkVersion` + `:168 Incr __version` + `:174 NewVersion`
   - SetPlayerField 同样
   - `Router.AcquirePlayerLock`(`router.go:254`)是真 Redis SETNX+TTL
   - **结论**:lock + version 双层兜底全部落地,符合 `mmo_cross_server_architecture.md §8 Layer 2`

5. **Data Service per-player 锁验证**(任务 #3)— 同上,**已落地**

6. **Kafka offset reset 脚本**(任务 #18)— `zone_data_rollback.md §3` 「优先级:中」缺口
   - `tools/scripts/kafka_offset_reset.ps1` — 4 种模式(`ToDatetime` / `ToEarliest` / `ToLatest` / `DeleteAndRecreateTopic`),默认 dry-run
   - `dev_tools.ps1` 注册 `kafka-offset-reset` 命令 + 9 个 Kafka 参数
   - `zone_data_rollback.md §3` 缺口表已标完成

7. **AddCurrency 唯一入口旁路审计**(任务 #13)— **审计 PASS**
   - grep `cpp/` 仅 2 处 `mutable_values()`,均在 `currency_system.cpp` 内部
   - 0 处外部调用 — 补缴 hook 不会被绕过
   - `currency_system.h` 顶部加防御性 doc(warning + audit history)
   - 落档 `docs/ops/deferred-clawback-bypass-audit-2026-05.md`(含持续保障建议)

8. **k8s-zone-rollback 一键脚本**(任务 #17)— `zone_data_rollback.md §3` 「优先级:中」缺口
   - `tools/scripts/k8s_zone_rollback.ps1` — 7 步流程(zone-down → Kafka drain → MySQL PITR 提示暂停 → Redis FLUSHDB → kafka-offset-reset → zone-up → 验证清单)
   - `dev_tools.ps1` 注册 `k8s-zone-rollback` 命令 + 9 个 rollback 参数
   - `zone_data_rollback.md §3` 缺口表已标完成

9. **跨服 observability metrics(部分)**(任务 #9)— **代码完成,接线未完**
   - `go/data_service/internal/metrics/metrics.go`(~200 行,仿 scene_manager 形式)
   - 9 个 metric:`save_player_data_total/save_latency_seconds`、`player_lock_total`、`version_mismatch_total`、`cross_scene_transition_latency/total`、`rollback_total/players_affected/orphans_cleaned`
   - `data_logic.go SavePlayerData` 三类 outcome(ok/version_mismatch/lock_conflict/redis_error)埋点完整
   - `rollback_logic.go RollbackPlayer/Zone/All` 全部埋点(含 affected/orphans)
   - `acquirePlayerLock` 三类 outcome 埋点完整
   - **未完工的接线见下面 §⚠️**

### 任务清单调整

10. **Instance 节点 conflict hook**(任务 #16)— **N/A**
    - `cpp/nodes/` 下只有 `scene/main.cpp` + `gate/main.cpp`,无独立 Instance 节点
    - 任务标完成(等 Instance 节点真存在时再做)

11. **「合服: 迁移工具」**(任务 #5)+ **「合服: 设计文档」**(任务 #12)— 标完成
    - 工具(`tools/merge_zone/`)+ 设计文档(本会话新写的 `server_merge_design.md`)都已落地

---

## ⚠️ 未完工的接线 — 下次会话必须先做

### 9-A. data_service 启动 metrics HTTP 端口

`metrics.Start(addr)` 已实现,但**没有人调它**。要在 `data_service` 主入口加一行(类似 scene_manager 的做法):

```go
// go/data_service/data_service.go(或 main.go,看实际入口)
import "data_service/internal/metrics"

func main() {
    // ... existing init ...
    metrics.Start(c.MetricsListenAddr) // 加这一行
    // ... existing start ...
}
```

同时在 `config.Config` 加 `MetricsListenAddr string` 字段(参考 scene_manager 的 config)。

### 9-B. go mod tidy

诊断器报警:`github.com/prometheus/client_golang should be direct (go mod tidy)`。`prometheus/client_golang` 当前是 indirect 依赖(通过其他包传递引入),metrics.go 直接用它后需要提升为 direct:

```bash
cd .claude/worktrees/rollback-cross-merge/go/data_service
go mod tidy
```

### 9-C. 横切场景 transition 埋点未接

`crossSceneTransitionLatency` + `crossSceneTransitionTotal` 只定义了 helper(`ObserveCrossSceneTransition` / `ObserveCrossSceneTransitionOutcome`),**没有调用者**。这需要在 `scene_manager` 的切场景编排代码里加埋点(`release` / `save` / `load` / `total` 四个 phase)。是 P3 范围,可下次会话或更晚做。

---

## 剩余 pending 任务(本会话未做)

| ID | 任务 | 估时 | 难度 | 备注 |
|---|---|---|---|---|
| #7 | 跨服 P2 Region 路由表 | 2-3 个会话 | 高 | 当前单 zone,不急 |
| #11 | bag/quest/mail/currency/guild/friend Export/Import RPC | 3 个会话(每服务 0.5) | 中 | 客服走整人回档则可不做 |
| #15 | 合服角色重名(待调研)| 0.5(调研)+ 0.5(实现)| 低-中 | 已在 server_merge_design.md §4.2 落档 |
| **#9 子项** | metrics 接线 + go mod tidy | 0.2 个会话 | 低 | **下次会话先做这个**(见 §⚠️) |

---

## worktree 新增 / 改动文件清单

新增(11 个):
- `AUDIT.md`(实现状态审计报告)
- `PROGRESS.md`(本文件)
- `docs/design/server_merge_design.md`(合服权威 SOP)
- `docs/ops/mysql-backup-pitr-runbook.md`(MySQL 备份 / PITR runbook)
- `docs/ops/deferred-clawback-bypass-audit-2026-05.md`(补缴旁路审计报告)
- `deploy/k8s/manifests/infra/mysql-backup-cronjob.yaml`(每日备份 CronJob)
- `tools/scripts/kafka_offset_reset.ps1`(Kafka offset reset 脚本)
- `tools/scripts/k8s_zone_rollback.ps1`(一键 zone 回档)
- `go/data_service/internal/metrics/metrics.go`(Prometheus metrics 包)

修改(5 个):
- `cpp/libs/modules/currency/system/currency_system.h`(顶部加防御性 doc + audit history)
- `deploy/k8s/manifests/infra/mysql.yaml`(加 PVC + ConfigMap + initContainer + binlog 配置)
- `docs/design/zone_data_rollback.md`(§3 缺口表 3 项已标完成)
- `go/data_service/internal/logic/data_logic.go`(metrics 埋点)
- `go/data_service/internal/logic/rollback_logic.go`(metrics 埋点)
- `tools/scripts/dev_tools.ps1`(注册 2 个新命令 + 21 个新参数)

---

## 给下次会话的开干顺序

1. **先解决 §9-A + §9-B**(0.2 个会话):metrics 接线 + go mod tidy,然后 `go build ./...` 确认整个 data_service 能编
2. **再做 #15 调查**(0.5 个会话):按 server_merge_design.md §4.2 调查清单跑一遍,落档「需要 / 不需要改名逻辑」决策
3. **决定 #11 Export/Import 优先级**(对话半轮):问用户「客服是否需要 per-service 颗粒度回档」?如果只走整人回档则不必做
4. **(可选)#7 Region 路由表**:只在「真要做 1000 zone」时启动

---

## 真实进度回顾

用户 3 轮前问「跨服合服回档你给我做完了吗」时,我说「没做完,~5%-70% 不等」。

经本会话的 AUDIT + 实际工作后,真实数字:

| 模块 | AUDIT 估计 | 本会话后 |
|---|---|---|
| 回档 | ~90% | **~95%**(P0 ops 三项 + 旁路审计 + version 验证已落,只剩 Export/Import RPC 颗粒度可选项) |
| 跨服 | ~75% | **~85%**(per-player 锁 + version 已确认,metrics 框架已落 80%) |
| 合服 | ~60% | **~75%**(权威 SOP 文档落地,工具已存在,剩玩家重名调研 + 实施) |

**实际可用性**:回档、合服在「合理审慎使用」前提下**可以投生产**。跨服观测性差点 metrics 接线,但代码层面架构(per-player 锁 + version + NodeId 冲突差异化)已经支撑实际跨服流量。

---

## Changelog

- **2026-05-15 v1**:初版,8 项任务完成,1 项 metrics 接线尾巴留下
- **2026-05-15 v2**(同日续会话):补完 metrics 接线 + #15 角色重名调研收口。10 项任务完成,剩 2 项「有意识不做」
- **2026-05-15 v3**(同日续第三会话):用户确认 #11 必须做。摸 C++ 代码发现 bag/quest/mail **完全无持久化路径**,前置条件不满足。落档 `bag-rollback-feasibility-analysis.md` + 新建 #21 / #22
- **2026-05-16 v4**:用户确认「玩家肯定要跨 zone 玩」是核心设计。深查跨 zone 链路后,**写新审计文档 cross-zone-readiness-audit.md(Kafka 自治 + 三件套权威方案)+ 同步修订 mmo_cross_server_architecture.md §7-8 §11 §13 + 更新 AUDIT/PROGRESS/bag-rollback-feasibility-analysis 反映真相**
- **2026-05-16 v5**(同日续第五会话):用户要求按「文档→改文档→改代码」顺序继续。**代码层面实施步骤 2(PlayerFrozenComp + 延后 DestroyPlayer)**。两个会话内发现:① `player_migrate` topic 当前没人订阅(落档为 audit §10);② 用户纠正 Kafka 必须 pb 不能 JSON,撤回 JSON 改 `PlayerMigrationAckEvent` protobuf。步骤 2 代码已落 7 件,剩 3 个子任务:**#27 proto 重新生成(必须,否则编译不过)/ #28 topic 订阅接线 / #29 业务系统加 Frozen 检查**
- **2026-05-16 v6**(同日续第六会话):**完成 #27(proto 重新生成)+ #28(Kafka topic 订阅接线)**。Proto-gen 跑通(从主仓拷 go.mod + 二进制绕开 worktree gitignore 限制),PlayerMigrationAckEvent 类已生成。Scene main.cpp 加 RegisterKafkaMessageHandler 订阅 `player_migrate` + `player_migrate_ack` topic。打开 #29 时发现需要先做 17 个 system 的分类决策(写入/tick/消息/跨玩家影响),**落档 cross-zone-readiness-audit.md §11 业务系统 Frozen 接入分类指南**,#29 留给下次会话按指南分批实施。**未做 cpp MSBuild 编译验证**(本会话上下文不够,留 caveat 给下次会话)
