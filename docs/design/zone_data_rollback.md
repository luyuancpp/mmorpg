# Zone 数据回档方案

> **修订(2026-08-15)**: 数据层迁 TiDB 后,灾难恢复级的"MySQL PITR"须改为 TiDB BR/PiTR(runbook 未改写前**不许迁生产**);Phase 2 单一全局库后 zone 粒度回档改由应用级按 home_zone 承担。见 [global-data-layer-tidb-decision.md](./global-data-layer-tidb-decision.md) §D8。

## 概述

Zone 数据回档分两个层面：
1. **应用级回档**（RPC/逻辑骨架已实现，生产执行暂停）：通过 `RollbackZone` RPC，从快照恢复玩家数据
2. **灾难恢复级**（Ops 操作）：MySQL PITR + Redis flush + Kafka offset reset

> **当前安全状态（2026-08-03）**：应用级 `RollbackPlayer` / `RollbackZone` /
> `RollbackAll` 在生产 `ServiceContext` 中没有可用的跨服务离线 epoch 栅栏，
> 因此会以 `error_code=16 (NotImplemented)` **fail-closed**，不修改任何玩家
> 数据。这不是“先查一次玩家是否在线”能代替的问题。

---

## 1. 单玩家回档（应用级）

详见 [single_player_rollback.md](single_player_rollback.md)。

- 踢玩家下线 → 找目标时间点之前最近的快照 → 调用各服务 `ImportPlayerData()` 恢复
- 自动校验交易日志防止道具复制（有正常转移记录的物品不恢复）
- 本质是**补偿式**回档，不是时间倒流

---

## 2. 整 Zone 应用级回档（`RollbackZone` RPC）

### 必须先闭合的在线写栅栏

当前 Scene 的后续存盘会经 Kafka 进入 `go/db` 并落 MySQL，而
`data_service` 的应用级回档修改 Redis。如果只在回档前查一次
“当前离线”，会同时存在两个窗口：

- 查完后玩家可以重新登录/Scene 激活（TOCTOU）；
- 旧 Scene epoch 在回档前已发出、但尚未落库的存盘，会在回档后
  再次覆盖结果。

因此 `RollbackFence` 的契约必须是原子的跨服务离线 epoch：

1. 阻止目标 player/zone 的新登录和 Scene 激活；
2. 让当前 Scene 下线并排空它已发出的 Kafka/DB 写；
3. 存盘消息携带 epoch，DB 落库端拒绝旧 epoch；
4. 栅栏覆盖“意图审计 → pre-rollback 安全快照 → 恢复 → 结果审计”
   全过程，持久化完成后才释放。

当前仓库尚没有 login / player_locator / Scene / db 共同实现这个协议。
`data_service` 仅保留了可注入的契约接口；生产不注入实现，所以执行路径
默认禁用。单元测试可注入 fake fence，验证缺栅栏、安全快照失败、
审计失败时均不会静默继续。

### 设计决策：Guild/Friend 不回档

**结论：Guild/Friend 数据不需要回档。**

**原因**：
- Player Redis blob（`player:{id}:player_database`）**不包含** guild_id 或 friend 引用
- Guild 通过 `GuildService` gRPC 独立查询，Friend 通过 `FriendService` gRPC 独立查询
- 两个系统的数据完全解耦，回档 player 数据不会与 guild/friend 产生不一致
- 即使存在时间差异（如玩家回档到加入公会前，但 guild_member 表仍有记录），
  这类不一致通过正常的 guild/friend 操作即可自愈（退出重进、重新申请等）

> **2026-09-28 追加(帮会二期 B5d-2b)**:上面"不需要回档、可自愈"只对**关系**数据(成员、好友)成立,
> **不适用于帮会资产**。捐献 / 帮会商店 / 活动发奖是玩家与帮会两边各记一笔的资产操作:回档把玩家的钱包
> 换回快照,帮会资金、帮贡、限购与领奖记录却不在快照里 —— 捐献方向变成资产复制,商店 / 发奖方向变成
> 玩家白亏,这不会自愈。现在三个 `Rollback*` 在写任何玩家数据之前都要过"帮会资产闸":先问 guild 这些玩家
> 自快照以来有没有已应用的资产操作,有就默认拒绝(`ErrCodeRollbackGuildDivergence`,可带
> `accept_guild_divergence` + `x-admin-token` + reason/operator 放行并留逐行日志供人工补偿),问不到就
> 拒绝且不可放行(`ErrCodeRollbackGuildCheckFailed`),写完 10s 后复查一次
> (`ErrCodeRollbackGuildDivergedAfterWrite`)。三个 `Rollback*` 也因此一律要求 `x-admin-token`。
> 设计与运维手册见 [guild-phase2/07-rollback-fail-closed.md](./guild-phase2/07-rollback-fail-closed.md)。

### 无快照角色处理（fail-closed）

`RollbackZone` **不会再自动删除任何无快照角色**。快照只在 GM、事件和回档
安全点产生，仓库内没有周期性快照任务；因此“目标时间前没有快照”不能证明
“角色是在目标时间后创建的”。把两者等同会删除大量正常老玩家。

| 情况 | 处理 |
|------|------|
| T 之前创建的角色 | 从 player_snapshot 恢复数据 ✅ |
| 目标时间前没有快照的角色 | 仅列入 `orphan_player_ids` 候选名单，不恢复、不删除 |
| 候选角色的 Redis / zone mapping | 保持原样 |
| 候选角色的账号 / guild / friend | 保持原样 |

字段名 `orphan_player_ids` 为保持 wire compatibility 暂不改名；它的当前语义是
“需要人工核对的无快照候选”，**不是已确认的新建角色，更不是已清理结果**。
`orphans_cleaned` 恒为 0。若以后恢复自动清理，必须先引入权威角色创建时间，
并走独立审批、pre-delete 快照和可恢复删除流程。

### 执行流程（栅栏落地后，2 阶段）

```
Preflight: 获取跨服务离线 epoch 栅栏 + 写 STARTED 审计
→ 任一前置失败则停止，零玩家数据变更

Phase 1: 恢复 player 数据
────────────────────────
遍历 zone 内所有有快照的 player
→ 对每个 player: 找 target_time 之前最近的 player_snapshot
→ 创建 pre-rollback 安全快照
→ 用 snapshot 数据覆盖 Redis

Phase 2: 报告无快照候选
──────────────────────
SCAN mapping Redis, 找到该 zone 下所有有 zone mapping 的 player
→ 对比 snapshot 中的 player 列表，得到差集（候选）
→ 将候选写入响应，所有数据和映射保持不变

Finalize: 写 RESULT 审计 → 释放 epoch 栅栏
```

### 代码位置

| 文件 | 作用 |
|------|------|
| `go/data_service/internal/logic/rollback_logic.go` | RollbackZone（player 回档 + 无快照候选报告） |
| `go/data_service/internal/routing/router.go` | GetAllPlayerIDsInZone（SCAN mapping Redis） |
| `go/data_service/internal/store/snapshot_store.go` | player_snapshot + rollback_audit_log CRUD |
| `proto/data_service/data_service.proto` | RollbackZone RPC 定义 |

### 跨服务调用链

```
RollbackZone (data_service)
  ├── RollbackFence: 阻断新会话 + 排空/拒绝旧 epoch 存盘（尚未落地）
  ├── Phase 1: 恢复每个 player 的 Redis 数据（从 snapshot）
  └── Phase 2: 返回无快照候选名单（零删除、零账号变更）
```

> **安全边界**：候选名单只能用于人工核对，调用方不得把它解释成“已经清理”。

---

## 3. 整 Zone 灾难恢复级回档（Ops 操作）

### 架构前提

每个 zone 在 K8s 中是独立 namespace（`mmorpg-zone-{zoneName}`），拥有独立的：
- MySQL 实例（数据库名 `game`）
- Redis 实例（`--appendonly yes`）
- Kafka broker / topic（`db_task_topic`）

写路径：`C++ node → Kafka(db_task_topic) → Go db service → MySQL`，Redis 仅为缓存加速层。

### 操作步骤

```
步骤 1: 关闭 zone（停止所有业务写入）
─────────────────────────────────────
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-down -ZoneName <zone>

步骤 2: MySQL Point-in-Time Recovery (PITR)
────────────────────────────────────────────
# 先恢复到影子库验证
mysqlbinlog --stop-datetime="2026-03-26 10:00:00" binlog.000xxx | mysql -h shadow-host -u root -p game

# 确认无误后替换正式库
# 方式 A: 云 RDS 控制台一键 PITR
# 方式 B: 自建 MySQL 用 binlog 手动恢复

步骤 3: Redis 处理
──────────────────
# Redis 是缓存层，C++ 节点启动时会从 MySQL 重建，直接清空即可
redis-cli -h <redis-host> FLUSHDB
# ⚠ 2026-10-12 补:K8s 下各 zone 的玩家缓存与 mapping Redis(player:zone / player:placement / merge:*)、
#   go/db 的游标与重试收据同在共享实例的 DB 0,这里的 FLUSHDB 会清掉**所有 zone** 的这些全局键,
#   不只是本 zone 缓存。k8s_zone_rollback.ps1 已加防护:目标 DB 里有这些键就在停服之前拒绝,
#   除非显式传 -ConfirmFlushSharedRedis(dev_tools 为 -RollbackConfirmFlushSharedRedis)。
#   映射丢失后的恢复见 docs/ops/merge-zone-runbook.md §9.2。

# 或从 AOF/RDB 备份恢复到对应时间点（如果有归档的话）

步骤 4: Kafka 处理
──────────────────
# 清空 db_task_topic，防止回档时间点之后的写入消息被重放
# 方式 A: 删除并重建 topic
kafka-topics.sh --delete --topic db_task_topic --bootstrap-server <broker>
kafka-topics.sh --create --topic db_task_topic --partitions 5 --bootstrap-server <broker>

# 方式 B: 重置 consumer group offset 到指定时间戳
# (2026-10-15 更正:组名是本 zone 的 db_rpc_consumer_group_z<id>,topic 是 db_task_zone_<id>,TopicGeneration ≥ 2 时
#  带 _g<gen> 后缀;重置之后还要删掉 legacy 组 db_rpc_consumer_group 在本 topic 上的位点。一键脚本的做法见下方「2026-10-15 修订」)
kafka-consumer-groups.sh --reset-offsets --to-datetime 2026-03-26T10:00:00.000 \
  --group db_rpc_consumer_group_z<id> --topic db_task_zone_<id> --bootstrap-server <broker> --execute

步骤 5: 重新拉起 zone
─────────────────────
pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-up \
  -ZoneName <zone> -ZoneId <id> -NodeImage <image> -WaitReady
```

> **(2026-09-29 更正)步骤 5 的命令不完整,且整套手工步骤已由一键脚本取代**:部署参数不粘滞,`k8s-zone-up` 每次按本次参数重新生成,
> 不从集群读回旧值。只写上面这行会把一个以 `-GateRouterMode 0` 回退、以 `-ClientEntryMode external` 对外、经 Ingress 暴露或 staging / prod 档运行的 zone
> 静默重建成默认形态(路由模式 `"1"`、podip、无 Ingress、dev 档)。重新拉起时必须照传上一次部署的同一组参数,清单与预检见下方「2026-09-29 修订」。
> `-NodeImage` 必须是**回滚目标版本**的不可变 tag,不是当前工作树的版本。

### 关键安全点

| 要点 | 说明 |
|------|------|
| Redis 可安全清空 | 它是缓存层，节点启动后从 MySQL 重建 |
| Kafka 必须处理 | 不清空 topic/offset 会导致旧消息重放覆盖回档数据 |
| MySQL 必须先恢复 | 它是唯一的持久化真相源（source of truth） |
| 串行保证 | Kafka partition key = `player_id`，同玩家写入串行执行 |

### 2026-09-29 修订:第 0 步预检与不粘滞的部署参数

一键入口是 `tools/scripts/k8s_zone_rollback.ps1`(`dev_tools.ps1 -Command k8s-zone-rollback` 包装,默认 dry-run,`-Apply` 才执行)。
原先的 7 步(zone-down / Kafka drain / MySQL PITR 暂停 / Redis FLUSHDB / offset reset / zone-up / 验证清单)前面**新增了第 0 步**
(脚本头 `.DESCRIPTION`,`k8s_zone_rollback.ps1:15-27`;Step 6 的不粘滞说明在 `:34-48`)。起因:K8s 默认 `-GateRouterMode` 已翻为 `"1"`(turn-based §22 D75),集群外入口参数也不粘滞;
而 Step 1 会删掉整个 zone namespace,`k8s_deploy.ps1` 自己的集群现状预检到 Step 6 面对的是空 namespace,一条都触发不了。所以核对前移到停服之前。

**第 0 步(只读,先于停服;dry-run 只预告不执行),两部分都过才进 Step 1**:

1. **集群现状核对**(`-Apply` 且下列任一项留空时,一次 `kubectl` 读 zone namespace 里的 gate —— StatefulSet 或 Deployment 两种形态都认 —— 与 Ingress `gateway`):
   - `-GateRouterMode` 留空:gate 模板当前的 `GATE_CLIENT_RPC_ROUTER` 必须等于 `k8s_deploy.ps1` 的默认值(按语法树读出,现为 `"1"`)。
     以 `"0"` 回退运行的 zone **必须显式传 `-GateRouterMode 0`**,否则在停服之前被拒(以前会被 Step 6 静默切回 `"1"`,若路由服仍不可用,gate 卡在依赖门,登录 / 匹配全部 `no_target`)。
   - `-ClientEntryMode` 留空:gate 当前形态(StatefulSet = external,Deployment = podip)必须等于默认 `podip`。以 external 运行的 zone 必须显式传 `-ClientEntryMode external`。
   - `-GatewayIngressHost` 与 `-ConfirmNoGatewayIngress` 都没给:namespace 里不能有 Ingress `gateway`(Step 1 会连它一起删、Step 6 留空不重建),且必须读得到。
   - 不一致或判不出(集群不可达、Forbidden、gate 用 valueFrom 注入)一律在停服之前拒绝,要求显式传参;显式传了的项不核对。
   - **上一次回滚中途放弃、namespace 已被删掉时重跑**:一个 gate 都读不到,三项都判为读不到。必须显式传 `-GateRouterMode` 与 `-ClientEntryMode`,
     并对 Ingress 表态:照传 `-GatewayIngressHost`(及 trusted proxies),或加 `-ConfirmNoGatewayIngress`(`dev_tools.ps1` 侧为 `-RollbackConfirmNoGatewayIngress`)。
   - 地址参数与 `-RequireClientEndpoint` **不逐项核对**:预检只把 gate StatefulSet 现有的地址 env 与 Ingress host 打印出来,供操作者对照部署记录。
2. **静态预检**:把 Step 6 将要传给 `dev_tools.ps1 k8s-zone-up` 的同一份参数加 `-DryRun` 执行一遍(`k8s_deploy.ps1 -DryRun` 只本地渲染、不调 kubectl),非 0 即拒绝。
   参数组合、档位与密钥、发布门禁的规则只在 `k8s_deploy.ps1` 一处,这里不复制;拼错或矛盾的组合在停服之前暴露。要求 PATH 上有 git;staging / prod 档工作树脏会在这里被拒(与 Step 6 一致,只是提前)。

**回滚时必须照传的参数(与该 zone 上一次部署一致)**:

| 场景 | 参数 |
|---|---|
| 以 `"0"` 回退运行 | `-GateRouterMode 0`(回退态不粘滞,见 `k8s_deploy.ps1:157-160` 参数注释;D-12 修订的"回退"条款) |
| 以 external 对外 | `-ClientEntryMode external`、`-GateServiceType`、`-GateNodePortBase`、`-ClientPublicHost` 或 `-GateClientHostTemplate`、`-GateExternalTrafficPolicy`;逐个 zone 切 external 的窗口里还有 `-RequireClientEndpoint` |
| 经 Ingress 对外 | `-GatewayIngressHost`、`-GatewayTrustedProxies`(及 `-GatewayIngressClassName` / `-GatewayIngressTlsSecret`) |
| staging / prod | `-ReleaseProfile`(**留空 = 按 dev 档部署**:login 以 dev 模式、密钥回落占位值拉起,安全方向 fail-open);部署 Java 服务时还需环境变量 `MMORPG_GATEWAY_ADMIN_API_KEY`(Step 6 子进程继承);staging / prod 还需要 `MMORPG_LOGIN_QUEUE_TOKEN_SECRET`(排队缺省开;见 `deploy/k8s/README.md`)(2026-10-15 更正:登录排队合入门槛未清时各档排队都关,不要求该变量;见 `deploy/k8s/README.md`「登录排队的合入门槛」) |
| dev 依赖开发口令登录 | `-LoginDevPasswordAuth`(照常透传到 Step 6,由 `k8s_deploy.ps1` 的 dev 档门禁把关;漏传则 login 没有任何认证配置,Step 7 冒烟登录失败) |
| 指定集群 | `-KubeContext` / `-KubeConfig`:第 0 步、Step 1、Step 6 用同一集群。`dev_tools.ps1` 的回滚入口以前静默丢掉它们、回滚落到 kubectl 当前 context,现已转发 |

- `-AllowDisruptiveSwitch` 只转给 Step 6,**不豁免第 0 步**;它只在 namespace 没删净、`k8s_deploy.ps1` 的集群现状预检真的触发时起作用。
- `dev_tools.ps1` 的回滚入口不转发 `-BattleOrchestrator`(zone 回滚不碰 battle 全局池);`dev_tools.ps1:1592-1617` 列出了全部转发项。
- `k8s-release-*` 也接受 `-RequireClientEndpoint`,但经那条路只参与预检、不改写 login / scene_manager 的 ConfigMap。
- 命令示例见 `k8s_zone_rollback.ps1` 头部 `.EXAMPLE`(含 `"0"` 回退与 external + Ingress 的 staging / prod 两例)。

**仍未解决(已登记为独立任务,本批不做)**:Step 6 的 `GoSvcTag` / `JavaSvcTag` 取当前工作树的 sha,不是回滚目标版本;`OpsProfile`、`GateReplicas`、
`SceneReplicas` 等不透传;`-ReleaseProfile` 留空仍按 dev 档部署,只有摘要行提醒,没有与集群核对。

**验证状态**:以上全部**未运行**(未编译、未测试,待 Codex 验证)。Codex 在仓库根执行
`pwsh -NoProfile -File tools/scripts/tests/k8s_zone_rollback_gate_router_mode.tests.ps1`(需要 PATH 上有 git),再跑 `k8s_deploy_contract.tests.ps1`、
`k8s_client_entry_contract.tests.ps1`、`dev_tools_merge_zone_contract.tests.ps1`,通过标准 `fail=0`,失败时保留全部 `[FAIL]` 行。
DryRun 冒烟:`pwsh -File tools/scripts/dev_tools.ps1 -Command k8s-zone-rollback -ZoneName e2e -ZoneId 101 -RollbackTargetTime 2026-05-15T14:23:00Z -NodeImage registry.invalid/test/mmorpg-node:0123456789ab -KubeContext kind-mmorpg -LoginDevPasswordAuth`,
期望 Step 1、预检 kubectl、Step 6 三行都带上 context,Step 6 还带 `-LoginDevPasswordAuth`。

### 2026-10-15 修订:存盘消费组按 zone 拆分后的 Kafka 步骤(删 Step 2、第 0 步 c / d、Step 5 退役 legacy 位点、Step 6 落到 final(Z))

**状态:已落码,未编译、未测试,待 Codex 验证。** 本节按 2026-10-16 工作区的代码写(`tools/scripts/k8s_zone_rollback.ps1`、`tools/scripts/kafka_offset_reset.ps1`、
`tools/scripts/lib/kafka_consumer_groups.ps1`、`tools/scripts/k8s_deploy.ps1`、`tools/scripts/dev_tools.ps1`),与规格不一致处以代码为准,见本节末尾。
设计见 [db-consumer-group-per-zone.md](./db-consumer-group-per-zone.md)(§5 收尾、§6 回档、§11 未核实项)。上文「2026-09-29 修订」里「原先的 7 步」
含 Kafka drain、第 0 步只有两部分,这两处以本节为准(脚本头 `.DESCRIPTION` 已同步改写)。

起因:go/db 的存盘消费组从 K8s 上全 zone 共用的 `db_rpc_consumer_group`(下称 legacy 组)逐 zone 换成 `db_rpc_consumer_group_z<ZoneId>`(下称 zone 组)。
旧脚本 Step 5 重置的是 legacy 组:只要别的 zone 在线,这个组就不是 Empty,重置一定被 Kafka 拒绝。旧 Step 2 在 zone-down 之后等 LAG 排空,
可本 zone 的 db 已经停了,必然等满超时再 `proceeding anyway`。

**组名与分区数**:脚本入口用 `Get-YamlScalar` 现读 `go/db/etc/db.yaml` 的 `ServerConfig.Kafka.GroupID`(基名)与 `ServerConfig.Kafka.PartitionCnt`(必须是正整数),
读不到即拒绝,此时尚未产生任何改动。`-KafkaGroup` 留空推导为 `<基名>_z<ZoneId>`;新增 `-KafkaLegacyGroup`,留空推导为 `<基名>`;两者相同(区分大小写比较)入口即拒。
摘要行 `kafka` 一栏变为 `<bootstrap> topic=<topic> group=<zone 组> retire=<legacy 组> partitions=<N>`。

**第 0 步由两部分扩为 a–d**(上文「第 0 步」的 1 / 2 即脚本里的 a / b)。a、b、d 都过才进 Step 1,c 只告警:

- **c. Kafka LAG 只读快照**(`Show-KafkaTopicLagSnapshot`):只在 `-Apply` 下执行,dry-run 只打印两条 `kafka-consumer-groups.sh --describe --group <组>` 命令。
  - 对 zone 组与 legacy 组各 describe 一次,只打印本 topic 的分区 / current / end / lag 与 LAG 合计,别的 topic 的行不计入;组不存在时打印 `(group does not exist)`。
  - 任何失败只 `Write-Warning`,绝不拒绝。它取代原 Step 2,用途只是停服前看一眼两组在本 topic 上的进度。
- **d. Step 5 预演**(`Invoke-Step5Rehearsal`):只在 `-Apply` 下、Step 1 之前执行。
  - 先 describe zone 组。它在本 topic 上已有数值位点就跳过,打印 `zone group already holds offsets on <topic>; Step 5 rehearsal skipped`。
  - 组不存在或没有位点(从未切过组的 zone)时,以 hashtable splatting 调 `kafka_offset_reset.ps1 -BootstrapServer … -Topic … -Group <zone 组> -ToDatetime <TargetTime> -Partitions <N>`,
    即 dry-run,不带 `-Apply` 与 `-RetireGroup`。计划不完整、抛错或退出码非 0,都在停服之前拒绝。
  - describe 本身失败同样拒绝:判不出就无法证明 Step 5 走得通。
  - 已知局限(未处理):zone 组在本 topic 上没有已提交位点、却仍有活跃成员时(例如以 final(Z) 运行、但从未写入过的 zone),dry-run 重置会因组不是 Empty 被 Kafka 拒绝,回档在停服前被拒。
- c / d 直接调用 PATH 上的 `kafka-consumer-groups.sh`(本脚本没有 `-KafkaBin`),执行机必须装有 Kafka CLI 且连得到 `-KafkaBootstrap`。

**Step 2 已删除**,编号 3–7 不变。`k8s_zone_rollback.ps1` 的 `-KafkaDrainTimeoutSec` 与 `dev_tools.ps1` 的 `-RollbackKafkaDrainTimeoutSec` 一并删除。
理由:Step 5 本来就把位点拨回 TargetTime,排空没有意义。

**Step 5:重置 zone 组,复核通过后退役 legacy 组在本 topic 上的位点。** 脚本以 hashtable splatting 调用
`kafka_offset_reset.ps1 -BootstrapServer <b> -Topic db_task_zone_<id> -Group db_rpc_consumer_group_z<id> -RetireGroup db_rpc_consumer_group -ToDatetime <TargetTime> -Partitions <N> [-Apply]`:

1. 解析 `--reset-offsets` 输出的 GROUP / TOPIC / PARTITION / NEW-OFFSET 计划表(`ConvertFrom-KafkaResetPlan`)。计划必须恰好覆盖 0..N-1,NEW-OFFSET 都是数值;dry-run 与 `-Apply` 都检查。
2. `-Apply` 时,`--execute` 之后再 describe zone 组,逐分区断言 CURRENT-OFFSET 等于计划的 NEW-OFFSET;不符即抛出「重置后复核失败,没有删除 db_rpc_consumer_group 的位点」。
   这个抛出在删除之前。
3. 复核通过才执行 `Remove-RetiredGroupTopicOffsets`:
   - legacy 组在本 topic 上没有数值位点时打印 `nothing to retire`(幂等,可重跑);
   - 否则执行 `kafka-consumer-groups.sh --delete-offsets --group db_rpc_consumer_group --topic db_task_zone_<id>`,输出含 `GROUP_SUBSCRIBED_TO_TOPIC` 即抛出;删完再 describe 一次,仍有数值位点也抛出。
   - 回档是有意回退,这条路不做「新组 ≥ 旧组」检查(那是 `-FinalizeHandoff` 的前置条件)。

- legacy 组即使还有别的 zone 的成员,只要没有成员订阅本 topic,broker 就允许删除。这一点是推断,待 Codex 在钉版的 Kafka 4.3.1 上实测(设计文档 §11 V3(d))。
- 仍有订阅者时(本 zone 的 namespace 没删干净、留有 db 实例),Step 5 失败。这时 zone 已停、MySQL 已回档、Redis 已清:清掉残留实例后,**单独重跑脚本打印的那条
  `kafka_offset_reset.ps1` 命令**(带 `-Apply`;重跑时旧组若已没有本 topic 的位点,退役一步打印 `nothing to retire`),再单独跑 Step 6。
- `-RetireGroup` 必须配 `-Partitions > 0`,不能等于 `-Group`,不能与 `-DeleteAndRecreateTopic` 同用,违反任一条即在入口拒绝。

**Step 6:显式 `-DbConsumerGroup zone`,真跑时落到最终形状 final(Z)。** `$upArgs.DbConsumerGroup = 'zone'`,第 0 步 b 的静态预检用的是同一份参数。

- 真跑时,`k8s_deploy.ps1` 在写路径入口(`Initialize-DbConsumerGroupPlans`)kubectl exec 进 infra namespace 的 `kafka-0`,读两组在本 topic 上的位点(`Read-DbGroupTopicFacts`)。
  命令经 `/bin/sh -c "KAFKA_HEAP_OPTS='-Xms16m -Xmx64m' /opt/kafka/bin/kafka-consumer-groups.sh --bootstrap-server localhost:9092 --describe --group <组>"` 执行,
  避免 CLI 继承 broker 的大堆参数、把 kafka-0 撑到 OOMKilled。
- Step 5 之后 zone 组有位点、legacy 组在本 topic 上没有,`Resolve-DbConsumerGroupPlan` 落到 `Shape=final`、`Source=explicit-already-authoritative`:
  db ConfigMap 只写 `GroupID: "db_rpc_consumer_group_z<id>"`,不带 `GroupHandoffFrom`,交接闸之前的旧镜像也能起。
  db Pod 模板注解 `mmorpg.io/db-consumer-group` 为 zone 组,`mmorpg.io/db-group-handoff-from` 为 `none`。
- 第 0 步 b 的静态预检带 `-DryRun`、不读集群,渲染的是交接形状的预览(`ConsumerGroup` + `GroupHandoffFrom`,`Source=dryrun-preview`,另有一行告警)。它只校验参数组合,不代表真跑的形状。
- **回档之后,该 zone 一律以 zone 组为权威**:仍在 legacy 组上的 zone 借回档迁到 zone 组。这是行为变化。
- 剩余风险:Kafka 位点直到 Step 6 才读,第 0 步不读。部署账号缺 `pods/exec` 权限或 kafka-0 未就绪时,`k8s_deploy.ps1` 会在任何写操作之前拒绝,但这时 zone 已停服、数据已回档;
  修好之后,按脚本打印的 Step 6 那一行单独重跑 `dev_tools.ps1 -Command k8s-zone-up … -DbConsumerGroup zone`。
- Step 5 的退役没做成(两组在本 topic 上都有位点)时,显式 zone 模式同样拒绝,并打印两种 `-FinalizeHandoff` 收尾命令。回档场景下应重跑 Step 5,不要照那条提示收尾。
- Step 6 必须部署 db:`dev_tools.ps1` 的 `-GoSvcRegistry` 缺省非空。显式 `-DbConsumerGroup` 配上不部署 db 的运行(没给 `-GoSvcRegistry`,或带了 `-SkipGoSvc`)时,`k8s_deploy.ps1` 在写路径入口拒绝。

**`dev_tools.ps1` 入口**:
- `-RollbackKafkaGroup` 缺省改为空串(由回滚脚本推导);新增 `-RollbackKafkaLegacyGroup`。两者都只在非空时转发。
- `k8s-zone-rollback` 显式给了 `-DbConsumerGroup` 就在调用回滚脚本之前拒绝,因为 Step 6 固定用 zone。
- `kafka-offset-reset` 分支改为 hashtable splatting,新增 `-KafkaRetireGroup` / `-KafkaFinalizeHandoff` 透传(都要配 `-KafkaPartitions > 0`)。

**与规格的差异(以代码为准)**:
- 原规格写「新增脚本常量 `$DbConsumerGroupBase`」;代码改为入口现读 db.yaml,没有这个常量。
- 原规格写 Step 6 渲染 zone 的交接形状(`ConsumerGroup` + `GroupHandoffFrom`);代码按修订稿渲染最终形状 final(Z),只有 `-DryRun` 预览才是交接形状。
- 原规格只有第 0 步 c;代码按修订稿另加第 0 步 d(Step 5 预演),并要求 `-Partitions` 参与计划覆盖与 `-Apply` 后复核。
- 原规格风险 8 写「漏删 legacy 位点时由 go/db 的 R7 拦住」;final(Z) 下 go/db 不配 `GroupHandoffFrom`、没有交接闸,实际兜底是 `k8s_deploy.ps1` 显式模式的「两组都有位点即拒绝」。

**未核实项**(待 Codex 在 `apache/kafka:4.3.1` 上实测,设计文档 §11):
- 对不存在的组执行 `--reset-offsets --to-datetime --execute` 能否建出位点(V3(c));
- 组 Stable、但无人订阅本 topic 时 `--delete-offsets` 能否成功(V3(d));
- 不存在组 / Empty 组 describe 的原文,以及 `--execute` 是否打印 NEW-OFFSET 表(V3(b)(f)(h)(i))。

脚本按推定的文案 fail-closed。若 V3(c) 证实建不出位点,回档改动不得发布,交负责人决策。

**Codex 验证**(Claude 未执行):在仓库根逐个执行 `pwsh -NoProfile -NonInteractive -File tools/scripts/tests/<名>.tests.ps1`,名字为
`k8s_zone_rollback_gate_router_mode`、`k8s_zone_rollback_flush_guard`、`kafka_offset_reset_output_guard`、`dev_tools_kafka_offset_reset`、`dev_tools_merge_zone_contract`、
`k8s_deploy_contract`、`k8s_client_entry_contract`,通过标准 `fail=0`,失败时保留全部 `[FAIL]` 行。dry-run 冒烟:
`pwsh -NoProfile -File tools/scripts/k8s_zone_rollback.ps1 -ZoneName t1 -ZoneId 101 -TargetTime 2026-05-15T14:23:00Z -NodeImage ghcr.io/luyuancpp/mmorpg-node:0123456789ab -SkipMySqlPause`,
期望摘要含 `group=db_rpc_consumer_group_z101 retire=db_rpc_consumer_group partitions=10`,Step 5 行含 `-RetireGroup db_rpc_consumer_group` 与 `-Partitions 10`,
Step 6 行含 `-DbConsumerGroup zone`,输出里没有 Step 2 的排空等待。

---

## 3. 当前缺失基建

| 缺失项 | 状态 | 说明 | 优先级 |
|--------|------|------|--------|
| MySQL 定时备份 + binlog 归档 | ✅ **已落地 (2026-05-15)** | `deploy/k8s/manifests/infra/mysql.yaml`(PVC + binlog on)+ `mysql-backup-cronjob.yaml`(每天 03:17 UTC dump + binlog 复制),SOP 见 [`docs/ops/mysql-backup-pitr-runbook.md`](../ops/mysql-backup-pitr-runbook.md) | ~~高~~ |
| Redis RDB/AOF 定期快照归档 | 未做 | 当前 `--appendonly yes` 保证持久化，但无历史时间点恢复能力 | 低（可 FLUSHDB） |
| Kafka offset 回档工具 | ✅ **已落地 (2026-05-15)** | `tools/scripts/kafka_offset_reset.ps1`(支持 to-datetime / to-earliest / to-latest / delete-and-recreate-topic 4 种模式,默认 dry-run),注册为 `dev_tools.ps1 -Command kafka-offset-reset` | ~~中~~ |
| 一键 zone 回档脚本 | ✅ **已落地 (2026-05-15)** | `tools/scripts/k8s_zone_rollback.ps1`(7 步:zone-down / Kafka drain / MySQL PITR 提示暂停 / Redis FLUSHDB / kafka-offset-reset / zone-up / 验证清单),注册为 `dev_tools.ps1 -Command k8s-zone-rollback`,默认 dry-run。**2026-09-29 更正**:现为「第 0 步预检 + 7 步」,见上文「## 3. 整 Zone 灾难恢复级回档」下的「2026-09-29 修订:第 0 步预检与不粘滞的部署参数」(本文有两个「## 3.」标题,指的是前一个)。**2026-10-15 更正**:Step 2(Kafka drain)已删除,第 0 步增加 c(LAG 只读快照)/ d(Step 5 预演),Step 5 重置 zone 组 `db_rpc_consumer_group_z<id>` 并退役 legacy 组在本 topic 上的位点,Step 6 落到 final(Z),见同处「2026-10-15 修订」(已落码,未编译、未测试,待 Codex 验证) | ~~中~~ |
| 备份异地归档(S3/OSS) | 未做 | PVC 只防 pod 重启,不防整个集群 / 机房挂 | 高(生产前必做) |
| 跨服务离线 epoch 回档栅栏 | 未做（应用级回档 fail-closed） | login/player_locator 阻断新会话，Scene 排空存盘，Kafka/db 携带并校验 epoch | P0（重启应用级回档前） |

---

## 4. 生产环境建议

- **云 MySQL（RDS）**：自带自动备份和 PITR，回档操作在控制台点几下即可，**强烈推荐**
- **自建 MySQL**：必须配置 `log-bin`、定期 `mysqldump` CronJob、binlog 归档到对象存储
- **Redis**：zone 回档时直接 FLUSHDB，无需额外备份
- **Kafka**：回档后最安全的做法是清空 `db_task_topic`，避免旧消息重放
