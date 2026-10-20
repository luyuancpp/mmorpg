# 交接：存盘管线剩余代码落码（2026-10-15）

> **接手第一步**：读本文 §0–§4，再按 §3 的顺序干活。规格、复核、修订稿与各阶段报告都在 [godb-pipeline-impl-20261015/](godb-pipeline-impl-20261015/)。
> **上游交接**：[godb-pipeline-handoff-20261013.md](godb-pipeline-handoff-20261013.md)（§4 待办清单、§5 拍板项）与它的附录；[上线待做](../ops/godb-pipeline-launch-todo.md)。
> **为什么交接**：本会话的周额度用完，第 2 批落到一半中断（A3 已落但没交报告，A4 / A5 与第 3 批没开始）。

---

## 0. 给新账号的开场提示词（可直接复制）

```
你接手 go/db 玩家存盘管线剩余代码的落码。先走 AGENTS.md 会话启动门禁，然后读
docs/handoff/godb-pipeline-impl-handoff-20261015.md，按它 §3 的顺序做 R1 → R7。
规格在 docs/handoff/godb-pipeline-impl-20261015/：每项先读 *-spec.md，再读 *-review.md，
最后读 *-addendum.md，三者冲突时修订稿优先。已落阶段的报告在 batch-reports.md，
其中「留给第 3 批」是第 3 批要落的要点。
约束：你不编译、不测试（编译负责人自己做，测试交 Codex），交付时给出构建与测试命令；
不 commit / push；同一工作区有别的会话在写，改文件前先重读目标段落、只改自己的段落；
docs/PROGRESS.md 只追加。全部用中文，运行期日志文本用英文。
```

---

## 1. 背景与授权

负责人 2026-10-13 对上游交接 §5 的答复记在 `docs/PROGRESS.md`「2026-10-14 存盘管线接手…」条目。与本轮有关的：

- ① 读写分区对齐：授权，接受「db 积压超过约 8 秒时冷读进场失败」。
- ③ 消费组按 zone 拆：授权，是**开服门槛**（要支持单 zone 回档或合服）。
- ④ 重试只交给分区属主：先做，并修订 TiDB 决策 D5。④B2 换代 topic 排在条件写之后，本轮不做。
- WAITAOF（go/db 的入队点）、scene 发 DBTask 失败后的脏快照、Lua tonumber、db_task 的 retention 与 PVC：归本交接。
- `kafka:expand:status` 保留；② 批 B（压缩连接池）作废。

2026-10-15 负责人的原话：「你把代码做完给我就行了啊，其他的你不用管」。另一会话里还说过「编译的话我自己去做」。各规格的待决项都按规格里的推荐默认值拍板了，见各 *-spec.md 末尾「待决项」，其中几项重要的：
- ③ 拆成 4 个 PR 继续做。
- Kafka 镜像钉 `apache/kafka:4.3.1@sha256:77e3df9054047a88b520d0cc46e16696d3b22022e1d580aeccd2632df6532837`。
- 脏快照：下线收尾一律等 DBTask 回执。Kafka 不可用时，退出实体保留到 Kafka 恢复或停机看门狗到期。
- 保留期默认值：prod 2h / 3 MiB/s，prod-like 2h / 1 MiB/s，dev 1h / 256 KiB/s。PVC 不够时按盘收缩并 WARN。
- 本轮不关「一节点一 topic」。
- Codex 跑测试时可以联网下载 Go 模块（「同意 2」，2026-10-15）。

**不归本轮**：MySQL 行内围栏 / 条件写由「玩家消息投递与位置管理」会话落码（见 §4）；⑤ guild 扩展。

---

## 2. 状态

全部改动**未编译、未测试**。用户频繁把整个工作区打成快照提交，下表的已落代码基本都已入库（截至 `f0dadf5b0`），不要按「未提交」去找。

| 阶段 | 内容 | 状态 | 报告 |
|---|---|---|---|
| A1 | ① Go：`go/shared/kafkautil/dbtask_partition.go`（CRC32 契约与 golden 向量）、login 生产者改用契约分区器并删除一致性哈希、go/db 写分区审计 | 已落 | batch-reports.md §A1 |
| C1 | ① C++：db_task 显式 partitioner、`DbTaskKafkaKey`、golden 契约测试 | 已落 | §C1 |
| C2 | 脏快照（S5a）：DBTask 投递账本 `db_task_dispatch_{ledger,system}`、失败强制重存、下线等回执 | 已落 | §C2 |
| B4 | S4 脚本：`go_services.ps1` 先 Ctrl+C 再强杀；`stress_summarize.ps1` DB 段 2d / 2e / 2f | 已落 | §B4 |
| A2 | ③ Go：交接闸 `group_handoff.go`、播种、让出，配置四种形状，`kafkautil.CommittedOffsets` | 已落 | §A2 |
| B1 | ③ PR-A：Kafka 镜像四处钉版，`deploy/k8s/README.md`「镜像钉版本」 | 已落 | §B1 |
| B2 | ③ 脚本：`lib/kafka_consumer_groups.ps1`、`kafka_offset_reset.ps1`（重置后复核、-RetireGroup、-FinalizeHandoff）、`k8s_zone_rollback.ps1`（删 Step 2、第 0 步 c / d）、`dev_tools.ps1`（改为 hashtable splatting）、`stress_summarize.ps1` 参数块 | 已落 | §B2 |
| B3 | merge_zone：唯一权威组（③）、P4 / 审计用 SCAN 覆盖 `kafka:retry:queue:<topic>:p*`（④） | 已落 | §B3 |
| **A3** | S4 go/db：G1 治标、`ordering_lease.go`、Stop 五步、`metrics/pipeline.go` 打点、tonumber 修复、注释订正、9 个新测试文件 | **已落但未核实**：代理因额度中断，没交报告；文件已随 `de4960bee` 入库 | 无 |
| A4 | ④ + WAITAOF 的 go/db 部分 | **未开始** | — |
| A5 | S5b 保留期的 Go 部分 | **未开始** | — |
| 第 3 批 | k8s_deploy / 清单 / 告警（D1）、文档（D2） | **未开始** | — |

另外，L-04 第 1 批已于 `ebccaf04d` / `54c9d775d` 入库，验证清单在[上线待做](../ops/godb-pipeline-launch-todo.md) §2。

---

## 3. 剩余工作（按顺序）

go/db 这条线（R1 → R2 → R3）**必须串行**：三项都改 `key_ordered_consumer.go`、`config.go`、`metrics.go`、`db.go`、`etc/db.yaml`。R4 与 R1–R3 没有文件重叠，可以并行。R5 要等 R2 / R3 把配置键与指标名定下来。R6 在代码全部落完后做，R7 最后做。

### R1　核实并补完 A3（S4 go/db）
- 依据：`S4-g1-and-small-items-spec.md`（没有单独的复核，对照代码核实）。
- 逐条核对规格「逐文件改动」与「测试」两节，缺的补上。重点看这几处：
  - Stop 改成「先撤拉取再等 worker」之后，A2 的 `closeHandoff` 必须仍在 `consumer.Close()` 之后；
  - A2 交接闸在 Start 里等待时，要同时 select `pullStop` 和 ctx；
  - `metrics/pipeline.go` 的 step 与指标名，要和已入库的 `stress_summarize.ps1` DB 段逐字一致；
  - verify 已挪进 `runDBOp`，放在「复核之后、缓存回写之前」，`markAppliedSeq` / `markAppliedOwnerEpoch` 本身的顺序不变。要把新旧 Redis 步骤顺序写给条件写会话（§4）。
- 只改 S4 清单里的 go/db 文件。

### R2　A4：④ 重试只交给分区属主 + WAITAOF（go/db）
- 依据：`S1-retry-owner-waitaof-spec.md` → `-review.md` → `-addendum.md`（修订稿优先）。修订稿的主要改动：
  - WAITAOF 改为异步组提交，确认前不 ack；
  - `StartRetryConsumer` 纳入 `c.wg`；
  - 停机时被取消的入队计 `canceled`，不计 `error`；
  - nil 接收者安全；
  - 新增 `db_claim_acquired_total`；
  - `placement_route_test.go`、`retry_ownership_test.go` 移出清单。
- 文件：
  - 新增 `go/db/internal/kafka/{retry_routing,durable_enqueue}.go` 及其 `_test.go`；
  - 修改 `key_ordered_consumer.go`、`retry_ownership.go`、`store_failure.go`、`config/config{,_test}.go`、`metrics/metrics{,_test}.go`、`db.go`、`etc/db.yaml`；
  - 新增 `go/db/cmd/retry_requeue/main.go`。
- 在 R1 之上接入：保留各处 `pullStop` 分支；Stop 最后一步是 `enqueuer.close()`；属主判定看 claim 本身的生命周期（停机排空期间 claimCtx 不随会话取消）。
- merge_zone 的 p* 已由 B3 落完，键名要与 `retry_routing.go` 核对一致。

### R3　A5：S5b 保留期（Go）
- 依据：`S5-dirty-snapshot-retention-spec.md`，只做 S5b 的 Go 部分。
- 文件：`go/shared/kafkautil/topic_init{,_test}.go`、`go/db/internal/config/config.go`、`go/db/db.go`、`go/db/etc/db.yaml`、`go/login/internal/config/config.go`、`go/login/login.go`、`go/login/etc/login.yaml`。前面几步改过的地方只追加。

### R4　D1a：k8s_deploy.ps1 的 ③ 部分
- 依据：S2 三件套，重点是修订稿 §2 / §3 / §12，以及 batch-reports.md §B2「留给第 3 批」前 6 条。要做的：
  - `-DbConsumerGroup keep|zone|legacy`，只允许和 zone-up 一起用；
  - dot-source `lib/kafka_consumer_groups.ps1`；
  - 实现四种 ConfigMap 形状，`Resolve-DbConsumerGroupPlan` 决策表，`Read-DbGroupTopicFacts`（kubectl exec 进 kafka-0）；
  - F2 / F4 两处 throw；
  - keep 遇到交接形状时自动去掉 `GroupHandoffFrom`；
  - 契约测试按修订稿 §12，并在 `k8s_client_entry_contract` 的 Forwarded 清单里加 `DbConsumerGroup`。
- 回档脚本 Step 6 已经传 `-DbConsumerGroup zone`，依赖这里实现。

### R5　D1b：k8s_deploy.ps1 的 ④ 与保留期部分，清单与告警
- k8s_deploy.ps1：
  - ④：`Assert-DbReplicasWithinPartitionContract`（db 副本数 ≤ PartitionCnt），db ConfigMap 加 `RetryRouting` 与 `RedisClient` 的三项 WaitAOF；
  - 保留期：db / login ConfigMap 加 RetentionMs / RetentionBytes，Kafka PVC 按容量公式计算。
- 契约测试：新用例放在文件末尾新开的一节。
- `deploy/k8s/manifests/go-svc/db.yaml`：加 ③ 的两条注解（batch-reports §B2）和 S4 的 `minReadySeconds`。④ 修订稿已把这个文件移出 ④ 的清单。
- `deploy/k8s/manifests/infra/kafka.yaml`：只改 PVC 与保留期（B1 已改过 image 行）。
- 告警：
  - `deploy/k8s/db-task-retry-alerts.yaml` 加三组：④ 按修订稿共 7 条（含 `DbRetryPartitionedStranded`、`DbZoneNoPartitionClaims`）；③ 的 `db-task.consumer-group-handoff` 组 5 条（batch-reports §A2 / §B2）；S4 规格要求的租约与释放失败告警；
  - 新增 `deploy/k8s/db-task-partition-alerts.yaml`（S3 规格）。
- 每个告警表达式里的指标名都要到 go/db 代码里 grep 核实。

### R6　D2：文档（代码全部落完之后）
- **新增设计文档**：
  - `docs/design/db-retry-owner-routing.md`（④，含回滚前置）；
  - `docs/design/db-consumer-group-per-zone.md`（③，含 §11「Kafka CLI 行为核对（4.3.1）」的待实测清单）；
  - `docs/design/db-task-delivery-and-retention.md`（S5，§A 脏快照、§B 保留期）。
- **修改**，要点见各规格「逐文件改动」与 batch-reports 各阶段的「留给第 3 批」：
  - `global-data-layer-tidb-decision.md`（D5）
  - `db-task-kafka-partition-contract.md`、`ARCH.md`、`infra-reconnect-overview.md`、`cross-zone-readiness-audit.md`、`login-simplification-2026-04.md`
  - `zone_data_rollback{,_en,_zh}.md`、`merge-zone-runbook.md`、`server_merge_design.md`
  - `data-consistency-stress-testing.md`、`no-single-node-horizontal-scaling-20261001.md`（只追加）、`db-task-pipeline-bottlenecks.md`
  - `kafka-cluster-production-runbook.md`、`shared-redis-split-by-purpose.md`、`release-checklist.md`、`tidb-phase1-and-launch-readiness.md`
  - [上线待做](../ops/godb-pipeline-launch-todo.md)：补 ①③④、脏快照、保留期的验证与部署项
- **登记**：
  - `docs/README.md` 登记三份新设计文档，与 `gen_docs_index.py` 的生成结果逐字一致；
  - `docs/PROGRESS.md` 只追加；
  - Java 仓库 `D:\luyuan\xuanming-server-mmo\PARITY.md` 末尾追加行，①③④、WAITAOF、钉版、保留期都是「不适用（Java 没有 Kafka 存盘链）」；脏快照按 Java 的同步写库路径核实后再登记。

### R7　语义复核（不做编译可行性复核）
负责人自己编译，所以复核只看编译查不出来的问题：
- 并发与生命周期：sarama 再平衡与 claim 生命周期、WAITAOF 异步确认与位点提交、停机五步、C++ 投递报告回调回主循环、实体销毁。
- 数据一致性：旧写覆盖新写、重放幂等，以及 Redis 丢 AOF 尾巴时游标与收据一起回退。
- 滚动升级新旧并存：交接形状与最终形状、旧镜像回滚、p* 滞留。
- 回档与合服脚本：破坏性步骤之前是否已经 fail-closed。

复核按条目分组做，每组一个复核员，额度紧时先复核 ③ 和 ④。

---

## 4. 协作约束（截至 2026-10-15 晚）

- **「玩家消息投递与位置管理」会话**：负责 MySQL 条件写。它在等两件事：go/db 这批落完后的文件清单，以及 proto2mysql `SaveConditional` 经 Codex 验证并合入。已承诺落完后发清单给它。清单里要单独标出：
  - verify 被挪进 `runDBOp`（R1）；
  - Stop 改成五步；
  - `luaDecimalGreaterFn`，条件写要比较 uint64 时复用它，不再写 tonumber。

  另外答应过它：Codex 的 proto2mysql 结果一回来就通知它（任务单 [godb-pipeline-codex-tasksheet-20261013.md](godb-pipeline-codex-tasksheet-20261013.md)，第 2.1 版，结果还没回来）。
  **该会话已于 2026-10-15 晚交接**，交接文档是 [handoff-landing-recovery-and-fencing-20261016.md](handoff-landing-recovery-and-fencing-20261016.md)，§3.4 记的是双方约定。之后的文件清单和 SaveConditional 验证结果，发给它的接手会话：名字问负责人，或用 ListAgents 查。
- **「全局单例扩容交接」会话**：在改 `k8s_deploy.ps1`，范围是 `New-NodeConfigMapYaml`、C++ Pod 模板注解、`Apply-Infra` 的 `$infraManifests`、新增「Redis 连接总账」一节、`-ConfirmRedisCoreRestart`，还给 `Get-ManifestDeploymentReplicas` 加了可选参数，缺省不变。它也在改 `dev_tools.ps1` / `k8s_image.ps1` 的透传，以及契约测试的 prod 段、battle infra 段和 MySQL 总账用例附近。它的 C++ Kafka 改动已结束：`kafka_producer.h` 的 `brokersConfigured()` 要保留，`kafka_manager.*`、`kafka_consumer.cpp`、`node.cpp` 都不碰。
- **launch-readiness 会话**：k8s_deploy.ps1 只剩 `Write-MysqlConnectionLedger` 一个函数在改。契约测试里避开用例 15（「G4b-2 前置…」）和第 2479 行附近的 Secret dry-run 断言。
- 新开的桌面版会话可能跑在 `.claude/worktrees/<名字>` 里，这时 Edit 改不了主工作区，只能导出补丁交付。见记忆 shared-worktree-concurrent-sessions。

---

## 5. 风险与待验证（交付时要写进说明）

- **golden 向量**：45 条 CRC 值取自规格（附录作者与复核各自手算），没有运行代码算过。由 Codex 用 Go crc32、sarama、librdkafka 三方机械复核，不一致时回报，不要改表。
- **librdkafka 版本**：C1 加了 `rd_kafka_version()==RD_KAFKA_VERSION` 断言，`bin/rdkafka.dll` 的版本没核实。
- **Kafka 4.3.1 CLI 行为（V3）**：describe 不存在的组时的文案、对不存在的组 `--reset-offsets --to-datetime` 能否建出位点、`--delete-offsets` 的行为，都还没实测。**如果实测建不出位点，回档脚本的改动（B2 里的 `k8s_zone_rollback.ps1`）不得发布**，交负责人决定。
- **行为变化**（A1）：
  - key 校验 fail-closed；
  - `SendToTopic` / `SendToTopicPartition` 拒绝 db_task topic；
  - 删掉 safego 点位与「全部分区不可用」快速失败；
  - db 积压超过约 8 秒时冷读进场失败（tip 3023）。
- **行为变化**（C2）：Kafka 长时间不可用时，周期存盘不再被快路径跳过，每周期整份写一次 Redis；退出实体重存 3 轮后保留。
- **回滚前置**：回滚目标提交必须包含 `d5cbb8a8e`（`git merge-base --is-ancestor`）；④ 的回滚规程见修订稿（先切 shared，`retry_requeue -check` 确认 p* 为 0）。
- **上游未完成项**：proto2mysql 的验证（任务单任务 A / B）还没回来，条件写因此没法开工。

---

## 6. 构建与测试（给负责人 / Codex）

各阶段的具体命令与通过标准见 batch-reports.md 各节的「验证命令」。全部落完后按下面的顺序跑：

1. **Go**：环境变量照任务单 §1（`GOTOOLCHAIN=local`、`GOFLAGS=-mod=readonly`、`GOPROXY=https://goproxy.cn,direct`）。依次在 `go/shared`、`go/db`、`go/login`、`tools/merge_zone` 下跑 `go vet ./...`、`go build ./...`、`go test ./... -count=1`；go/db 的 kafka 包需要回环网络（miniredis）。`go/test.ps1` 的 L1 列表已加入 `shared/kafkautil` 与 `login/internal/kafka`。
2. **C++**：MSBuild 串行 `/m:1`，先编 scene 库（CMakeLists.txt / scene.vcxproj 已登记新文件），再编 `kafka_command_test`、`cross_zone_test` 并运行。
3. **PowerShell**：跑 `tools/scripts/tests/` 下本轮改过和新增的 `*.tests.ps1`，以及 `test_stress_summarize.ps1`。
4. **上线前**：按[上线待做](../ops/godb-pipeline-launch-todo.md)与各设计文档的 rollout 节执行，比如 kcat 抽样核对 C++ 写消息守契约、V0 核对现网 broker 版本、本机双 db 实例复验。

---

## 7. 2026-10-16 ~ 10-18 接手会话进展（Claude，全部未编译、未测试；2026-10-18 收尾定稿）

> 接手会话按 §3 做完 R1 → R7，另加 R6b（④ 文档与 R7 同步）和 R8（回档语义落码），用工作流分批落码并逐批对抗复核。代码大部分已随用户快照 `e76f6f747`、`e7bb46070` 等提交入库；之后的改动看工作区。协议生成、编译、测试、冒烟全部交 Codex（负责人 2026-10-18：「你只实现代码，协议生成 编译冒烟交给 codex」）。

### 7.1 已完成

| 项 | 内容 | 关键结论 / 偏离 |
|---|---|---|
| R1 | 核实 A3（S4 go/db）并补修 | A3 基本落全。新发现并修复：停机第③步 `consumer.Close()` 抢 sarama `consumerGroup.lock`，与第④步互等。修法：`claimAcker.untrack`、三处 select 加 pullStop 分支、第②步超时先 `c.cancel()`。偏离三处：consumeCtx 随撤拉取取消；runDBOp 在 sqlCtx 已取消时返回 `context.Cause(sqlCtx)`；`errOrderingLeaseLost` 文本改了 |
| R2 | ④ + WAITAOF（go/db） | 新文件 `durable_enqueue.go`、`retry_routing.go`、`cmd/retry_requeue`；Stop 六步。偏离：再平衡、让出、失败收尾时，等到的异步确认不提交位点（修订稿 §1.6 说会提交），只有停机排空会先等再提交 |
| R3 | 保留期 Go | `TopicSpec.RetentionBytes`，db / login 的 `KafkaConfig.RetentionBytes` |
| R4 | k8s_deploy ③ | 四种 ConfigMap 形状、决策表、kafka-0 exec（带小堆）、全新集群探测、db.yaml 注解占位 |
| R5a / R5b | 部署与告警 | Kafka 盘预算（行为变化：非 dev 缺 zones 配置时拒绝）、`minReadySeconds`、四组告警与新告警文件、副本数门禁、ConfigMap 镜像 RetryRouting / WaitAOF / `Fencing.Mode`；Redis 总账 db 行 `Fixed=2` |
| R6a | ③ / S5 两份新设计文档与 ①③S4 订正 | `db-consumer-group-per-zone.md`、`db-task-delivery-and-retention.md`，另订正 14 份文档 |
| R7 | 全批语义复核（5 个复核面 × 对抗复核）与修复 | go/db 9 条：双属主窗口可见化、滞留收据重搬表、认领与 ack 的 nonce 防重、重新入组退避、`retry_requeue` 退出码 4/5 与 `-zone` 必填。C++ 3 条：停机清单改在 Node 拆除前钩子里做；永久 Kafka 错误不再无限期保留实体。脚本 8 条：交接闸能力校验、禁止 db_task 同名重建、第 0 步 e、按 TopicGeneration 推导 topic、合服 P3 全覆盖等 |

### 7.2 负责人裁决（2026-10-17）

- **回档语义 = 丢弃 TargetTime 之后的写**。落法：Step 5 改用 `-ToLatest`；Step 4 / 5 之间盘点并拦下本 topic 的重试收据；不清游标。还要核实共享 DB0 里本 zone 玩家缓存的处理，避免读缓存再存盘把回档抵消。**脚本尚未落码（R8）**。
  - 2026-10-18 补：R8 脚本侧已落码（未编译、未测试，待 Codex 验证），本 zone 玩家缓存按 PITR 之后的库由回档工具在 Step 4b 删除、5b 复核；同日回档复核 [0]–[11] 已修。读 MySQL / 写 Redis 的回档工具 `go/db/cmd/zone_rollback_discard` 尚未落地，`-Apply` 不可执行。流程与待拍板事项（#1 / #3 / #4 / #6）见 `docs/design/zone_data_rollback.md`「2026-10-18 修订」，上线待做见 `docs/ops/godb-pipeline-launch-todo.md` 6.5。

### 7.3 后来完成的（2026-10-18）

- **R6b**：新建 `docs/design/db-retry-owner-routing.md`；④ 周边文档（D5 修订、release-checklist 回滚前置、shared-redis 键清单、partition-contract、launch-todo §9）；告警新增 `DbRetryStrandedReceipts` 和 `DbConsumerGroupHeartbeatErrors`；R7 三块修复的文档同步。复核出 7 条，均已修。
- **R8**：`k8s_zone_rollback.ps1` 按「丢弃 T 之后的写」重写：
  - Step 5 改用 `-ToLatest`；去掉 FLUSHDB；
  - 新增第 0 步 f（部署事实 + Redis run_id + 工具版本核对）、Step 1b（停服与静默核对、行指纹基线）、4a / 4b / 5b（交给回档工具）；
  - 新测试 `k8s_zone_rollback_discard.tests.ps1`，以及共用沙箱 `tests/lib/k8s_zone_rollback_sandbox.ps1`；删除 `k8s_zone_rollback_flush_guard.tests.ps1`；
  - `ConvertFrom-LiveDbConsumerGroup` 从 k8s_deploy.ps1 移进 lib。

  复核出 12 条，均已修。文档 `zone_data_rollback{,_en,_zh}.md`、`db-consumer-group-per-zone.md` 都已改为「已裁决」。
- **登记**：`docs/README.md` 手工插入三份新设计文档。没有整段跑 gen_docs_index.py，因为那会改掉别的会话手写的条目；生成器 `--check` 里另外几处差异不是本线造成的。另外追加了 PROGRESS 条目和 Java `PARITY.md` 7 行。

### 7.4 没做的（负责人 2026-10-18 已授权自行拍板；R9 设计已出，未落码）

> **2026-10-19 现状**：R9 设计已写进 [zone_data_rollback_discard_tool.md](../design/zone_data_rollback_discard_tool.md)，包括 7 项与三个小项的决定、完整设计、9 个落码批次、测试和风险。设计预计改动 **73 个文件**，超过 AGENTS §10.2 的 30 文件门槛，**落码前须负责人确认规模**。两面对抗复核共 28 条意见，其中 7 条 high，**尚未并入**，列在该文 §6；落码第一步就是并入这些意见。另外，维护围栏和三个小项这两路专项调研因 403 认证错误没跑完，落码前要补一轮核实。
>
> **2026-10-19 负责人授权**（原话）：「同意按 73 个文件、9 批的规模开工 R9：先并入 28 条复核意见并补齐缺的两路调研，再按 B1→B6 分批落码，每批交 Codex 验证。」
>
> **2026-10-19 设计 v3**：两路调研已补；28 条复核意见已并入（v2）；之后按「尚未上线」范围收窄（§6.3，删掉的内容移入 §8「上线后再补」），复查轮 11 条也已处理（§6.4）。**总文件数 109，比授权多 36，未落码，等负责人定规模。** 按存盘围栏设计「上线状态」一节，回档 `-Apply` 在上线前不是阻塞项。109 个文件里属于上线前该修的只有 B4（死信去重，15 个）和 B5（探针剥离与 op="other" 告警，8 个），再加 B4 依赖的 B1 中 `dead_dedup.go` / `dead_dedup_test.go` 2 个，共 25 个。持久副本目录（`-ArchiveDir`）放在哪是运维部署问题，代码里只是一个必填参数，不阻塞落码。
>
> **2026-10-19 负责人换账号继续**：R9 的后续工作交接在 [zone-rollback-r9-handoff-20261019.md](zone-rollback-r9-handoff-20261019.md)。

> 负责人 2026-10-18 原话：「你就按最标准的去做就行，数据一致性而且可水平扩展」。下面 7 项与三个小项，由接手会话按业界标准做法拍板并落码（R9）。取舍顺序：正确性 / 数据一致性 / 向后兼容 > 简单 > 可维护 > 性能；要求可水平扩展，不引入全局单点，可分片、可断点续跑。R9 的决定与设计会写进 `docs/design/zone_data_rollback.md`。

1. **回档工具 `go/db/cmd/zone_rollback_discard`（未写）**。在它落地之前，K8s 整 zone 回档只能 dry-run，`-Apply` 停在入口校验。工具契约写在 `k8s_zone_rollback.ps1` 文件头「zone_rollback_discard 契约」一节，依赖以下待拍板项（详见 `docs/design/zone_data_rollback.md`「2026-10-18 修订」）：
   - #1 回档窗口的 zone 维护围栏是前置条件；他区在线的本 zone 玩家是踢下线还是等他们下线；
   - #2 回档是否同时做一次纪元抬升（INCR owner_epoch、抬高 applied_epoch），这要 fencing 负责人签字；
   - #3 严格屏障协议（strict）下怎么处理，以及「有 pending、无 ACK」的玩家；
   - #4 blocked 玩家是隔离保留还是删除；T 之后建的角色能否以空骨架继续使用；
   - #5 接受有效回档点比 T 早一个消费延迟；
   - #6 T 之后合入或迁入本 zone 的玩家，是否一律拒绝回档；
   - #7 放弃 unchanged 优化，接受回档后全部走冷读带来的 MySQL 负载尖峰。
2. 死信按 (topic, partition, offset) 去重，副本数设硬上界：与 L-04 第 2 批死信工具一起定。
3. `dirty_save::IsEqual` 是否剥离 `stress_test_probe`。
4. 分区契约告警是否给 op="other" 补规则。

### 7.5 已知未同步的旧描述（不在本线范围）

以下几处仍写着旧的 FLUSHDB 防护或 `-ConfirmFlushSharedRedis`，由各自负责人更正：
- `docs/design/db-write-fencing-mysql.md` 约 :1030；
- `docs/design/player-route-mapping-durability-proposal.md` 约 :453；
- `docs/ops/merge-zone-runbook.md` 约 :933；
- `docs/design/shared-redis-split-by-purpose.md` 的相关段。

另外 `db-task-store-unavailable-deferral.md` §9 的 WAITAOF 与 ④ 两行，同样交负责人更正。

### 7.6 交 Codex 的验证顺序（本线全部改动）

1. **Go**（`GOTOOLCHAIN=local`、`GOFLAGS=-mod=readonly`、`GOPROXY=https://goproxy.cn,direct`）：依次在 `go/shared`、`go/db`、`go/login`、`tools/merge_zone` 下跑 `go vet ./...`、`go build ./...`、`go test ./... -count=1`。go/db 的 kafka 包要跑 -race，正则见 `docs/ops/godb-pipeline-launch-todo.md` §9.2。
2. **C++**：MSBuild 串行 `/m:1`。`node.h` 改了，所以 core 和 scene / gate / battle 全部节点都要重编，然后编 `cross_zone_test`、`kafka_command_test`，再跑：
   - `cross_zone_test --gtest_filter=DbTaskDispatch*:ExitDbTaskGate*:ExitPersist*`
   - `kafka_command_test --gtest_filter=DeliveryErrorClassification*:ClassifyProduceResult*`
3. **PowerShell**：在仓库根逐个执行 `pwsh -NoProfile -NonInteractive -File tools/scripts/tests/<名>.tests.ps1`，名字依次为：
   - `k8s_deploy_contract`
   - `k8s_client_entry_contract`
   - `k8s_migrate_gate`
   - `k8s_zone_rollback_discard`
   - `k8s_zone_rollback_gate_router_mode`
   - `kafka_offset_reset_output_guard`
   - `dev_tools_kafka_offset_reset`
   - `dev_tools_merge_zone_contract`
   - `go_services_stop`

   另跑 `test_stress_summarize.ps1`。
4. **回档 dry-run 冒烟**：命令与期望见 `docs/ops/godb-pipeline-launch-todo.md` 6.5。
5. **上线前实测**：Kafka 4.3.1 CLI 行为，见 `db-consumer-group-per-zone.md` §11 V3。如果证实不能为不存在的组建出位点，回档改动不得发布。
6. 各项通过标准与失败时要保留的日志，见 launch-todo 各节。只有真库用例才算真库验证通过，显示 SKIP 不算通过。

### 7.7 与其它会话

- **「玩家数据存储位置」会话**：2026-10-18 在 go/db 落完 B0-3「task:result 读回执按 login 实例分频道」第 2 批（含评审修复，未编译、未测试），空档已收回。文件清单（对方回报）：
  - `go/db/internal/kafka/key_ordered_consumer.go` 只改了三处：
    - 常量块：删去 `taskResultNotifyChannel`；
    - `publishDBReadResult`：调用 `publishTaskResult` 的那一行改为传 task；
    - `publishTaskResult` 本体：先 LPUSH + EXPIRE，再按 `reply_instance` 只 PUBLISH 一次；合法发实例频道，空或不合法回落旧频道；通知失败仍 return nil。

    R7 的 ConsumeClaim、Errors 协程、重新入组退避，以及缓存相关 hunk 都没碰。
  - `go/db/internal/kafka/placement_route.go`：`failRead` 改一行，传 dbTask。
  - 新建：
    - `go/db/internal/kafka/` 下的 `task_result_notify.go`、`task_result_notify_route_test.go`、`task_result_notify_test.go`；
    - `go/db/internal/metrics/` 下的 `task_result_notify.go`、`task_result_notify_test.go`。新指标 `db_task_result_notify_total{target,result}` 预置 3×3 个 0，带 sync.Once。
  - `go/db/internal/metrics/metrics.go`：只在 `Start()` 里 `registerPipeline()` 之后加了一行 `registerTaskResultNotify()`，另补头注释；预建序列和 metrics_test.go 都没动。
  - 部署与脚本：
    - `deploy/k8s/task-result-notify-alerts.yaml` 是新建的独立 PrometheusRule；
    - `go/test.ps1` 的 db 套件加了 `./internal/metrics/...`，需要 Codex 先确认稳定为绿；
    - `tools/scripts/stress_summarize.ps1` 新增 2g 段，`test_stress_summarize.ps1` 同步改了。
  - 文档：
    - `docs/ops/godb-pipeline-launch-todo.md` §10；
    - `new-player-first-enter-no-cold-read.md` 三处更正注；
    - `docs/handoff/launch-readiness-20261013/spec_B0-3_task_result_channel.md` 文末「第 2 批交付」节，含权威验证命令；
    - `deploy/k8s/README.md` 的告警安装说明。
  - 上线顺序：login 已经上了第 1 批的 zone，go/db 要按 zone 金丝雀发布，不要一次升全部 zone。
- **「D1 玩家归属与存储持久化」会话**：2026-10-19 在 go/db 落完 D1 第 3 批「读方门禁」，未编译。对方回报的改动：
  - 新文件 `internal/kafka/route_ready_gate.go`（读方接口与 go-redis 实现、lookupReady、routeNotReadyError、进程级就绪闸、启动探针、装配函数），以及 `route_ready_gate_test.go`、`route_ready_integration_test.go`；
  - `placement_route.go`：lookup 与 confirm 共用一个读取入口。`Placement.RequireRouteReady=true` 时走 `placement.RouteReadyReadScript`。新哨兵 `errRouteNotReady` 写进了 `retryBudgetExempt`，读任务不豁免；复核时按「路由变了」处理；
  - `store_failure.go`：新增延后原因常量 `route_not_ready`，没改既有判定顺序；
  - `key_ordered_consumer.go`：两处查闸，一处在 ConsumeClaim 取消息前，一处在重试循环采样后、分拣认领前；另有 deferFailedTask 的一个 DEBUG 分支和装配传参；
  - `config.go` 的 Placement 段加了两个字段；`etc/db.yaml` 的 Placement 段同步；
  - `metrics.go` 追加 `db_route_ready_gate`、`db_route_ready_verdict_total` 和 outcome `route_not_ready`，只在开关打开时才注册和预置。
  - **未核对**：本线提醒过「就绪闸要能被 pullStop / ctx 打断，并保留重新入组退避」，对方还没评审核对，已登记在 PROGRESS 2026-10-19「D1 剩余批次落码完成」的未做项。Codex 验证 go/db 时，要重点跑停机用例（`consumer_shutdown_test.go`），确认开关打开后 Stop 不会被闸卡住。

### 7.8 与「落地恢复与围栏」会话（已停，交其接手方）

- 它加了 `ServerConfig.Fencing.Mode`（legacy | dual，缺省 legacy），k8s_deploy 原样镜像，没有逐 zone 覆盖。
- 它已把 handleTask 成功分支改成 `advanceApplied`（`player_cache_publish.go`），`metrics.RedisSteps` 扩到 12 个，`stress_summarize` 2e 段已同步。
- **未送达的核对请求**（发出时对方会话已停）：
  - `advanceApplied` 与 R7 改成 nonce 防重的 `ackRetryReceipt`，先后顺序是否正确；
  - 围栏 v0.4 的被拒写进死信时，应复用 R2 的 `persistBeforeAck`；
  - `db-task.ordering-lease` 告警里的 `lease_verify` / `verify_error`，是否随令牌发布脚本改了语义。
- R7 在 go/db 动过的函数：ConsumeClaim（带名字的返回值）、Start（`consumer.Errors()` 协程）、`requeueRevokedRetry` / `moveClaimedRetry` / `moveClaimedRetryToDead` / `deferFailedTask` 重试分支 / `deadLetterRetry`（登记待重搬）、`claimFrom` / `ackRetryReceipt`（nonce）、消费循环（重新入组退避）。新文件：`consumer_group_errors.go`、`retry_stranded.go`、`rejoin_backoff.go` 及各自测试。
