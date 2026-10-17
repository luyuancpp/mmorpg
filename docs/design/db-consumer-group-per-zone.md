# go/db 存盘消费组按 zone 拆分与不停服交接

> **状态**：已落码，未编译、未测试，待 Codex 验证（截至 2026-10-16 工作区）。Kafka CLI 在钉版 4.3.1 上的行为（§11）同样没有实测。
> **范围**：交接（handoff）里的 ③「消费组按 zone 拆」，外加它的前置「Kafka 镜像钉版本」和回档 Step 2 的修正。
> **依据**：`docs/handoff/godb-pipeline-impl-20261015/` 下的 S2 规格（`S2-consumer-group-per-zone-spec.md`）、复核（`…-review.md`）、修订稿（`…-addendum.md`，优先级最高），以及 `batch-reports.md` 的 A2 / B1 / B2 / B3 各节。接手方在落码时定下的事实以代码为准。**代码与规格冲突时以代码为准**，冲突处在正文里标「与规格的差异」，汇总见 §10.6。
> **证据口径**：全部来自静态阅读代码，没有编译、没有运行，也没有连过集群。
> **相关文档**：[瓶颈文档](db-task-pipeline-bottlenecks.md) §4.4（G6 停机顺序）与 §5.4（③ 要点）；[回档方案](zone_data_rollback.md)；[合服手册](../ops/merge-zone-runbook.md)；[分区契约](db-task-kafka-partition-contract.md)（①）；`deploy/k8s/README.md` 的「镜像钉版本」和「go/db 存盘管线告警」两节；[上线待做](../ops/godb-pipeline-launch-todo.md)。

## 0. 结论与拍板

**结论**

- 改动前，K8s 上所有 zone 的 go/db 共用一个消费组 `db_rpc_consumer_group`。sarama 用的是 eager 再平衡，任何一个 zone 的 db Pod 有变化（发布、崩溃、驱逐），所有 zone 的存盘消费都会一起暂停。改动后每个 zone 用自己的组 `db_rpc_consumer_group_z<ZoneId>`，切换不停服。
- 切换不能直接改组名。新组没有已提交位点，`Offsets.Initial = OffsetOldest` 会让它从保留期起点全量重放；游标缺键时旧写还会被放行。所以 go/db 里加了一道**交接闸**：入组前判定一次；对方组释放本 topic 之后，用对方组的已提交位点给本组播种；守望只凭正面证据让出；另有 R7（本组位点落后于对方时拒绝放行）。
- ConfigMap 收敛成**四种形状**。最终形状只写 `GroupID`，任何二进制都能加载；交接形状只写 `ConsumerGroup` 加 `GroupHandoffFrom`，不写 `GroupID`。交接闸之前的二进制把 `GroupID` 当必填，遇到交接形状在 `conf.MustLoad` 就拒启，所以「镜像带交接闸」的证明来自二进制本身。
- 部署脚本 `k8s_deploy.ps1` 新增 `-DbConsumerGroup keep|zone|legacy`，按「线上 ConfigMap + Kafka 位点事实」查决策表（§2.3）。回档 Step 6、全新 namespace、收尾之后的 keep，永远只渲染最终形状。
- 收尾（`kafka_offset_reset.ps1 -FinalizeHandoff`）删掉旧组在本 topic 上的位点，之后再以 keep 跑一次 zone-up，自动去掉 `GroupHandoffFrom`。到这一步，进程里不再有交接闸，也不再依赖 legacy 组。
- 回档一律落到 zone 组，合服只认唯一的权威组。

**拍板与前置**

- **授权**：负责人 2026-10-13 授权 ① 和 ③，并定 ③ 为**开服门槛**（开服要支持单 zone 回档或合服）。出处：`docs/PROGRESS.md` 2026-10-13「§5 拍板记录」第 3、15 条；`docs/handoff/godb-pipeline-impl-handoff-20261015.md`。
- **前置：Kafka 镜像钉版本（已落码）**。四处统一为 `apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837`：`deploy/docker-compose.yml` 的 `kafka` 与 `kafka-topic-init`、`deploy/k8s/manifests/infra/kafka.yaml`（同时加了 `imagePullPolicy: IfNotPresent`）、`deploy/k8s/manifests/infra/kafka-topic-init.yaml`。理由、V0 核对命令和升级步骤见 `deploy/k8s/README.md`「镜像钉版本(2026-10-15)」。
- **规格待决项按推荐默认定**：
  1. 按 4 个 PR 拆分：A 钉版，B go/db，C 脚本与工具，D 文档。
  2. 钉 tag@digest；拉镜像做 V3 核对属于下载，执行前要负责人确认。
  3. 偏离附录的地方写进 §10。修订稿改写过这一部分，以 §10 为准。

## 1. 现状

### 1.1 改动前的问题

- K8s 上 `k8s_deploy.ps1` 给 db ConfigMap 写的是同一个字面量 `GroupID: "db_rpc_consumer_group"`，每个 zone 都一样。共享组的后果：
  - 任一 zone 再平衡，全服存盘都暂停。
  - ④ 的 owner 模式里，任一 zone 的再平衡都会在全服引发重试撤回尖峰（瓶颈文档 §5.2）。
- 回档 `k8s_zone_rollback.ps1` 有两处问题：
  - Step 5 重置的是 legacy 组。只要还有别的 zone 在线，这个组就不是 Empty，重置一定被 Kafka 拒绝。
  - Step 2 在 zone-down 之后等 LAG 排空。此时本 zone 的 db 已经停了，必然等满超时。
- 合服 `tools/merge_zone` 的 P3 门禁只查单个组，而且缺省就是 legacy 组。
- Kafka 镜像用的是浮动 tag `apache/kafka:latest`。

### 1.2 本地组名的现状（修订稿 R10 更正）

本地组名规则**没有改**，`go_services.ps1` 不在本项改动范围内：

- **单 zone 默认启动**（`start_game` / `dev-start` / `go-svc-start` 都不带 `-Zone`）：组名是裸名 `db_rpc_consumer_group`，topic 用 `go/db/etc/db.yaml` 的 `ZoneId: 1`，即 `db_task_zone_1`。
- **`-Zone N` 启动**（`dev-start-zones`）：`tools/scripts/go_services.ps1` 的 `if ($Zone -gt 0)` 块用正则 `$groupRegex` 给带引号的 `GroupID:` 行追加 `_z<N>`，组名变成 `db_rpc_consumer_group_zN`，topic 是 `db_task_zone_N`。

原规格的现状描述有两句与此不符，都只对 `dev-start-zones` 成立：「本地与压测一直是每 zone 一组」「本地合服门禁一直在查不存在的组」。

### 1.3 改动后的代码位置

| 职责 | 文件 | 符号 |
|---|---|---|
| 配置键、组合校验 | `go/db/internal/config/config.go` | `KafkaConfig.GroupID / ConsumerGroup / GroupHandoffFrom / SessionTimeoutMs / HeartbeatIntervalMs / RebalanceTimeoutMs`、`EffectiveGroupID`、`ValidateConsumerGroup`、`DefaultKafka*`、`MinKafkaRebalanceTimeoutMs` |
| 交接闸 | `go/db/internal/kafka/group_handoff.go` | `groupInspector`、`saramaGroupInspector`、`groupHandoff`（`await` / `evaluate` / `decideGate` / `seed` / `watch` / `watchOnce` / `yield` / `withInspector`）、`consumerClientID`、`rebalanceGapTracker`、`consumerKafkaVersion` |
| 消费者挂点 | `go/db/internal/kafka/key_ordered_consumer.go` | `NewKeyOrderedKafkaConsumer`、`Start` 的消费循环、`consumerGroupHandler.Setup / Cleanup / ConsumeClaim / sessionScope / yielded`、`closeHandoff`、`stop` |
| 位点读取（与 lag 探针共用） | `go/shared/kafkautil/group_lag.go` | `CommittedOffsetsReader`、`CommittedOffsets` |
| 指标 | `go/db/internal/metrics/metrics.go` | `consumerGroup*` 六个指标、`HandoffSeed*` / `HandoffGate*` 常量、`ObserveRebalanceGap` 等导出函数 |
| 告警 | `deploy/k8s/db-task-retry-alerts.yaml` | 组 `db-task.consumer-group-handoff` |
| 部署 | `tools/scripts/k8s_deploy.ps1` | `-DbConsumerGroup`、`ConvertFrom-LiveDbConsumerGroup`、`Test-DbConsumerGroupNeedsKafkaFacts`、`Resolve-DbConsumerGroupPlan`、`New-DbConsumerGroupYaml`、`Get-DbConsumerGroupPlan`、`Read-LiveDbConfigMapYaml`、`Read-DbGroupTopicFacts`、`Initialize-DbConsumerGroupPlans`、`Test-ZoneDbDeployed`、`Assert-DbConsumerGroupName`、`Apply-OneGoSvc` 的 `if ($SvcName -eq 'db')` 块 |
| Pod 注解占位 | `deploy/k8s/manifests/go-svc/db.yaml` | `mmorpg.io/db-consumer-group: "PLACEHOLDER_DB_CONSUMER_GROUP"`、`mmorpg.io/db-group-handoff-from: "PLACEHOLDER_DB_GROUP_HANDOFF_FROM"` |
| CLI 输出解析（纯函数库） | `tools/scripts/lib/kafka_consumer_groups.ps1` | `Get-KafkaGroupMissingPatterns`、`Test-KafkaGroupMissingText`、`Test-KafkaGroupNoActiveMembersText`、`ConvertFrom-KafkaConsumerGroupDescribe`、`Get-KafkaCommittedOffsetMap`、`Resolve-KafkaGroupTopicOffsets`、`ConvertFrom-KafkaResetPlan`、`ConvertFrom-KafkaGetOffsets`、`Test-HandoffFinalizeReady`、`Get-DbTaskTopicName` |
| 收尾与退役 | `tools/scripts/kafka_offset_reset.ps1` | `-FinalizeHandoff`、`-RetireGroup`、`Remove-RetiredGroupTopicOffsets`、`Get-ResetPlanProblems`、`Get-ResetVerificationProblems` |
| 回档 | `tools/scripts/k8s_zone_rollback.ps1` | `-KafkaGroup` / `-KafkaLegacyGroup`、`Show-KafkaTopicLagSnapshot`（第 0 步 c）、`Invoke-Step5Rehearsal`（第 0 步 d）、Step 5 的 `$kArgs`、`$upArgs.DbConsumerGroup` |
| 本地入口 | `tools/scripts/dev_tools.ps1` | `-DbConsumerGroup`、`-KafkaRetireGroup`、`-KafkaFinalizeHandoff`、`-RollbackKafkaGroup`、`-RollbackKafkaLegacyGroup`、`-MergeKafkaGroup` |
| 合服 | `tools/merge_zone/main.go`、`preflight.go`、`merge_run.go`、`unmerge.go` | `defaultKafkaGroup`、`-kafka-group`、`-kafka-legacy-group`、`resolveAuthoritativeLag`、`committedOffsetPresent`、`consumerGroupMissingPatterns`、`kafkaGroupCandidates`、`dbConsumerGroupForZone` |
| 压测汇总 | `tools/scripts/stress_summarize.ps1` | `-Zone`（缺省 0） |
| login 的死配置 | `go/login/internal/config/config.go` | `KafkaConfig.GroupID` 改为 `json:"GroupID,optional"`，注明已废弃 |

## 2. 方案

### 2.1 组名推导

记号：

- **L** = `go/db/etc/db.yaml` 里 `ServerConfig.Kafka.GroupID` 的值，即 `db_rpc_consumer_group`。它有两个身份：legacy 共享组，以及所有推导组名的**基名**。
  - `TestEtcYamlConsumerGroupIsFinalShape`（`go/db/internal/config/config_test.go`）钉住 db.yaml 里是裸名、最终形状。
  - `go/db/etc/db.yaml` 的 GroupID 行必须保持 `GroupID: "<名>" # 注释` 这个形状，因为 `go_services.ps1` 的正则和 `dev_tools.ps1` 的 `Get-MergeDbKafka` 都按这个形状匹配。
- **Z** = L + `_z` + ZoneId，例如 `db_rpc_consumer_group_z101`。
- **T** = 本 zone 的 db_task topic。世代号 ≤1 时是 `db_task_zone_<id>`，否则是 `db_task_zone_<id>_g<gen>`。规则与 `go/db/internal/config/config.go` 的 `DbTaskTopicForGeneration` 相同，PowerShell 侧是 lib 里的 `Get-DbTaskTopicName`，世代号取 db.yaml 的 `ServerConfig.Kafka.TopicGeneration`。

**go/db 运行时从不自己拼组名**：`EffectiveGroupID()` 原样返回配置字面量。组名由谁推导，分三种口径：

| 场景 | 组名 | 由谁推导 |
|---|---|---|
| 本机单 zone 默认启动（不带 `-Zone`） | 裸名 `db_rpc_consumer_group` | db.yaml 原值 |
| 本机 `-Zone N` 启动（`dev-start-zones`） | `db_rpc_consumer_group_zN` | `go_services.ps1` 改写派生配置 |
| K8s | 切组前是 `db_rpc_consumer_group`（final(L)）；切组后是 `db_rpc_consumer_group_z<ZoneId>`（final(Z)） | `k8s_deploy.ps1` 用 `Get-AuthoritativeScalar` 读 db.yaml 的基名，按 §2.3 的决策表渲染 |

几个工具各自推导同一个名字，规则完全一致：

- `k8s_zone_rollback.ps1` 在入口用 `Get-YamlScalar` 读 `ServerConfig.Kafka.GroupID` 和 `ServerConfig.Kafka.PartitionCnt`，读不到就 throw，不抄常量。
- `tools/merge_zone/preflight.go` 的 `dbConsumerGroupForZone` 返回 `defaultKafkaGroup + "_z" + zone`。
- `stress_summarize.ps1` 只在 `-Zone > 0` 时推导 `db_rpc_consumer_group_z<Zone>`。

### 2.2 四种 ConfigMap 形状与必填键证明

`go-svc-db-config` 里 `db.yaml` 的 `ServerConfig.Kafka` 段只有四种形状：

| 形状 | 写法（4 格缩进） | 谁能加载 |
|---|---|---|
| final(L) | `GroupID: "db_rpc_consumer_group"` | 任何版本 |
| final(Z) | `GroupID: "db_rpc_consumer_group_z<id>"` | 任何版本 |
| handoff(Z←L)（正向交接） | `ConsumerGroup: "…_z<id>"` + `GroupHandoffFrom: "db_rpc_consumer_group"`，不写 GroupID | 只有带交接闸的二进制 |
| handoff(L←Z)（反向交接） | `ConsumerGroup: "db_rpc_consumer_group"` + `GroupHandoffFrom: "…_z<id>"`，不写 GroupID | 只有带交接闸的二进制 |

- `New-DbConsumerGroupYaml` 先输出一行注释 `    # 消费组由 k8s_deploy.ps1 按 -DbConsumerGroup 与 Kafka 位点决定(docs/design/db-consumer-group-per-zone.md)`，然后输出最终形状的一行或交接形状的两行，替换掉 db 模板里原来的 `    GroupID: "db_rpc_consumer_group"` 锚点行（`New-GoSvcConfigMapYaml` 里的 `${dbConsumerGroupYaml}`）。
- 超时键不渲染，一律取默认值，见 §2.5。
- 组名在写进 YAML、Pod 注解和 kafka-0 的 sh 命令行之前，都要先过 `Assert-DbConsumerGroupName` 校验字符集 `^[A-Za-z0-9._-]+$`。

**必填键证明**：

- 交接闸之前的二进制把 `GroupID` 声明为 `json:"GroupID"`，即必填。遇到交接形状，它在 `conf.MustLoad` 就因 `field … GroupID is not set` 拒启。db Deployment 配的是 `maxUnavailable: 0`，旧镜像的新 Pod 起不来，rollout 卡住，旧 Pod 继续服务，不破坏数据。
- 新二进制的 `ValidateConsumerGroup` 拒绝以下组合，错误文本是英文。启动时由 `NewKeyOrderedKafkaConsumer` 包成 `invalid Kafka consumer group config: …`，发生在建消费组之前：
  - 两者都空：`Kafka.GroupID or Kafka.ConsumerGroup must be set`
  - 两者都有：`set only one of Kafka.GroupID and Kafka.ConsumerGroup`
  - `GroupHandoffFrom` 配了 `GroupID`：`Kafka.GroupHandoffFrom requires Kafka.ConsumerGroup (without Kafka.GroupID) so that binaries without the handoff gate refuse to start`
  - `GroupHandoffFrom` 等于本组：`Kafka.GroupHandoffFrom must differ from the consumer group`
  - `HeartbeatIntervalMs×3 > SessionTimeoutMs`
  - `RebalanceTimeoutMs < 15000`
- 方法刻意不叫 `Validate`：go-zero 的 `conf.Load` 会在 `Normalize` 之前自动调用根结构的 `Validate()`。
- 配置单测（都待 Codex 跑）：`TestPreHandoffBinaryRefusesZoneShapedConfig`（旧结构加载交接形状必须报错，且文本含 GroupID）、`TestPreHandoffBinaryLoadsFinalZoneShape`（旧结构能加载 final(Z)）、`TestPreHandoffBinaryIgnoresUnknownKeys`（旧结构忽略未知键）。

**与规格的差异**：`ValidateConsumerGroup` 允许「只有 ConsumerGroup、没有 GroupHandoffFrom」，二进制会把它当最终形状运行。但生成器从不写这种形状，`ConvertFrom-LiveDbConsumerGroup` 读到它会 throw。脚本侧更严，属于 fail-closed，不影响正确性。

**解析线上形状**（`ConvertFrom-LiveDbConsumerGroup`，纯函数）：

- 只认 `ServerConfig.Kafka.{GroupID, ConsumerGroup, GroupHandoffFrom}` 这三个路径。值走 `lib/release_common.ps1` 的 `ConvertFrom-YamlToFlatMap`，规则与 `Get-AuthoritativeScalar` 相同，所以注释行和带前缀的同名键（例如 `SnapshotConsumerGroup:`）都不算。
- 其它段的同名键一律忽略，例如顶层 `GroupID`、`ServerConfig.Database.GroupID`，因为 go/db 不读它们。
- 以下情况都 throw：同一路径出现两次；值为空或字符非法；组名不是 L 或 Z；四种形状以外的任何组合；三个键一个都没有。

**与规格的差异**：修订稿 §2 写的是「每个键按行锚定匹配」，代码改成了按 YAML 路径取值。

### 2.3 决策表（`Resolve-DbConsumerGroupPlan`，纯函数）

**输入**：

- `-Mode keep|zone|legacy`、`-LegacyGroup L`、`-ZoneGroup Z`、`-IsDryRun`。
- `-Live`：`ConvertFrom-LiveDbConsumerGroup` 的结果；`$null` 表示线上没有 `go-svc-db-config`。
- `-Facts @{ZoneHasOffsets; LegacyHasOffsets}`，下文记作 Zc / Lc：该组在 T 上至少有一行数值 `CURRENT-OFFSET`。
- `-FactsError`：读 Kafka 失败的原因。
- `-Topic / -Partitions / -BootstrapServer` 只用来拼收尾命令。

**输出**：`@{Shape; Group; HandoffFrom; Source; Notice; Warning}`，或者 throw。Facts 缺字段或不是布尔值时 throw（内部错误），不会把「读不到」当成「没有位点」。

**DryRun（不读集群）**

| Mode | 渲染 | Source |
|---|---|---|
| keep | final(Z) | `dryrun-preview` |
| zone | handoff(Z←L) | `dryrun-preview` |
| legacy | handoff(L←Z) | `dryrun-preview` |

三种都附 Warning：`DbConsumerGroup=<mode> under -DryRun: cluster not read (dryrun-preview); previewing …`。意思是这次没读集群，真跑时按线上 ConfigMap 和 Kafka 位点决定，结果可能是最终形状，也可能被拒绝。

**非 DryRun，keep**

| 线上 Live | Kafka 事实 | 渲染 | Source / 提示 |
|---|---|---|---|
| final(G) | 不读 Kafka | final(G) 原样保留 | `live-configmap` |
| handoff(X←Y) | 读不到（FactsError） | 保持 handoff(X←Y) | `live-configmap` + Warning（原样保留永远安全） |
| handoff(X←Y) | Y 在 T 上没有位点 | **final(X)**（自动收尾，去掉 GroupHandoffFrom） | `handoff-finalized` + Notice：db 滚动一次，注解 db-group-handoff-from 从 Y 变成 none |
| handoff(X←Y) | Y 仍有位点 | 保持 handoff(X←Y) | `live-configmap` + Warning，附收尾命令 |
| 无（全新 namespace） | Zc 且 Lc | **throw**（F4），给出两种收尾命令的写法 | — |
| 无 | 只有 Zc | final(Z) | `fresh-namespace-kafka` |
| 无 | 只有 Lc | final(L)（保留运维的反向决定，不隐式切组） | `fresh-namespace-kafka` + Notice |
| 无 | 都没有 | final(Z)（新 zone） | `fresh-namespace-kafka` |
| 无 | 读不到 | **throw**，提示可以先 `infra-up -WaitReady`，再 `all-up -SkipInfra` 或 zone-up | — |

**非 DryRun，显式模式**（zone 时 X=Z、Y=L；legacy 时 X=L、Y=Z；总要读 Kafka）

| X 有位点 | Y 有位点 | 其它条件 | 渲染 | Source / 提示 |
|---|---|---|---|---|
| ✓ | ✓ | Live 正好是 handoff(X←Y)，属于幂等重跑 | 保持 handoff(X←Y) | `explicit-handoff` + Warning（未收尾，附收尾命令） |
| ✓ | ✓ | 其它 | **throw**（F2）。当前权威组取 Live 的有效组（final 的 G 或 handoff 的 X）；Live 为 null 时打印两种写法 | — |
| ✓ | ✗ | — | final(X) | `explicit-already-authoritative`（回档 Step 6 走这一行） |
| ✗ | ✓ | — | handoff(X←Y) | `explicit-handoff` + Notice（滚完立刻收尾） |
| ✗ | ✗ | — | final(X) | `explicit-nothing-to-inherit` |
| 读不到 | | | **throw**（尚未做任何写操作） | — |

显式模式只允许 `-Command zone-up`，见 §4.1。throw 文案都写明「尚未做任何写操作」，并给出完整的 `tools/scripts/kafka_offset_reset.ps1 -BootstrapServer <b> -Topic <T> -Group <权威组> -RetireGroup <另一组> -Partitions <N> -FinalizeHandoff`。命令里的 `-BootstrapServer` 是集群内地址 `kafka.<InfraNamespace>:9092`，在集群外执行时要换成执行机连得到的 broker 地址。

**什么时候需要读 Kafka**（`Test-DbConsumerGroupNeedsKafkaFacts`）：非 DryRun，并且满足以下任一条件：显式模式；Live 为 null；Live 是交接形状。keep 遇到最终形状不读 Kafka，所以日常发布不依赖对 kafka-0 的 exec 权限。

### 2.4 交接闸的运行时语义（`go/db/internal/kafka/group_handoff.go`）

**只在交接形状下生效**

- `newGroupHandoffFromConfig` 在 `GroupHandoffFrom` 为空时返回 `(nil, nil)`，并且不调用 inspector 工厂（T8）。最终形状下进程里没有任何交接对象，不连 admin，不依赖对方组。
- 与 Phase A 之前相比，最终形状只多了三样：ClientID、三个与默认值相同的显式超时、新增指标。其中交接相关的五个指标恒为 0；`db_consumer_group_rebalance_gap_seconds` 不依赖交接闸，照常记录。

**挂点顺序**（`key_ordered_consumer.go`）

- `NewKeyOrderedKafkaConsumer`：
  1. 对本地副本先 `Normalize`；
  2. `ValidateConsumerGroup`；
  3. 用 `EffectiveGroupID` 建组；
  4. `newGroupHandoffFromConfig`。失败时先关掉已建的 consumerGroup，再返回 `group handoff config: …`。
- `Start`：有 handoff 时先起守望 goroutine（`watch(c.ctx, c.handoffDone)`）。消费循环里每轮 `Consume` 之前先 `await(consumeCtx)`；等待被 ctx 取消时打 `consumer group stopped while waiting for group handoff: …` 并退出循环。
  - `consumeCtx` 在撤拉取（Stop 第①步）时取消，所以闸前的等待不会卡住停机。
  - 启动日志带 `handoffFrom=`。
- `Setup`：
  1. 分区契约检查；
  2. handoff 非 nil 时 `seed`。出错就返回 `group handoff seeding failed: …`：sarama 释放本 session，消费循环 1s 后（`consumeRetryBackoff`）重新入组。此时已经 released，不会再过闸，只会用重建的 inspector 再播种一次；
  3. 取消旧的 `yieldCancel`，再从 `session.Context()` 派生新的 `yieldCtx`，然后 `setSessionYield`。返回 false，说明守望在播种和登记之间已经让出，就立即取消；
  4. 置 `setupDone`，记 rebalance gap。
  - handoff 为 nil 时不建 `yieldCtx`，`sessionScope` 退回 `session.Context()` 和一个 nil channel，行为与引入前相同（`TestConsumerSetup_NoHandoffKeepsLegacyBehavior`）。
- `Cleanup`：取消 `yieldCancel`，调 `clearSessionYield`。只有 `setupDone` 为真时才调 `gap.sessionEnded`。Setup 失败之后的 Cleanup 也能容忍。
- `ConsumeClaim`：
  - 父 ctx 由 `sessionScope` 给出。`claimCtx` 由 `context.WithoutCancel(parentCtx)` 派生，再配一个观察 goroutine：只有在没有撤拉取时才随父 ctx 取消。这是 S4 的停机顺序，见瓶颈文档 §4.4「G6 停机顺序」，本文不重复。
  - 顶层 select 和消息通道关闭后的排空 select 里都有 `case <-yieldDone`。让出时走原有的 `drainAfterStop("handoff peer reclaimed partition")` 收尾路径，然后 return nil，让 sarama 结束本 session；如果 session ctx 也已经结束，按 `session canceled` 记。已经撤拉取时转去 `drainClaimForShutdown`。
- `stop`：`closeHandoff` 放在第⑤步，在 `consumer.Close()`（第③步）和 `releaseRetryOwnership` 之后、第⑥步 `c.enqueuer().close()` 之前。它先等守望 goroutine 退出（c.ctx 已经取消，最多等一次在途的 watchOnce），再关 inspector。

**闸前判定**（`await` → `evaluate` → 纯函数 `decideGate`）

- 本进程第一次 `Consume` 之前，按 `handoffPollInterval`（1s）轮询。被闸住的一方不入组，也不持有分配，这样修掉了 B1 的互等。
- 放行后在锁内置 `released=true`，之后的再平衡不再看对方组。只有守望让出时才复位。
- 每次判定记 `CountHandoffGate(outcome)`，所以 gate_total 至多 1/s/Pod。等待期间 `SetHandoffWaiting(已等时长)`；放行时把它置 0，并 `ObserveHandoffWait`。
- `evaluate` 在一次 `withInspector` 里依次查：本组在全部分区上的位点 → 对方在全部分区上的位点 → 对方 `DescribeGroup`。任何一步出错都会丢弃 inspector。判定顺序就是优先级：

| 序 | 条件 | outcome | 结果 | 日志 |
|---|---|---|---|---|
| ① | 本组位点查询出错，或 inspector 建不出来 | `wait_own_unavailable` | 等待 | Error `group handoff waiting` |
| ② | 对方位点或 DescribeGroup 出错（不管本组是否完整） | `wait_peer_unavailable` | 等待 | Error |
| ③ | 对方 Stable 且持有本 topic 的分区 | `wait_peer_holding` | 等待 | Info |
| ④ | R7：存在分区 p，两组都有位点且 peer[p] > own[p] | `wait_own_behind_peer` | 等待 | Error `group handoff refused …`，写明两种处置 |
| ⑤ | 对方在全部分区上都是 -1，且不持有分区（不管对方处于什么状态） | `no_peer_offsets` | **放行** | Info `group handoff released` |
| ⑥ | 对方 Empty / Dead（GROUP_ID_NOT_FOUND 映射为 Dead），或 Stable 且不持有 | `peer_released` | **放行** | Info |
| ⑦ | 对方 PreparingRebalance / CompletingRebalance | `wait_peer_rebalancing` | 等待 | Info |
| ⑧ | 状态为空串或其它未知值 | `wait_peer_state_unknown` | 等待 | Error，带原始 State |

- 等待原因变化时打一行日志；原因不变时最多每 10s 打一行（`handoffWaitLogInterval`）。
- 放行时把 settled 初始化为 `{p : own[p] ≥ 0}`。
- 原规格里的 `peer_unreachable_own_complete` 已按修订稿 R5 删除：它排在 R7 之前，对方查询失败时会跳过 R7，等于 fail-open。

**播种**（`seed`，在 Setup 里调用）

- 只处理本 session 分到、且不在 settled 里的分区（need）。need 为空时不碰 admin，也不计数。
- 先查本组在 need 上的位点。own ≥ 0 的分区记 `own_present` 并进入 settled；其余分区记为 rest。
- rest 非空时再查对方。对方 ≥ 0 的分区执行 `session.MarkOffset(topic, p, peer, "")`：用**原值，不加 1**，因为已提交位点本来就是「下一条要读的 offset」。记 `seeded`，打 Info 日志 `group handoff seeded partition: …`，但**不进 settled**：MarkOffset 之后、1s 自动提交之前崩溃的话，这次提交会丢，所以下一次 Setup 要再确认一次本组位点。
- 两组都是 -1 的分区记 `no_offsets` 并进入 settled，从 `Offsets.Initial`（oldest）起步。从未写入的分区本来就是这样。logx 没有 WARN 级别，所以用 Infof 打，文本以 `WARN ` 开头。
- 任一查询失败时，本次涉及的每个分区记 `aborted`，并返回 error。
- 不在 ConsumeClaim 里 `MarkOffset(claim.InitialOffset())`：组没有已提交位点时，`InitialOffset()` 是 `OffsetOldest` 的哨兵值 -2，而 MarkOffset 只会往前推，-2 是空操作（修订稿 R4，按 sarama v1.43.1 核对）。

**守望与让出**（`watch` / `watchOnce` / `yield`）

- 每 `handoffWatchInterval`（5s）观测一次：先 `DescribeGroup`，再查对方位点。未放行时清空基线；查询出错时直接返回，从成功转为失败的那一刻打一行 Info。
- 三个条件**同时**成立才让出：对方 Stable；对方持有本 topic 的分区；相邻两次观测之间对方有分区的位点前进了（prev<0 且 cur≥0 也算）。
- 让出时记 `CountHandoffConflict`，打 Error 日志 `group handoff conflict: peer … is consuming topic …; yielding this session`，然后 `yield()`：在锁内复位 released、清空 settled、取消当前 session 的 yieldCtx。ConsumeClaim 返回 nil，sarama 结束 session，消费循环下一轮重新过闸。
- 被闸住的对方不入组，也不持有分配，所以触发不了让出。正向、反向两侧都配了 `GroupHandoffFrom` 时也不会互等（`TestGroupHandoffWatch_DoesNotYieldToGatedPeer`）。

**inspector 出错即重建**（修订稿 R2）

- sarama v1.43.1 的 ClusterAdmin 只读缓存的 coordinator，遇到 NOT_COORDINATOR 不刷新。
- 所以 `withInspector` 在 `inspMu` 内执行查询：fn 一出错就 Close 并置 nil，下次调用由工厂重建。新 client 会重新 FindCoordinator。不做「RefreshCoordinator 加重试」。
- 工厂 `newSaramaGroupInspector` 的配置：
  - 自建 `sarama.Client`，再 `NewClusterAdminFromClient`；
  - `ClientID = <消费者 ClientID>-handoff`；
  - `Net.Dial/Read/WriteTimeout = handoffAdminTimeout`（5s），`Metadata.Retry.Max = 1`；
  - 版本 `consumerKafkaVersion`（`sarama.V3_5_0_0`，与消费者共用）。
- `CommittedOffsets` 委托 `kafkautil.CommittedOffsets`：调用失败、resp 为 nil、顶层 Err、block 缺失、block.Err 都返回 error。`errors.Is(err, sarama.ErrGroupIDNotFound)` 时按全部 -1 处理。
- `DescribeGroup`：结果条数不等于 1 时报错；成员分配解码失败时 fail-closed 报错（`heldPartitions`）。

### 2.5 超时键、ClientID 与起始位点

- **超时键**：

  | 键 | 缺省 | 来源 |
  |---|---|---|
  | `SessionTimeoutMs` | 10000 | `DefaultKafkaSessionTimeoutMs` |
  | `HeartbeatIntervalMs` | 3000 | `DefaultKafkaHeartbeatIntervalMs` |
  | `RebalanceTimeoutMs` | 60000 | `DefaultKafkaRebalanceTimeoutMs` |

  - 缺省值等于 sarama v1.43.1 的默认值。≤0 时由 `Normalize` 回填。
  - 约束：`HeartbeatIntervalMs × 3 ≤ SessionTimeoutMs`；`RebalanceTimeoutMs ≥ MinKafkaRebalanceTimeoutMs`（15000 = `claimDrainTimeout` 10s + 5s 提交余量，两处要同步改）。
  - K8s 不渲染这三个键。要调（例如心跳 1000、再平衡 25000），必须先按 AGENTS §6 压测出对比表，另立项改生成器。
- **ClientID**：`consumerClientID(zone, host)` 返回 `db-z<zone>-<host>`。host 里 `[A-Za-z0-9._-]` 以外的字符换成 `-`，去掉首尾的 `-`，总长截到 200；host 为空时只返回 `db-z<zone>`。broker 侧（describe 的 CLIENT-ID 列）因此能认出是哪个 zone 的哪个 Pod。
- **起始位点**：`Offsets.Initial` 保持 `OffsetOldest`。改成 Newest 会让真正的全新 topic 丢掉第一个 session 之前写入的消息。按 zone 拆组靠的是播种，不靠改这里。

### 2.6 指标（subsystem `db`，都不带 player_id，label 是封闭集合）

| 指标 | 类型 | label / 桶 | 含义 |
|---|---|---|---|
| `db_consumer_group_rebalance_gap_seconds` | histogram | 桶 0.5,1,2,3,5,10,15,20,30,45,60,120 | 从一个 session 的 Cleanup 到下一个 session 的 Setup 完成，含闸前等待和 Setup 失败后的重新入组。进程的第一个 session 不记 |
| `db_consumer_group_handoff_total` | counter | `result` = seeded / own_present / no_offsets / aborted | Setup 里逐分区的播种结果。已落定的分区不再计数 |
| `db_consumer_group_handoff_gate_total` | counter | `outcome` = no_peer_offsets / peer_released / wait_peer_holding / wait_peer_rebalancing / wait_peer_state_unknown / wait_own_behind_peer / wait_peer_unavailable / wait_own_unavailable | 每次闸前判定 |
| `db_consumer_group_handoff_waiting_seconds` | gauge | 无 label | 当前已在闸前等待的时长；没在等时为 0 |
| `db_consumer_group_handoff_wait_seconds` | histogram | 桶 0.5,1,2,5,10,15,30,45,60,120,300,600,1800 | 放行时记一次本次等待的总时长 |
| `db_consumer_group_handoff_conflict_total` | counter | 无 label | 守望让出的次数 |

- `register()` 把 4 个 result、8 个 outcome 和 conflict 都先以 0 建出序列（`TestConsumerGroupHandoffSeriesPrecreated`）。
- 取值必须与 `HandoffSeed*` / `HandoffGate*` 常量逐字一致：PromQL 表达式里写错 label 值不会报错，只会永远不响。
- 六个指标追加在 S3 的 `db_task_producer_partition_total` 之后。

### 2.7 告警（`deploy/k8s/db-task-retry-alerts.yaml` 组 `db-task.consumer-group-handoff`，interval 30s，5 条）

| 告警 | 表达式 | 级别 | 要点 |
|---|---|---|---|
| `DbConsumerGroupHandoffWaiting` | `max by (namespace) (db_consumer_group_handoff_waiting_seconds) > 45` | warning | 有 Pod 在闸前等了超过 45s。切组那次滚动发布中途会响一次，属于预期：第一个新 Pod 要等本 zone 最后一个跑对方组的旧 Pod 退出。按日志里的 reason 处置；正向交接时，legacy 组被其它 zone 的发布弄得反复再平衡，所以切组期间冻结其它 zone 的发布 |
| `DbConsumerGroupHandoffStuck` | `min by (namespace) (db_consumer_group_handoff_waiting_seconds) > 300` | critical | **用 min 不用 max**：只有本 zone 没有任何 Pod 越过闸时才呼人。同 namespace 已放行的副本 gauge 为 0，会压住本条。等太久，没消费的存盘会被 Kafka 按保留期删除（retention.ms 与 retention.bytes 由 `k8s_deploy.ps1` 按 `-KafkaProfile` 设置，实际缓冲以部署输出的 Kafka disk ledger 为准） |
| `DbConsumerGroupHandoffOwnBehindPeer` | `sum by (namespace) (increase(db_consumer_group_handoff_gate_total{outcome="wait_own_behind_peer"}[5m])) > 0` | critical | R7 拦住了放行，处置分两种。**本组陈旧**（上一次交接没收尾）：`kafka_offset_reset.ps1 -Group <对方组> -RetireGroup <本组> -FinalizeHandoff`。**对方陈旧**（本组是有意拨回的，例如回档）：用重置模式加 `-RetireGroup <对方组>`，与回档 Step 5 相同。不要手改游标 |
| `DbConsumerGroupHandoffConflict` | `sum by (namespace) (increase(db_consumer_group_handoff_conflict_total[10m])) > 0` | critical | 守望发现对方组在消费本 topic，多半是旧发布物被误部署。先处理掉那个 Pod，再核对本 zone 的 `go-svc-db-config` |
| `DbConsumerGroupHandoffSeedAborted` | `sum by (namespace) (increase(db_consumer_group_handoff_total{result="aborted"}[5m])) > 0` | warning | 播种查询失败会让本 zone 的组反复再平衡。**按分区计数**，一次失败会记多个。查 coordinator，以及 legacy 组是否可读；日志搜 `group handoff seeding failed` |

- `no_offsets` 不设告警：全新 topic 首次启动、从未写入的分区都属正常。
- 最终形状下 gauge 恒为 0，counter 不增长，五条都不会响。
- 安装方式见 `deploy/k8s/README.md`「go/db 存盘管线告警」。规则手工 apply，仓库不部署 Prometheus。

**与规格的差异**：

- 原规格只有 4 条，修订稿补了 SeedAborted。
- 原规格 Stuck 用的是 `max by > 300`，代码改成 `min by`。理由：收尾之前单个副本重启会重新过闸，而同 namespace 已放行的副本照常消费全部分区，这时不应该 critical。

## 3. 安全论证（附录 F8 的更正）

交接闸**做不到互斥**。正确性仍然靠按 topic 拼键的三样东西兜底，它们都与消费组无关：

- 游标 `consumer:applied:<topic>:<key>:<msgType>`（`appliedSeqKey`）
- 排序锁 `kafka:ordering:<topic>:<key>:<msgType>`（`orderingLockKey`）
- 归属 epoch `consumer:applied_epoch:<topic>:<key>:<msgType>`（`appliedEpochKey`）

交接闸做的事是：把重叠窗口压短，并且不让新组从 Oldest 起步、不让它从陈旧位点续消费。

- **(a) 闸加守望做不到互斥。** 以下情况下两组会同时消费同一分区：
  - 对方是 Phase A 配置（最终形状，没有交接闸），入组即消费；
  - 旧 Pod 在滚动中崩溃重启，读到的还是旧 ConfigMap；
  - 有人用旧镜像或旧发布物部署。

  守望只凭正面证据让出，重叠窗口最长约为一个守望周期（5s）加一轮再平衡。对方查询失败时守望不让出，看不见的重叠会更长。这段时间里只有游标、排序锁和 owner_epoch 兜底。
- **(b) 读和写可能落在不同分区，「没有重叠就没有并发写者」不成立。** login 发的读任务和 C++ scene 发的写任务，在 ①（读写分区对齐，[分区契约](db-task-kafka-partition-contract.md)）全量上线之前按不同的分区函数选分区。读任务回写缓存，与其它分区上的写本来就会并发。交接只是不放大这个既有竞态。① 已落码，但 login 全部滚到按契约分区的版本之前，这一条一直成立。
- **(c) 重试与消费组无关。** 重试的 ready / processing / 死信键都按 topic 拼。④ 之后又加了按分区的 ready 键 `kafka:retry:queue:{topic}:p{N}`（`retry_routing.go` 的 `retryPartitionReadyKey`）。交接闸覆盖不到重试，重试的正确性仍然靠排序锁租约。④ 的 owner 模式以 claim 租约判定属主，租约用 `sessionScope` 派生的 claimCtx 登记，让出时一起撤销。但在重叠窗口里，两组的成员可能各自持有同一分区的 claim。
- **(d) 游标缺键时默认放行。** FLUSHDB 或丢键之后，「写重放会被游标吸收」这个前提就不成立了。K8s 上游标在共享 Redis DB 0；`k8s_zone_rollback.ps1` 的 FLUSHDB 防护（`Get-SharedRedisGuardPatterns` 里的 `consumer:applied*` 等模式）会拒绝清共享库。回档自己的 FLUSHDB 发生在 Step 4，所以 Step 5 必须先复核重置结果，再删 legacy 位点（§6）。
- **R7（进程内纵深防御）**：两组在同一分区上都有位点、且对方更大时，闸不放行，也就不会从陈旧位点大段重放（附录 B2 / B4）。它兜住手工部署、旧脚本和漏收尾，与部署期的 F2 / F4 检查（§4）是两道独立的闸。回归用例 `TestReplayAfterSeed_WritesAreOrderingStale`：从 S−k 重放时写全部判为 orderingStale，游标不变，不写缓存。

## 4. 部署脚本 `tools/scripts/k8s_deploy.ps1`

### 4.1 参数与入口拒绝

- `[ValidateSet("keep","zone","legacy")][string]$DbConsumerGroup = "keep"`。值先经 `ConvertTo-ClientEntryCanonicalName` 规范化。
  - **keep**：按线上现状渲染（§2.3）。
  - **zone**：本 zone 切到 Z。
  - **legacy**：反向切回 L，用于回滚。
- 不是 keep、且 `-Command` 不是 zone-up 时，入口直接 throw：`-DbConsumerGroup <mode> 只允许与 -Command zone-up 一起用(逐 zone 切组 / 反向交接);<cmd> 请用默认的 keep。` all-up 一次切全部 zone，等于把所有 zone 同时放进交接窗口。
- **接手方补充**：显式 zone|legacy 配上「本次不部署 db」的运行（没给 `-GoSvcRegistry`，或带了 `-SkipGoSvc`；`Test-ZoneDbDeployed` 为假）时，`Initialize-DbConsumerGroupPlans` 直接 throw。不然 C++ 节点照常重部署、退出码为 0，运维会以为已经切过组了。**`k8s_deploy.ps1` 的 `-GoSvcRegistry` 缺省为空，所以直接调它切组时必须显式给 `-GoSvcRegistry`。** `dev_tools.ps1` 的 `-GoSvcRegistry` 缺省是 `ghcr.io/luyuancpp`，经 `dev_tools.ps1 -Command k8s-zone-up` 时不受影响。

### 4.2 写路径入口 `Initialize-DbConsumerGroupPlans`

**位置**：在写路径预检块里，紧跟在 `Assert-ClientEntryDeployPreflight` 之后，早于密钥解析（`Initialize-InjectedSecrets`）和 `switch ($Command)` 里的任何写操作。所以读失败或判定拒绝时，不会留下半截部署。

**前置**：只在 `Test-ZoneDbDeployed` 为真时计算；条件与 `Test-ZoneLoginDeployed` 相同：zone-up / all-up、不带 -SkipGoSvc、`-GoSvcRegistry` 非空。

**流程**：

1. 读 db.yaml 的 `ServerConfig.Kafka.GroupID`（L）、`TopicGeneration`（必须是非负整数）、`PartitionCnt`（必须是正整数）。读不到或不合法就 throw。bootstrap 记为 `kafka.<InfraNamespace>:9092`，只用来拼命令。
2. zone 列表：zone-up 取 `-ZoneName / -ZoneId`；all-up 取 zones 配置里的每个 zone（all-up 只能是 keep）。
3. **全新集群的首次 all-up**（接手方补充）：不带 `-SkipInfra`、非 DryRun 的 all-up，先只读执行 `kubectl get namespace <InfraNamespace> --ignore-not-found -o name`。
   - namespace 不存在：本次 Apply-Infra 会新建一个空 Kafka，两组在任何 topic 上都不可能有位点。各 zone **不 exec**，直接按「两项都是 false」查决策表，并打一行 `DB consumer group: infra namespace … does not exist; …`。
   - 判据只用「namespace 不存在」：StatefulSet 被删而 PVC 还在时，位点仍然存在。
   - 探测本身失败就 throw。
4. 逐个 zone 处理：
   1. 非 DryRun 时用 `Read-LiveDbConfigMapYaml` 读线上 ConfigMap，再归到四种形状之一（`ConvertFrom-LiveDbConsumerGroup`）；
   2. 需要时用 `Read-DbGroupTopicFacts` 读 Kafka。读失败的原因作为 `-FactsError` 交给决策表：只有「keep + 线上交接形状」接受它，其余情况由决策表 throw；
   3. 调 `Resolve-DbConsumerGroupPlan`，结果存进 `$script:DbConsumerGroupPlans[zoneId]`；
   4. 每个 zone 打一行 `DB consumer group: zone=<name>(<id>) mode=… shape=… group=… handoff_from=…|none source=… topic=…`。Notice 用 Write-Host，Warning 用 Write-Warning。

### 4.3 读线上 ConfigMap 与 Kafka 事实（R4 落码事实）

- **`Read-LiveDbConfigMapYaml`**：经 `Invoke-ClientEntryKubectl` 执行 `get configmap go-svc-db-config -n <zone ns> --ignore-not-found -o json`。
  - 退出码非 0 就 throw：读不到不等于没有。如果把「没读到」当成全新 namespace，线上的交接形状会被静默改写。
  - 输出为空返回 `$null`。
  - 不是合法 JSON，或者没有 `data.db.yaml`，都 throw。
- **`Read-DbGroupTopicFacts`**：对 Z、L 各执行一次。
  ```
  kubectl exec -n <InfraNamespace> kafka-0 -c kafka -- /bin/sh -c "KAFKA_HEAP_OPTS='-Xms16m -Xmx64m' /opt/kafka/bin/kafka-consumer-groups.sh --bootstrap-server localhost:9092 --describe --group <g>"
  ```
  - 小堆覆盖的理由：容器 env 里 broker 自己的大堆参数会被 `/opt/kafka/bin/*.sh` 继承，在 broker 的 memory limit 里再起一个大堆 JVM，会把 kafka-0 撑到 OOMKilled。写法与 kafka.yaml 里的探针命令相同。
  - 经 `/bin/sh -c` 执行，所以组名先过 `Assert-DbConsumerGroupName`，再拼进命令行。
  - stdout 和 stderr 拆行合并后，交给 lib 的 `Resolve-KafkaGroupTopicOffsets` 分类：missing / no-offsets 记为 false，offsets 记为 true。exec 失败、出现 `Error:` 行、退出码非 0、输出解析不了，都 throw。
  - 只在非 DryRun 时调用：`Invoke-ClientEntryKubectl` 自己不看 `$DryRun`，由调用方负责判断。
  - **与规格的差异**：修订稿 §3.3 写的是直接 exec `/opt/kafka/bin/kafka-consumer-groups.sh`，代码改成了 `/bin/sh -c` 加小堆。V3(j) 要按这个形状实测。
- **依赖**：部署账号要有 kafka-0 的 `pods/exec` 权限。exec 只打 kafka-0，多 broker 档时 kafka-0 不可用也算读不到。

### 4.4 DryRun 与全新 namespace

- DryRun 不读集群，按决策表渲染预览并告警（§2.3）。回档第 0 步 b 的静态预检走的就是这一路：zone 模式下预览 handoff(Z←L)，只校验参数组合。
- 全新 namespace（`Remove-Zone` 删 namespace 时 ConfigMap 一起没了）**以 Kafka 为准，只渲染最终形状**：两组都有位点就 throw；只有 L 有位点时保留 final(L)，不隐式切组。
- 全新 namespace 永远不需要收尾，也就不会出现「隐式交接之后忘了收尾」。

### 4.5 渲染与 Pod 注解滚动

- `New-GoSvcConfigMapYaml` 对 db 调 `New-DbConsumerGroupYaml -Plan (Get-DbConsumerGroupPlan …)`。缓存里没有计划时 throw（内部错误），不会回落到任何默认组。
- `Apply-OneGoSvc` 的 `if ($SvcName -eq 'db')` 块是 db 专属注解替换的**唯一位置**（④ 的 RetryRouting 注解如果需要，也用同样的占位手法放进这个块）：
  - `PLACEHOLDER_DB_CONSUMER_GROUP` 换成 `Plan.Group`；`PLACEHOLDER_DB_GROUP_HANDOFF_FROM` 换成 `Plan.HandoffFrom`，为空时写 `none`。
  - **接手方补充**：manifest 里缺占位就 throw。`deploy/k8s/manifests/go-svc/db.yaml` 已有这两条占位。没有占位的话，只改消费组的部署是一次 no-op apply，切组和收尾都静默不生效。
  - 替换完仍残留 `PLACEHOLDER_DB_` 同样 throw。
- 消费组键只在 ConfigMap 里，而进程只在启动时读 ConfigMap，只改 ConfigMap 不会触发滚动。注解跟着组名或对方组变，Pod 模板变了才滚动；keep 收尾时 handoff-from 从对方组变成 none，也是靠它滚一次。注解**只用来触发滚动，不作能力证明**。
- 因为新增了注解，PR-C 之后每个 zone 的第一次部署都会滚一次 db，性质与普通发布相同。
- login 模板的 `  GroupID: "db_rpc_consumer_group"` 上方加了一行注释，说明它是死配置：login 只生产不消费，代码不读；它不是 db 的消费组；值不随切组变化；Step 7 删除。值没有改。

### 4.6 `dev_tools.ps1` 的透传与拒绝

- `[ValidateSet("","keep","zone","legacy")]$DbConsumerGroup = ""`：留空就不透传，由 `k8s_deploy.ps1` 的缺省值 keep 接管；非空时 `Invoke-K8sDeploy` 才透传。
- **接手方补充**：
  - `k8s-image-*` / `k8s-release-*`（`k8s_image.ps1` 不部署 db）显式给了 `-DbConsumerGroup` 时，在任何副作用之前 throw。
  - `k8s-zone-rollback` 显式给了也 throw，因为回档的 Step 6 固定传 zone。
  - **与规格的差异**：原规格写的是「release 流程不转发，等于 keep」，代码改成了拒绝，不静默吞掉。
- `k8s_client_entry_contract.tests.ps1` 已把 `DbConsumerGroup` 加进 `Invoke-K8sDeploy` 的 Forwarded 清单。

## 5. 收尾：`kafka_offset_reset.ps1 -FinalizeHandoff` 与 `-RetireGroup`

**为什么必须收尾**：不收尾时两组在本 topic 上都有位点，会出三个问题：

- 合服门禁拒绝（§7）；
- k8s_deploy 拒绝再切组（F2），全新 namespace 时判不出归属（F4）；
- go/db 的 R7 会拦住之后的反向交接。

所以每次正向或反向交接完成，都必须执行收尾。

**命令**（先 dry-run，确认没问题再加 `-Apply`）：

```
pwsh -File tools/scripts/kafka_offset_reset.ps1 -BootstrapServer <b> -Topic <T> -Group <新权威组> -RetireGroup <旧组> -Partitions <N> -FinalizeHandoff [-Apply]
```

- 正向：`-Group db_rpc_consumer_group_z<id> -RetireGroup db_rpc_consumer_group`；反向时两者互换。
- N 取 db.yaml 的 `ServerConfig.Kafka.PartitionCnt`（现在是 10）。
- 本地包装：`dev_tools.ps1 -Command kafka-offset-reset -KafkaBootstrapServer <b> -KafkaTopic <T> -KafkaGroup <新组> -KafkaRetireGroup <旧组> -KafkaPartitions <N> -KafkaFinalizeHandoff [-KafkaApply]`。这个分支用 hashtable splatting，修掉了原来数组 splatting 按位置绑定的老 bug（修订稿 R8，`dev_tools_kafka_offset_reset.tests.ps1` 钉住）。

**执行机前提**：要有 pwsh 和 Kafka CLI（`kafka-consumer-groups.sh`、`kafka-get-offsets.sh`），放在 PATH 上或用 `-KafkaBin` 指定，并且连得到 broker。kafka-0 容器里没有 pwsh。

**参数约束**：

- 五种模式恰好选一种：ToDatetime / ToEarliest / ToLatest / DeleteAndRecreateTopic / FinalizeHandoff。
- FinalizeHandoff 必须同时给 -Group、-RetireGroup、-Topic、-Partitions > 0。
- -RetireGroup 必须配 -Partitions > 0，不能等于 -Group（大小写敏感比较），不能与 -DeleteAndRecreateTopic 同用。

**FinalizeHandoff 的流程**：

1. describe 新组，再 describe 旧组（`Invoke-KafkaConsumerGroupDescribe` 内部走 `Resolve-KafkaGroupTopicOffsets`）。
2. `kafka-get-offsets.sh --bootstrap-server B --topic T --time -2`（earliest）和 `--time -1`（latest），解析见 `ConvertFrom-KafkaGetOffsets`。
3. `Test-HandoffFinalizeReady` 逐分区检查 p ∈ 0..N-1：
   - earliest 或 latest 缺 p：报问题；
   - 新组缺数值位点：只有 earliest == latest（空分区：从未写入，或已经全部过期）时放行，否则报问题。sarama 只提交被 MarkOffset 推进过的分区，所以空分区永远不会有位点（修订稿 R4）；
   - 两组都有位点、且新组 < 旧组：报问题，这通常说明方向弄反了。

   只要有问题就一次全部列出并 throw，不删任何位点。
4. 通过后调 `Remove-RetiredGroupTopicOffsets`，详见下面「删除与复核」。
5. `-Apply` 成功后打印下一步提示：`Next: re-run k8s_deploy.ps1 -Command zone-up for this zone with the default -DbConsumerGroup keep;`。keep 会把交接形状自动收成最终形状，见 §2.3 的 `handoff-finalized` 一行。

**删除与复核**（`Remove-RetiredGroupTopicOffsets`）：

- 旧组状态是 missing 或 no-offsets 时打印 `nothing to retire` 后返回，所以可以重跑。
- dry-run 时打印 `[DRY-RUN] would run kafka-consumer-groups.sh --bootstrap-server B --delete-offsets --group G --topic T`。
- `-Apply` 时执行删除，输出先过 `Assert-KafkaToolOutputClean`，退出码非 0 或出现 `Error:` 行都算失败。输出匹配 `GROUP_SUBSCRIBED_TO_TOPIC` 或 `actively subscribed` 时立即 throw：仍有成员订阅这个 topic。
- 删完之后再 describe 一次，旧组仍有数值位点就 throw。

**重置模式**（`-ToDatetime / -ToEarliest / -ToLatest`，回档 Step 5 用它）：

- 捕获工具输出，先过 `Assert-KafkaToolOutputClean`，再用 `ConvertFrom-KafkaResetPlan` 解析 GROUP/TOPIC/PARTITION/NEW-OFFSET 计划表；解析不出就 throw。
- `-Partitions > 0` 时，`Get-ResetPlanProblems` 要求计划恰好覆盖 0..N-1：缺分区、或出现 ≥N 的多余分区，都报问题。dry-run 和 -Apply 都检查。
- `-Apply` 时，--execute 之后再 describe 一次，`Get-ResetVerificationProblems` 逐分区断言 CURRENT-OFFSET 等于计划里的 NEW-OFFSET。复核失败的 throw 在删除 RetireGroup 位点**之前**；文案是「重置后复核失败,没有删除 <RetireGroup> 的位点」。
- 复核通过、且给了 -RetireGroup 时，才执行退役。回档是有意回退，所以这条路径**不做**「新组 ≥ 旧组」检查。

## 6. 回档：永远落到 zone 组（`tools/scripts/k8s_zone_rollback.ps1`）

**规则**：回档之后，这个 zone 一律以 zone 组为权威。还在 legacy 组上的 zone，借这次回档迁到 zone 组。第 0 步不需要判组，也没有新增 kubectl 读，原来「预检只读一次集群」的断言不受影响。

**参数**：

- `-KafkaGroup` 缺省为 ''，推导为 `<基名>_z<ZoneId>`。
- 新增 `-KafkaLegacyGroup ''`，推导为基名。
- 两者相等（大小写敏感）时，入口就 throw。
- 删除了 `-KafkaDrainTimeoutSec`。
- 基名和分区数从 db.yaml 现读，读不到就在任何改动之前 throw。
- 摘要行：`kafka : <bootstrap> topic=<T> group=<zone 组> retire=<legacy 组> partitions=<N>`。
- `dev_tools.ps1` 的对应变化：`-RollbackKafkaGroup` 缺省改为 ''，新增 `-RollbackKafkaLegacyGroup`，两者都只在非空时转发；删除了 `-RollbackKafkaDrainTimeoutSec`。

**第 0 步 c（只读快照，只告警）**：

- 只在 -Apply 下执行；dry-run 只打印要执行的 describe 命令。
- `Show-KafkaTopicLagSnapshot` 对 zone 组和 legacy 组各 describe 一次，只打印本 topic 的分区、位点、LAG 和合计。组不存在时打印 `(group does not exist)`。任何失败都只 `Write-Warning "Kafka LAG snapshot unavailable: …"`，绝不 throw。
- 它取代原来的 Step 2。

**第 0 步 d（Step 5 预演，修订稿 R7 / M5）**：

- 只在 -Apply 下、Step 1 之前执行。
- `Invoke-Step5Rehearsal` 先 describe zone 组。zone 组在本 topic 上已有数值位点时，打印 `zone group already holds offsets on <T>; Step 5 rehearsal skipped`。
- 结果是 missing 或 no-offsets 时，以 hashtable splatting 调 `kafka_offset_reset.ps1` 做 dry-run：`-Group <zone 组> -ToDatetime <TargetTime> -Partitions N`，不带 `-Apply` 和 `-RetireGroup`。退出码非 0 或 throw，就在**停服之前** throw，文案指向本文 §11 V3(c)。describe 本身失败也 throw。
- 这一步把「Kafka 建不出 zone 组的完整计划」提前到破坏性步骤之前暴露。

**Step 2**：已删除（2026-10-15）。zone-down 之后本 zone 的 db 已经停了，LAG 不可能排空；Step 5 本来就会把位点拨回 TargetTime，等排空没有意义。编号 3–7 保持不变。

**Step 5**：

- 用 hashtable splatting 调 `kafka_offset_reset.ps1`，参数为 `BootstrapServer / Topic / Group=<zone 组> / RetireGroup=<legacy 组> / ToDatetime / Partitions=N`，-Apply 时再加 `Apply`。调用前先把 LASTEXITCODE 清零。
- 流程：重置 zone 组 → 逐分区复核 → 复核通过后删掉 legacy 组在本 topic 上的位点。
- legacy 组即使还有别的 zone 的成员，只要没有成员订阅本 topic，broker 就允许删除。这一点待 V3(d) 实测。
- 仍有订阅者时，比如 namespace 没删干净，这一步会失败。此时 zone 已经停服、数据已经回档；清掉残留实例后，单独重跑同一条 kafka_offset_reset 命令。
- 如果漏了重跑：Step 6 的显式 zone 读到两组都有位点、线上又没有 ConfigMap，会在写操作之前 throw（F2）。手工强行部署的话，R7 也会拦住（legacy 位点 > zone 组的回档位点）。

**Step 6**：`$upArgs.DbConsumerGroup = 'zone'`，经 `dev_tools.ps1 k8s-zone-up` 透传。

- Step 5 之后只有 zone 组有位点，决策表落到 `explicit-already-authoritative`，渲染 final(Z)，即 `GroupID: "db_rpc_consumer_group_z<id>"`。任何镜像都能起，包括交接闸之前的镜像（修订稿 R1）。
- 第 0 步 b 的静态预检用同一份 `$upArgs` 加 -DryRun。

**已知局限**：zone 组在本 topic 上没有位点，却仍有活跃成员时（例如 final(Z) 下从未写入过的 zone），第 0 步 d 的 dry-run 重置会被 Kafka 以「组不是 Empty」拒绝，回档在停服前被拒。这是零写入 zone 的边角情形，没有处理；行为取决于 V3(a)。

回档 SOP 本身的 2026-10-15 修订见 [zone_data_rollback.md](zone_data_rollback.md)。

## 7. 合服：唯一权威组（`tools/merge_zone`）

**候选组**（`kafkaGroupCandidates`）：

- `-kafka-group` 显式非空时，只看这一个组。
- 否则候选是 zone 组 `db_rpc_consumer_group_z<zone>`，再加上 `-kafka-legacy-group`（缺省 `defaultKafkaGroup` = `db_rpc_consumer_group`；传空就只看 zone 组；与 zone 组相同时去重）。merge 取 src，unmerge 取 dst。

**判定**（`resolveAuthoritativeLag`，纯函数）：

- 每个候选组 describe 一次（`kafkaCLILagSource.TotalLag`）。
- 命中「组不存在」白名单的组视为不存在，不看退出码。
- 其余组退出码非 0，或者 `committedOffsetPresent` 读不出，都直接报错（fail-closed）。
- **恰好一个**组在本 topic 上有数值位点时，只对它用 `parseConsumerGroupLag` 算 LAG。LAG 为「-」照旧 fail-closed。
- 多于一个时报错 `more than one candidate group holds committed offsets on topic …`，提示先跑 `kafka_offset_reset.ps1 -FinalizeHandoff`；本机混用过单 zone 默认启动（裸名）和 `-Zone N` 启动（_zN）时，用 `-kafka-group`（dev_tools 用 `-MergeKafkaGroup`）指定实际在跑的组。
- 一个都没有时报错 `no candidate group holds committed offsets on topic …`。
- 候选组为空时 P3 也拒绝（内部守卫）。
- 通过时打日志 `preflight P3 OK: authoritative group <g> lag=0 on <topic> [...]`。

**为什么不取最小值**：被冻结的旧组 LAG 可能是 0，会替新组「担保」已排空，这是 fail-open（附录 B6）。回档或手工往回拨位点、反向交接后留下的冻结位点，都可能让非活跃组的位点更高（`TestResolveAuthoritativeLag_InactiveGroupHigherIsRefused`）。

**「组不存在」白名单**：在 `tools/merge_zone/preflight.go`（`consumerGroupMissingPatterns`）和 `tools/scripts/lib/kafka_consumer_groups.ps1`（`Get-KafkaGroupMissingPatterns`）里各写一份，注释互相指向。三条正则：

```
(?i)consumer group '[^']*' does not exist
GroupIdNotFoundException
GROUP_ID_NOT_FOUND
```

- 没有表头时，只有命中 `(?i)consumer group '[^']*' has no active members` 才判为「组在、没有位点」，即 Go 的 `consumerGroupNoActiveMembersPattern` / PowerShell 的 `Test-KafkaGroupNoActiveMembersText`。其余情况 fail-closed。
- V3 拿到 4.3.1 的原文后，**两边同步**补正则，并把原文追加为 fixture（`merge_unit_test.go`、`kafka_offset_reset_output_guard.tests.ps1`）。

**dev_tools**：`Get-MergeZoneArgs` 只在 `-MergeKafkaGroup` 非空时转发 `-kafka-group`；世代号仍从 db.yaml 现读（`Get-MergeDbKafka`）。

**代价**：合服前必须先收尾，多了一个运维步骤。

## 8. 上线步骤与回滚

### 8.1 发布顺序约束

- 原规格的 PR 划分：A 钉版 → B go/db（Phase A）→ Codex 验证 → 发布 Phase A 镜像 → C 脚本与工具 → 金丝雀 → D 文档。
- PR-C 只有一条硬前提：对某个 zone 做显式 zone|legacy 交接之前，这个 zone 必须已经在跑 Phase A 及以后的镜像。
- 全新 namespace 和回档已经与镜像版本无关，因为它们只渲染最终形状。

### 8.2 步骤

- **步骤 0（钉版）**：先做 V0，再合入 PR-A。两条 V0 命令（与 `deploy/k8s/README.md` 相同，`-n` 换成实际的 `-InfraNamespace`）：
  ```
  kubectl get pod -n mmorpg-infra -l app=kafka -o jsonpath='{range .items[*]}{.metadata.name}{" "}{.status.containerStatuses[0].imageID}{"\n"}{end}'
  docker image inspect apache/kafka:latest --format '{{json .RepoDigests}}'
  ```
  - 每个 broker 的 imageID 和本机 RepoDigests 都要含 `sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837`。任一处不一致，就按一次 broker 版本升级来评估（KRaft metadata.version、日志目录兼容性、能否回退），由负责人决定是否另开窗口。
  - 合入后，下一次 infra-up 会因为镜像字符串变化滚动重启 kafka StatefulSet。单 broker 档等于一次 broker 重启，要在没有玩家的窗口做，完成后跑 `-Command infra-kafka-topics -WaitReady`。多 broker 档按 PDB 逐个滚。
- **步骤 1（Phase A）**：发布 PR-B 镜像，生成器不变（仍是 final(L)），各 zone 照常 zone-up。
  - 观察 `db_consumer_group_rebalance_gap_seconds` 的基线：任一 zone 滚动时所有 zone 都会出现样本，这正是共享组耦合的实测证据。describe 的 CLIENT-ID 列会出现 `db-z<zone>-*`。
  - 通过条件：除了新增指标，没有任何差异。
  - 回滚：用旧 tag 重跑。
- **步骤 2**：合入 PR-C。
  - 线上已是 GroupID 形状的 zone，keep 下保持不变；但第一次部署会因为新注解滚一次 db。
  - PR-C 之后，全新 namespace 和回档按 Kafka 只渲染最终形状。
- **步骤 3（金丝雀）**：
  1. 选人数最少的 zone，冻结其它 zone 的发布，确认 legacy 组处于 Stable。
  2. 执行只改配置的发布。直接调 `k8s_deploy.ps1` 时**必须补 `-GoSvcRegistry`**（它的缺省为空，不补就在写路径入口被拒）：
     ```
     pwsh -File tools/scripts/k8s_deploy.ps1 -Command zone-up -ZoneName <z> -ZoneId <id> -DbConsumerGroup zone -GoSvcRegistry <与线上相同> -GoSvcTag <与线上相同> [其余参数与该 zone 上一次部署完全相同]
     ```
     也可以走 `dev_tools.ps1 -Command k8s-zone-up … -DbConsumerGroup zone`，它的 `-GoSvcRegistry` 缺省为 `ghcr.io/luyuancpp`。脚本会先做 F2 预检。正常情况下（只有 legacy 组有位点）打印 `shape=handoff … source=explicit-handoff`。
  3. 观察：日志依次出现 `group handoff waiting … reason=wait_peer_holding` → `group handoff released` → seeded / no_offsets。`handoff_total{result=~"seeded|no_offsets"}` 覆盖的不同分区合计为 10（跨副本），conflict 为 0，本 zone 的 gap < 15s，每个播种值等于切换前 legacy 组在该分区的 CURRENT-OFFSET。
  4. 立刻收尾：`kafka_offset_reset.ps1 -BootstrapServer <b> -Topic db_task_zone_<id> -Group db_rpc_consumer_group_z<id> -RetireGroup db_rpc_consumer_group -Partitions 10 -FinalizeHandoff`，先 dry-run，再加 `-Apply`。
  5. 紧接着**以 keep 再跑一次同样的 zone-up**：输出含 `source=handoff-finalized`，ConfigMap 变为 `GroupID: "db_rpc_consumer_group_z<id>"`，注解 `mmorpg.io/db-group-handoff-from` 变为 `none`，db 滚动完成。
- **步骤 4**：金丝雀浸泡 24h。生产环境只看指标，经历一次例行发布；delete pod 演练放到 staging 或 kind 做。
- **步骤 5**：其余 zone 每天切 1–2 个，每个 zone 都重复步骤 3 的全部动作，包括收尾和 keep 重跑。全部切完后，legacy 组在各 topic 上的位点都已删掉。
- **步骤 6（以后另立项）**：按 AGENTS §6 跑压测、出对比表之后，再考虑调 `HeartbeatIntervalMs=1000`、`RebalanceTimeoutMs=25000`（要改生成器）。没有对比表不调。
- **步骤 7（下一轮）**：
  - login 的 `KafkaConfig.GroupID` 本轮已改为 optional，注明废弃，login 代码不读它。全部 login Pod 跑上 optional 版本**至少 7 天**后，按顺序从三处删除：`go/login/etc/login.yaml`、`k8s_deploy.ps1` 的 login 模板、`deploy/login-stack.linux/login.yaml`。先放宽再删，新旧 login 混跑期间都能启动。
  - 全部 zone 切完并收尾之后，把 merge_zone 的 `-kafka-legacy-group` 缺省改为空串（`tools/merge_zone/main.go` 的 flag 定义）。

### 8.3 回滚

- **单 zone 反向交接**（不影响已经切完的 zone）：
  1. 确认正向已经收尾。没收尾时脚本会以 F2 拒绝，按它打印的命令收尾后重跑。
  2. 执行 `zone-up -DbConsumerGroup legacy`，渲染 handoff(L←Z)。
  3. 收尾：`-FinalizeHandoff -Group db_rpc_consumer_group -RetireGroup db_rpc_consumer_group_z<id>`。
  4. 以 keep 再跑一次 zone-up，落到 final(L)。
- **退回到 Phase A 之前的镜像**：
  - 最终形状下可以直接用旧镜像部署。
  - 交接形状下必须先收尾，落到最终形状，再退镜像。
- **手工兜底**，只留给「收尾之前必须退镜像」这一种情形，例如新组 X 还没接手全部分区，FinalizeHandoff 的前置检查过不了。这一流程没有脚本化，也没有验证过：
  1. db 缩到 0（`kubectl scale deployment db -n <zone ns> --replicas=0`），等两组都没有成员。
  2. 以旧组 Y 为目标：如果 X 在某些分区已经越过 Y，用 `kafka-consumer-groups.sh --reset-offsets --to-offset` 或 `--from-file` 把 Y 的位点灌到 max(X, Y)；不灌的话，Y 从冻结点重放，写会被游标吸收（前提是游标键还在），读要全部重算。
  3. 用 `--delete-offsets --group X --topic T` 删掉 X 在本 topic 上的位点，并 describe 复核。
  4. 手工把 ConfigMap 改回 final(Y)：只写 `GroupID: "Y"`。
  5. 用旧镜像扩回副本数。
  6. 之后的 keep 部署会读到 final(Y) 并保持，注解变化再滚一次。

### 8.4 运维禁令

- 切组和收尾期间冻结其它 zone 的发布。正向交接时，legacy 组的反复再平衡会把闸卡在 `wait_peer_rebalancing`；最坏情况可达 Rebalance.Timeout 60s，超过 login 的 `TaskWaitTimeout` 30s。
- **收尾之后禁止用旧发布物（旧版生成器加旧镜像）部署 db。** 旧生成器会把字面量 `GroupID: "db_rpc_consumer_group"` 写回去，而 legacy 组已经没有本 topic 的位点，进程会从 Oldest 重放整个保留期。收尾之后守望已经不在运行，运行时发现不了这种情况，只能靠本条禁令，加上游标和排序锁兜底。
- 不要手改游标、排序锁或 applied 游标。陈旧位点一律用 kafka_offset_reset 处理。

## 9. 与 ①④、条件写及其它批次的关系

- **条件写**：本项不依赖条件写。`markAppliedSeq` / `markAppliedOwnerEpoch` 的调用位置和顺序没有动；`handleTask` / `processTaskBatch` / `runDBOp` 和 `dbOpHandler` 的签名都没有改。交接只改了 `claimCtx` 的父 ctx：有闸时是 yieldCtx，否则是 session ctx，与改动前逐字等价。另外加了让出分支。
- **①（读写分区对齐）**：两项正交。F8(b) 的竞态在 ① 全量上线之后才收窄（§3）。
- **④（重试只交给分区属主）**：③ 先于 ④ 上线，这样 ④ 的逐 zone 滚动只停本 zone，也避免共享组下 owner 模式出现全服重试撤回尖峰。挂点：
  - Setup 的顺序是：分区契约 → seed → yieldCtx / setSessionYield → 记 gap。
  - claim 租约用 sessionScope 派生的 claimCtx 登记；④ 的 inflight 等待挂在 `drainAfterStop` 里，让出路径自动继承。
  - 按分区的重试键 `kafka:retry:queue:{topic}:p{N}` 与消费组无关；merge_zone 的 P4 已经用 SCAN 覆盖 `p*`。
- **S4（停机顺序）**：`stop` 的六步和 ② 超时分支先 `c.cancel()` 的理由，见瓶颈文档 §4.4「G6 停机顺序」。与本项相关的只有两点：`closeHandoff` 在第⑤步；闸前等待随 `consumeCtx`（撤拉取）取消。
- **S5（保留期与 PVC）**：kafka.yaml 和 docker-compose.yml 里 S2 只改了 image 行，并给 kafka.yaml 加了 `imagePullPolicy`，与 S5 的保留期和 PVC 改动行不重叠。Stuck 告警描述里引用的保留期，由 S5 的 `-KafkaProfile` 档位决定。
- **全局单点交接**（handoff-global-singletons）里的 P1「db 消费组全 zone 共用」由本项落地。那个会话不要再按组名直接改。

## 10. 偏离附录之处与代价

附录指 `docs/handoff/godb-pipeline-handoff-20261013-appendix.md` 的 consumer-group-per-zone 节。原规格曾在 F3 / F4 / F5 上偏离附录，修订稿撤回了其中两处，最终状态如下。

### 10.1 F3：已按附录去掉 GroupHandoffFrom（keep 自动过渡）

- 原规格曾保留 `GroupHandoffFrom`。修订稿 M4 撤回了这个偏离：收尾之后去掉它。
- 机制不是新增第三种渲染模式，而是 keep 读到 handoff(X←Y)、且 Y 已经没有位点时，自动渲染 final(X)（`handoff-finalized`），db 滚一次。之后进程里没有交接对象，不再依赖 legacy coordinator。
- 「只闸一次」和「对方没有位点、也不持有分区就放行」（⑤，不管对方状态）照附录实现。
- **仍然偏离附录的一点**：附录 F3 要求「不因 admin 调用出错而等待」。修订稿 R5 改为对方查询出错一律等待（`wait_peer_unavailable`），否则会跳过 R7，等于 fail-open。代价：交接窗口里 legacy coordinator 不可达时，本 zone 会停在闸前，由 Waiting / Stuck 告警发现。这个依赖只存在于收尾之前。
- **代价**：收尾之后守望不再运行，误部署旧发布物在运行时发现不了，只能靠 §8.4 的禁令，加上游标和排序锁兜底。

### 10.2 F4：以 Kafka 事实判定

- 显式模式、全新 namespace、线上是交接形状这三种情况，都 `kubectl exec` 进 kafka-0 读两组的位点。两组都有位点时，显式模式和全新 namespace 都 throw，并附收尾命令。不新增登记 ConfigMap：复核建议的「持久化组选择」会与 Kafka 实况漂移，又多出一份真相源（修订稿 R6 不采用）。
- **仍然偏离附录的两点**：
  1. 附录写的是「看谁在本 topic 上有已提交位点**或成员**」，代码只看数值位点。成员信息由 go/db 的交接闸在运行时看（③ wait_peer_holding）。
  2. 附录要求 DryRun 必须显式传 -DbConsumerGroup，或者提供一个连集群的只读 plan 模式。代码的做法是：DryRun 渲染预览，并且总打 Warning，不静默。
- **代价**：k8s_deploy 依赖对 kafka-0 的 `pods/exec` 权限，kafka-0 也必须可用。读失败时按场景处理：显式模式和全新 namespace 拒绝部署（fail-closed）；keep 加线上交接形状时保持线上形状，并告警。

### 10.3 F5：改为「最终形状 + 必填键证明」

- 不采用附录的 OCI label 或 `db_build_info`，也不采用复核建议的 `git merge-base` 祖先判定。
- 需要交接闸的形状只在两条非破坏性路径上渲染：显式单 zone zone-up，以及 keep 读到未收尾的交接形状。旧镜像在这两条路上由必填键在 MustLoad 拒启，rollout 卡住，旧 Pod 继续服务。
- 回档 Step 6 和全新 namespace 只渲染最终形状，不需要能力校验（修订稿 R1 / M1）。
- 剩下唯一一种「破坏性步骤之后才确定失败」的情形，是 Kafka 不能为不存在的 zone 组建出位点。由第 0 步 d 在停服前兜住，取决于 V3(c)。
- **代价**：能力证明只在「拒启」这一点上成立，不提供部署前的显式判定。DryRun 也看不出目标镜像有没有交接闸。

### 10.4 R7 保留

- 进程内的 `wait_own_behind_peer` 作为纵深防御保留，兜住手工部署、旧脚本和漏收尾。
- **代价**：两组都有位点、且对方更大时，本 zone 的消费会停住，必须人工处理，由 OwnBehindPeer 告警发现。漏收尾或 PITR 之后没删对方位点，都会触发这种情况。

### 10.5 F6：回档不再判组

- 附录 F6 要求把组判定和「仍是 legacy」检查挪到 Step 0。现在的做法是：回档一律以 zone 组为权威，Step 5 重置 zone 组并退役 legacy 位点，Step 6 显式传 zone。第 0 步不需要判组，只加了只读快照（c）和 Step 5 预演（d）。
- **代价**：行为有变化，legacy zone 回档之后会自动迁到 zone 组。

### 10.6 实现与规格的差异（以代码为准）

- **k8s_deploy**：
  - kafka-0 的 exec 用 `/bin/sh -c` 加 `KAFKA_HEAP_OPTS='-Xms16m -Xmx64m'`（§4.3）。
  - 全新集群首次 all-up 时只读探测 infra namespace，不存在就不 exec（§4.2）。
  - 显式模式在不部署 db 时 throw（§4.1）。
  - `ConvertFrom-LiveDbConsumerGroup` 按 YAML 路径取值，重复键会 throw（§2.2）。
  - manifest 缺注解占位时 throw（§4.5）。
  - 「keep + 交接形状 + 读不到 Kafka」和「keep + Y 仍有位点」两种情况，Source 都是 `live-configmap`，修订稿没有指定。
- **dev_tools**：`k8s-image-*` / `k8s-release-*` / `k8s-zone-rollback` 显式给了 `-DbConsumerGroup` 时拒绝，原规格写的是不转发（§4.6）。
- **告警**：Stuck 用 `min by`；多了第 5 条 SeedAborted（§2.7）。
- **go/db**：
  - `no_offsets` 用 Infof 打，文本带 `WARN ` 前缀。
  - 排空 select 里也处理了 yieldDone。
  - Setup 增加了 `setupDone`，gap 从上一个真实 session 算起，进程第一个 session 不记。
  - `await` 每轮先检查 `ctx.Err()`。
  - 守望从成功转为失败时只打一行日志。
  - `ValidateConsumerGroup` 允许「只有 ConsumerGroup」（§2.2）。
  - `closeHandoff` 在 Stop 的第⑤步（§2.4）。
  - 校验发生在 `NewKeyOrderedKafkaConsumer` 里，`db.go` 没有提前调用，但仍然早于开始消费。
- **stress_summarize**：`-Zone` 缺省为 0，缺省组仍是裸名（修订稿 R10，原规格是 1 / `_z1`）。
- **kafka_offset_reset**：重置计划里出现 ≥N 的多余分区也报问题；`--delete-offsets` 输出含 `GROUP_SUBSCRIBED_TO_TOPIC` 或 `actively subscribed` 时立即 throw。
- **merge_zone**：候选组为空、results 为空时都 fail-closed。

## 11. Kafka CLI 行为核对（4.3.1）

**状态：全部待实测。** 本节是 `deploy/k8s/README.md`「镜像钉版本」升级步骤第 4 步指向的记录处。

- **前置**：先按 README 的 V3 只读部分用 `docker buildx imagetools inspect` 核对 digest。拉镜像（`docker pull apache/kafka:4.3.1@sha256:77e3…`）属于下载，执行前要负责人确认。本机命名卷 `kafka-data` 可能是更旧版本写的，按 AGENTS §6.2 先清掉再起更稳妥。
- **镜像本身**：`docker image inspect apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837 --format '{{json .Config.Entrypoint}} {{json .Config.Cmd}}'` 应该输出 `["/__cacert_entrypoint.sh"] ["/etc/kafka/docker/run"]`；`docker exec kafka /opt/kafka/bin/kafka-topics.sh --version` 应该显示 4.3.1。
- **记录要求**：每项逐字记录 stdout、stderr 和退出码，再判断与下表「代码的假设」是否一致。不一致时，同批修改 lib 白名单（`Get-KafkaGroupMissingPatterns`）和 Go 白名单（`consumerGroupMissingPatterns`），并把原文追加为 fixture。

| 项 | 操作 | 代码的假设与依赖点 | 实测（原始输出 / 退出码） |
|---|---|---|---|
| V3(a) | 对 Stable 组执行 `--reset-offsets … --execute` | 打印 `Error: Assignments can only be reset if the group 'g' is inactive, but the current state is Stable.`，退出码可能是 0。`Assert-KafkaToolOutputClean` 两种都按失败处理。§6「已知局限」也取决于这一项：对非 Empty 组做 dry-run 是否同样被拒 | 待回填 |
| V3(b) | `--describe` 一个不存在的组 | 命中三条白名单之一，退出码不论；文案在 stdout 还是 stderr 都可以（调用方会合并两路）。依赖点：`Resolve-KafkaGroupTopicOffsets`、`committedOffsetPresent`、`Read-DbGroupTopicFacts` | 待回填 |
| V3(c) | 对不存在的组执行 `--reset-offsets --to-datetime … --execute`，之后 describe | 能建出位点，describe 有数值 CURRENT-OFFSET，并打印完整的 NEW-OFFSET 计划。依赖点：回档第 0 步 d 与 Step 5 | 待回填 |
| V3(d) | 组 Stable、但没有成员订阅该 topic 时执行 `--delete-offsets --group G --topic T` | 成功，之后 describe 不再有本 topic 的数值行。依赖点：回档 Step 5 退役 legacy 组（legacy 组上还有别的 zone 的成员） | 待回填 |
| V3(e) | 有成员订阅该 topic 时执行 `--delete-offsets` | 输出含 `GROUP_SUBSCRIBED_TO_TOPIC`（脚本也认 `actively subscribed`），被拒。依赖点：`Remove-RetiredGroupTopicOffsets` | 待回填 |
| V3(f) | `--reset-offsets --execute`（以及 `--dry-run`）是否打印 GROUP/TOPIC/PARTITION/NEW-OFFSET 表 | 两种都打印。依赖点：`ConvertFrom-KafkaResetPlan`、`Get-ResetVerificationProblems`。如果 --execute 不打印，所有 -Apply 都会 fail-closed，需要改成先 dry-run 取计划 | 待回填 |
| V3(g) | `kafka-get-offsets.sh --bootstrap-server B --topic T --time -2` 和 `--time -1` | 每行 `topic:partition:offset`。依赖点：`ConvertFrom-KafkaGetOffsets`、`Test-HandoffFinalizeReady` | 待回填 |
| V3(h) | Empty 组、没有任何已提交位点时 `--describe` | 不打表头，只有 `Consumer group 'g' has no active members.` 一行。依赖点：`Test-KafkaGroupNoActiveMembersText`、`consumerGroupNoActiveMembersPattern` | 待回填 |
| V3(i) | 不存在组的 describe 原文（与 (b) 同一操作，这一项只取逐字原文） | 判断是 `does not exist` 还是 `GroupIdNotFoundException` / `GROUP_ID_NOT_FOUND`，原文追加进两边的 fixture | 待回填 |
| V3(j) | 在 kind 里执行 `kubectl exec -n mmorpg-infra kafka-0 -c kafka -- /bin/sh -c "KAFKA_HEAP_OPTS='-Xms16m -Xmx64m' /opt/kafka/bin/kafka-consumer-groups.sh --bootstrap-server localhost:9092 --describe --group <g>"` | 按 R4 落码的形状核：命令成功，kafka-0 没有因此 OOMKilled，部署账号有 `pods/exec` 权限。依赖点：`Read-DbGroupTopicFacts` | 待回填 |

**若 V3(c) 证实不能为不存在的组建出位点，回档改动（PR-C 中 `k8s_zone_rollback.ps1` 的部分）不得发布，交负责人决策。** 不改用 `--to-offset` 或 `--from-file`：它们与 `--to-datetime` 走同一条组状态检查和 alterConsumerGroupOffsets 路径，换参数没有用。运行期另有两道 fail-closed：第 0 步 d 和 Step 5 的复核。

**其它未核实项**（也交 Codex）：

- **sarama v1.43.1 遇到不存在的组**：`DescribeConsumerGroups` 返回的是 Dead 加 ErrorCode 0，还是 `GROUP_ID_NOT_FOUND`？`ListConsumerGroupOffsets` 返回的是全部 -1，还是顶层 `GROUP_ID_NOT_FOUND`？代码两种都按「Dead / 全部 -1」处理（`peerViewFromDescription`、`saramaGroupInspector.CommittedOffsets`）。
- **go-zero v1.10.0**：缺必填键时报错、且文本含 GroupID；未知键被忽略。由 `TestPreHandoffBinaryRefusesZoneShapedConfig`、`TestPreHandoffBinaryIgnoresUnknownKeys`、`TestPreHandoffBinaryLoadsFinalZoneShape` 钉住，失败时保留 go-zero 的原始错误文本。S5 的 F9（旧二进制遇到未知键 `RetentionBytes` 是否忽略）是同一个 go-zero 行为。
- **其余验证**：V0、V1（go/db、go/shared/kafkautil、go/data_service 的 vet / build / test，`-run` 包含 `GroupHandoff|HeldPartitions|PeerView|RebalanceGap|ConsumerClientID|ConsumerSetup|ConsumeClaim|NewKeyOrderedKafkaConsumer|Inspector|Settled|StateUnknown|ReplayAfterSeed`）、V2（go/login、tools/merge_zone）、V4（PowerShell 契约测试：`k8s_deploy_contract`、`k8s_migrate_gate`、`k8s_gate_drain`、`k8s_client_entry_contract`、`dev_tools_merge_zone_contract`、`dev_tools_kafka_offset_reset`、`k8s_zone_rollback_gate_router_mode`、`k8s_zone_rollback_flush_guard`、`kafka_offset_reset_output_guard`，以及 `test_stress_summarize.ps1`）、V5（本地正向 → 收尾 → 反向 → 收尾 → 正向的交接集成，加 final(Z) 实例续消费、旧二进制拒启和守望用例）、V6（kind 演练）。完整命令和通过标准见修订稿 §13，以及 `batch-reports.md` A2 / B1 / B2 / B3 各节的「验证命令」。

## 12. Java 版

不适用：Java 版没有 Kafka 存盘链（AGENTS §12）。按规格要求，Java 仓库的 `PARITY.md` 要登记一行：「db_task 消费组按 zone 拆分 + 交接闸 + 回档落 zone 组 | <本仓库 commit> | — | 不适用（Java 无 Kafka 存盘链；官方 kafka-clients 如需可直接配 cooperative-sticky / group.instance.id）」。这一行由下一批统一登记，本批没有改 PARITY.md。

## 13. 残余风险

1. **全部是静态结论**：没有编译、没有测试、没有压测。暂停时长与 rebalance_gap 都没有实测基线。§11 的 CLI 与 sarama 行为都未实测，脚本一律按 fail-closed 写。
2. **重叠窗口不是互斥**（§3 (a)）：最长约 5s 加一轮再平衡；对方查询失败时守望不让出，看不见的重叠会更长。兜底是游标、排序锁和 owner_epoch，而游标缺键时会放行旧写（§3 (d)）。
3. **切组空窗取决于 legacy 组多久能稳定**：其它 zone 在滚动时，legacy 组会反复再平衡，最坏到 Rebalance.Timeout 60s，超过 login 的 TaskWaitTimeout 30s。所以必须冻结其它 zone 的发布。db 的 readinessProbe 不反映有没有过闸，滚动依赖 `minReadySeconds: 20` 留出的时间。
4. **R7 会停住消费**（§10.4），需要人工收尾。`wait_peer_unavailable` 在交接窗口里依赖 legacy coordinator 可达。将来开 Kafka ACL 时，要给本 zone 的 db 授 legacy 组的 Describe 权限，否则闸会 fail-closed。这个依赖只存在于收尾之前。
5. **收尾之后守望不再运行**：用旧发布物误部署时会从 Oldest 重放，只能靠 §8.4 的禁令。
6. **部署依赖 kafka-0 的 exec**：权限缺失或 kafka-0 不可用时，显式切组和全新 namespace 部署会被拒（fail-closed）。exec 只打 kafka-0。
7. **回档**：
   - Step 5 遇到残留订阅者会在停服之后失败，需要手工重跑。
   - 第 0 步 d 对「zone 组有活跃成员、没有位点」的零写入 zone 会误拒。
   - V3(c) 不通过时回档改动不得发布。
   - legacy zone 回档之后会自动迁到 zone 组，属于行为变化。
8. **合服**：两组都有位点时一律拒绝，合服前必须先收尾。
9. **Stuck 告警的盲区**：用 min by 时，只要还有一个已放行的副本，本条就不响；只剩 max by 的 Waiting（warning）。另外，旧 Pod 跑的是还没有这个指标的版本时，Stuck 可能在旧 Pod 仍在正常消费时响起，见告警描述里的按 pod 核对步骤。
10. **配置漂移**：
    - 「组不存在」的白名单有两份（Go 和 PowerShell），必须同步维护。
    - 组名基名在 db.yaml，k8s_deploy、回档、merge_zone 各自现读或写死（merge_zone 的 `defaultKafkaGroup`）；merge_zone 目前没有跨文件断言把它与 db.yaml 对上。
11. **并行改动风险**：与 S1 / S4 / S5 在 `key_ordered_consumer.go`、`metrics.go`、`k8s_deploy.ps1`、`db-task-retry-alerts.yaml`、`go-svc/db.yaml` 上并行修改。合并时按 §9 的挂点处理；同一工作区还有别的会话在改 `k8s_deploy.ps1`。
