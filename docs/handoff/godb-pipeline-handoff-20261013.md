# 交接：玩家存盘管线（go/db）瓶颈修复与条件写（2026-10-13）

> 交出方：Claude（会话 2026-10-12 ~ 10-13）。接手方：新账号的 Claude；编译 / 测试 / 压测仍由 Codex 执行（AGENTS.md §10.1）。
> 本文是**做到哪、剩什么、怎么验**的清单；推导与证据在 [db-task-pipeline-bottlenecks.md](../design/db-task-pipeline-bottlenecks.md)（下称「瓶颈文档」），各项修复的逐文件方案与复核意见在 [附录](godb-pipeline-handoff-20261013-appendix.md)。
> 所有改动**未编译、未测试**（本机没有 Go 工具链）。

---

## 0. 接手第一步

1. 走 AGENTS.md「会话启动门禁」。`docs/PROGRESS.md` 只读末尾这几条：`2026-10-12 补充:actor 框架定论…`、`2026-10-12 补充:go/db 写路径 Redis 往返结论…`、`2026-10-13 补充:proto2mysql 新增 SaveConditional…`，以及本条之后追加的交接条目。
2. 读瓶颈文档 §0（结论速览）、§4（不停服转移的缺口）、§5（修复计划）、§6.6（proto2mysql 原语）。
3. 先看 §2「工作区与并行会话」——工作区里有大量**别的会话**的未提交改动，其中一块和本文待办 ② 重叠。
4. 第一件事是把 §3 的验证命令交给 Codex，**拿到结果之前不要在这些文件上继续叠改动**。

---

## 1. 已定结论（不要再翻案）

| 结论 | 出处 |
|---|---|
| **不引入 actor 框架**（ergo-services/ergo、asynkron/protoactor-go 都不用）。actor 能替代的「进程内同 key 串行」已有 SubShard / 行锁 / C++ 单线程；真正缺的是存储侧 fencing，框架不提供。 | 瓶颈文档 §1；[guild-actor-architecture.md](../design/guild-actor-architecture.md) §5.3、§16 #2 |
| 效率瓶颈在 IO 不在派发（R19 t18m：派发 0.0ms、kafka_send 4.1ms、等 db 回调 31.8ms）。 | 瓶颈文档 §2 |
| 水平扩展继续用 Kafka 消费组 + 按 key 固定子 goroutine + 存储侧条件写。 | 瓶颈文档 §3 |
| 写路径 Redis 往返（11 次）**现在不优化**：先补测量；正确性先做条件写；往返合并等排空超 SLO 再做。 | 瓶颈文档 §6.1、§6.3 |
| 条件写的库原语用 proto2mysql 的 `SaveConditional`（不是 C 方案原稿里的 `UpdateByPrimaryKeyContext` / `InsertContext`）。ctx 用 `WithContext` 传入，库不用改。 | 瓶颈文档 §6.6 |

---

## 2. 工作区与并行会话（重要）

- 主仓库 `D:\luyuan\xuanming-server-mmo-cpp` 停在 `main` / HEAD `73b852f32`，**本会话的改动全部未提交**，并且和其它会话的未提交改动混在同一个工作区里（`git status` 有一百多个文件）。
- **本会话改动的文件**（只有这些是本会话的，提交时只 `git add` 这些）：

  | 文件 | 性质 |
  |---|---|
  | `go/db/internal/kafka/key_ordered_consumer.go` | 只有 subShardIndex / spreadKey 一处（`routeToSubShard` 附近，+21 行） |
  | `go/db/internal/kafka/subshard_route_test.go` | 新增 |
  | `go/login/internal/kafka/key_ordered_producer.go` | 只有 markPartitionUnavailableLocked 三处 |
  | `go/login/internal/kafka/key_ordered_producer_lock_test.go` | 新增 |
  | `tools/scripts/kafka_offset_reset.ps1` | Assert-KafkaToolOutputClean 与调用处 |
  | `tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1` | 新增 |
  | `tools/scripts/stress_summarize.ps1` | DB 段 label 顺序 |
  | `tools/scripts/test_stress_summarize.ps1` | DB 段回归用例 |
  | `docs/design/db-task-pipeline-bottlenecks.md` | 新增 |
  | `docs/design/guild-actor-architecture.md` | §5.3、§15、§16 #2、§17 第 1 条 |
  | `docs/handoff/godb-pipeline-handoff-20261013.md`、`…-appendix.md` | 新增（本文与附录） |
  | `docs/README.md` | **混合**：本会话只加了 db-task-pipeline-bottlenecks 与本文两行，其余新增行是别的会话的 |
  | `docs/PROGRESS.md` | **混合**：本会话只追加了 2026-10-12 / 10-13 的四条（见 §0），其余是别的会话的 |

  这些文件在交接时核对过：除标「混合」的两个文档外，diff 只有本会话的改动。

- **与其它会话重叠的地方——动 ② 之前必须先看**：另一个会话（标记「G2b」，2026-10-12）在工作区里已经做了 MySQL 连接池指标与连接总账，同样未提交、未验证：
  - `go/shared/sqlpoolstats/`（新增）、`go/db/internal/metrics/metrics.go` 的 `RegisterSQLPools`、`go/db/db.go`、`go/data_service/internal/store/dbstats.go` 等；
  - `tools/scripts/k8s_deploy.ps1` 的 `Get-MysqlConnectionPlan` / `Get-MysqlConnectionLedger`，`deploy/k8s/manifests/infra/mysql.yaml`、`go-svc/db.yaml`、`go-svc/data-service.yaml` 的连接上限注释，`java/gateway_node/.../application.yaml` 的 Hikari 配置。
  - 这覆盖了待办 ② 批 A 的「导出 DBStats」和批 B 的「连接预算门禁」大半。规格见 [launch-readiness-20261013/spec_G2b.md](launch-readiness-20261013/spec_G2b.md)，它**明确不做**「1040 不耗重试预算」（登记为 L-04，见 `docs/design/tidb-phase1-and-launch-readiness.md`）。所以 ② 剩下**没人认领**的是：写任务遇 1040 / 连接不可用时延后、不耗重试预算（`db_task_deferred_total`）以及死信 / 重试队列告警。
- **同日另外两份交接，与本文有重叠，排期时合并、不要各做一份**：

  | 交接 | 与本文重叠的条目 | 建议归属 |
  |---|---|---|
  | [handoff-launch-readiness-and-d1-20261013.md](handoff-launch-readiness-and-d1-20261013.md)（开服首日瓶颈、D1 映射持久化，规格在 [launch-readiness-20261013/](launch-readiness-20261013/)） | G2b 连接池与总账（= 本文 ② 大半）；其 §4 第 10 项「同玩家串行缺口：MySQL 条件写」= 本文 §4 第 1 项 | G2b 归那边；条件写归本文（库原语已落码） |
  | [handoff-global-singletons-10m-ccu-20261013.md](handoff-global-singletons-10m-ccu-20261013.md)（全局单点与 1000 万在线容量） | P0「go/db 连接类错误耗重试预算进无消费者死信」= 本文 ② 剩余 / G2；P1「db 消费组全 zone 共用」= 本文 ③；P1「每次存盘 5 条 SQL 4 条无效（未开 clientFoundRows）」；「go/db 每任务 11.5 条命令合并」被它列为目标规模的硬前置 | 由负责人定；注意下面两点冲突 |

  - **往返合并的优先级有分歧**：本文按现网实测（R19 lag 0）结论「先测量、暂不做」；那份按 1000 万在线目标的静态推算认为共享 Redis 62% 命令来自 go/db，是目标规模的硬前置。两者不矛盾——**目标规模定下来之后**，往返合并（瓶颈文档 §6.3 的 3a/3b）应提前，但仍按「先条件写（3a 删掉锁/游标/epoch 往返），后合并（3b）」的顺序。
  - **clientFoundRows**：那份建议 go/db 的 DSN 开 `clientFoundRows=true` 来省掉 Save 的无效 SQL。开了之后 `SaveConditional` 的「条件成立但整行没变」会报 `SaveUpdated` 而不是 `SaveUnchanged`，正确性不受影响；go/db 侧的分类逻辑不要依赖 `SaveUnchanged` 一定出现。
- 其它会话还在改 `go/db/etc/db.yaml`、`go/db/internal/config/config.go`、`go/db/internal/logic/pkg/proto_sql/store_registry.go`、`go/login/**`、`go/data_service/**`、`tools/scripts/k8s_deploy.ps1` 等。**不要回退、覆盖或顺手整理它们**（AGENTS.md §10.2）。

- **proto2mysql**：本机 checkout 在 `D:\luyuan\proto2mysql`，分支 `feat/conditional-save` 已推送（commit `e81a1e7`，[GitHub](https://github.com/luyuan-cpp/proto2mysql/tree/feat/conditional-save)）。main 未动、未打 tag。负责人授权过在这个库 commit + push（它是负责人自己的库）；**打 tag 前仍要问负责人**。主仓库的 go.mod 仍钉 v0.2.0。

---

## 3. 第一件事：交 Codex 验证已落码的部分

### 3.1 主仓库第 0 批（工作目录 `D:\luyuan\xuanming-server-mmo-cpp`）

| # | 工作目录 | 命令 | 通过标准 |
|---|---|---|---|
| a | `go/db` | `go test ./internal/kafka/ -run 'TestSpreadKey\|TestSubShardIndex\|TestRouteToSubShard' -count=1 -v` | 5 个用例全绿 |
| b | `go/db` | `go test ./internal/kafka/... -count=1` | 无新增失败 |
| c | `go/login` | `go vet ./internal/kafka/ && go test ./internal/kafka/ -run TestSendTaskLeaderNotAvailableDoesNotDeadlock -count=1 -v` | 通过；可选红/绿：在临时 worktree 里只回退 `key_ordered_producer.go` 的改动，该用例应以 "deadlocked" 超时失败（不要在当前工作区回退） |
| d | `go/login` | `go test ./internal/kafka/... -count=1` | 无新增失败 |
| e | 仓库根 | `pwsh -File tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1` | 7 例全 PASS |
| f | 仓库根 | `pwsh -File tools/scripts/test_stress_summarize.ps1` | 打印 PASS；DB 段 6 个平均值都出现 |

注意：`go/db`、`go/login` 两个包里同时有别的会话的未提交改动，失败时先看失败点属于哪份改动，**分开汇报**。

### 3.2 proto2mysql（工作目录 `D:\luyuan\proto2mysql`，分支 `feat/conditional-save`）

```sh
go vet ./... && go test -count=1 ./...
cd tools/proto2sql && go test -count=1 ./... && cd ../..
PROTO2MYSQL_INTEGRATION=1 go test -count=1 -run SaveConditional -v .   # MySQL 8.x；起库方式见 docs/testing.md
PROTO2MYSQL_INTEGRATION=1 go test -count=1 -run SaveConditional -v .   # TiDB 8.5（换 testdata/db.json 指向；tidb_retry_limit > 0）
go test -race -count=1 -run SaveConditional .
```

通过标准：全绿；真库用例零 SKIP（`-v` 输出里不得有 `SKIP: TestSaveConditional`）；既有用例无新增失败。结果写回 proto2mysql 的 `docs/release-notes-v0.3.0.md`「验证」一节。

通过之后：合入 proto2mysql main → **问负责人**是否打 `v0.3.0` → 打了之后主仓库各 go.mod 的 `github.com/luyuancpp/proto2mysql v0.2.0` 与对应 `replace` 行一起升级（go/db、go/data_service、go/friend、go/guild、go/schemamigrate、go/trade 都有 replace，按需只升 go/db 也可以，但 replace 要逐字一致，见 go/guild/go.mod:133 的注释）。

---

## 4. 剩余待办（按建议顺序）

> 各项的逐文件改动、不停服上线步骤、回滚方式与复核阻断项都在[附录](godb-pipeline-handoff-20261013-appendix.md) A、B 两节；**复核阻断项必须并入实现**。超过 30 个文件的批次开工前要负责人授权（AGENTS.md §10.2）。

| 顺序 | 项 | 要做什么 | 规模 / 授权 | 依据 |
|---|---|---|---|---|
| 0 | 第 0 批验证 | §3 | — | — |
| 1 | **条件写 go/db 侧详细设计** | 新建 `docs/design/db-write-fencing-mysql.md`：DBTask 加 `save_seq`；两张玩家表加 `fence_owner_epoch` / `fence_save_seq`（字段号顺延帮会二期取号）；C++ `PlayerSaveSeqComp` + `NextSaveSeq(last, nowMs)=max(last+1, nowMs<<10)` 发号；`handleDBWriteOp` 接 ctx（`WithContext` + StatementTimeout）并改调 `SaveConditional`；off / dual / sql 三模式与 class-1/2/3 映射（瓶颈文档 §6.6）；先判 epoch 再判 seq（G3）；缓存带版本写；MySQL 读写超时；告警；迁移 Stage 0/1/2；Java 版核实 `WHERE owner_epoch=?` 能否挡住同 epoch 僵尸写。先出设计、交对抗评审，再开工。 | 实现约 20–25 文件，跨 C++/Go/proto/schema，**需授权** | 附录 B「mysql-fencing」+ 复核修正；瓶颈文档 §4.4 G1/G3/G5、§6.4、§6.6 |
| 2 | 测量打点（往返优化的第 0 步） | go/db 加 `db_task_redis_step_seconds{step}`（11 个固定取值）、stage=sql/task_total/queue_wait/kafka_e2e、`db_ordering_guard_total`、`db_subshard_batch_size`、go-redis PoolStats；只加观测不改行为；然后 Codex 按 AGENTS §6.2 跑 R20（基线 R19），加「突发全量存盘」场景。注意 sql.DBStats 已由 G2b 做了，别重复。 | 约 5 文件 | 瓶颈文档 §6.3 第 0 步 |
| 3 | 第 1 批 ①：读写分区对齐 | 以 C++ 实际在用的 CRC32 mod N 为契约（`go/shared/kafkautil/dbtask_partition.go` + golden 表），login 主生产者设 Partitioner，删掉从未生效的一致性哈希环与「不可用分区重映射」（重映射本身破坏同 key 有序）；上线前先用 kcat 抽样核对 C++ 写消息是否全守 CRC32 契约。 | 约 23 文件，**需授权** | 附录 A「partition-hash-unify」 |
| 4 | 第 1 批 ② 批 A 的剩余部分（= L-04 = 全局单点交接的 P0） | 写任务遇 1040/1203/ErrBadConn/连接错误时延后、不耗重试预算，`db_task_deferred_total{reason}`，「重试队列持续增长」「deferred 持续 >0」「死信长度 >0」告警。G2b 明确不做这一项；DBStats 已由 G2b 导出，别重复。 | 约 4–8 文件 | 附录 A「mysql-connection-budget」批 A（未经独立复核）；瓶颈文档 §4.4 G2 |
| 5 | 第 2 批 ③：消费组按 zone 拆 | `db_rpc_consumer_group_z{id}`；交接闸在入组之前；新组播种旧组已提交位点；收尾删除旧组本 topic 位点；merge_zone 只认唯一权威组；逐 zone 金丝雀。前置：Kafka 镜像钉版本。 | 28–33 文件，**需授权** | 附录 A「consumer-group-per-zone」（6 个阻断项） |
| 6 | 第 3 批 ④：重试只交给分区属主 | `kafka:retry:queue:{topic}:p{N}`，`RetryRouting` 开关 shared→owner，p* 搬回工具，`replicas ≤ PartitionCnt` 门禁。前置：placement-reverify-checklist 的双 db 实例项跑通。 | 批 1 27–29 文件，**需授权** | 附录 A「retry-owner-and-capacity」 |
| 7 | 第 4 批 ② 批 B | 各连接池按真实并发上界压缩、部署前预算门禁——**大半已由 G2b 做了**，只补差。 | 视对齐结果 | 附录 A |
| 8 | 第 5 批 ⑤：guild 阶段 0 | 先起服再注册、POD_IP、停机预算、连接池上限、K8s 清单、guild-stress 压测 A–F；不做 actor / write-behind（违反资产 fail-closed）。 | 超 35 文件，拆 4 个 PR，**需授权** | 附录 A「guild-scale」（5 个阻断项） |

### 4.1 零碎小项（可随手做，单独成 PR）

| 项 | 说明 |
|---|---|
| Kafka 镜像钉版本 | 部署里还是 `apache/kafka:latest`；③ 的前置 |
| 回档 Step 2 只统计本 topic | ⓪ 的另一半，`tools/scripts/k8s_zone_rollback.ps1`，见附录 A「consumer-group-per-zone」的 B5 修正 |
| `kafka:expand:status` 死代码 | 生产代码从不写入 expanding，每条写白读一次 Redis；删除需负责人决定（瓶颈文档 §6.2 第 5 行） |
| go/db `Stop()` 先 cancel | 有积压时必等满 10 秒；改成先撤拉取再等 worker；db.yaml 加 minReadySeconds（瓶颈文档 §4.4 G6） |
| `tools/scripts/go_services.ps1:879` | 本机停 db 用 `Stop-Process -Force`，等同崩溃；改成先 Ctrl+C 超时再强杀（G9） |
| 过时文档订正 | `docs/design/ARCH.md`「playerId 一致性哈希到同一分区」、`docs/design/db-task-kafka-partition-contract.md` 第 9 行、go/db 几处注释与事实不符（PROGRESS.md 2026-10-12 条目缺口 2）；`go/db/internal/logic/pkg/proto_sql/store_registry.go:294`、`go/db/internal/migrate/plan.go:177` 关于 Save 是 REPLACE/ODKU 的说法过时（实际是 UPDATE→INSERT→1062 重试→FOR UPDATE 归类） |
| docs 索引自检 | `python tools/scripts/gen_docs_index.py --check`（本机无 Python，交 Codex） |
| Java PARITY 登记 | Java 仓库 `D:\luyuan\xuanming-server-mmo\PARITY.md`：子 shard、login 自锁、位点重置、①③④ 登记「不适用（Java 没有 Kafka 存盘链）」；②⑤ 待做；条件写待核实 |

---

## 5. 需要负责人拍板

1. proto2mysql 验证通过后是否打 `v0.3.0`。
2. 条件写 go/db 侧：是否开工出详细设计（§4 第 1 项）。
3. 第 1 ~ 5 批各自的开工授权（超 30 文件）。
4. ② 与 G2b 的分工：剩余部分由谁做。
5. `kafka:expand:status` 死代码是否删除。
6. 往返优化的立项判据里「排空 SLO」的数值（现在没有）。
7. 换代 topic（④B2）建议不做；真要做须先落条件写并修订 TiDB 决策 D5。
8. 本会话主仓库改动何时提交（只提交 §2 表中的文件）。

---

## 6. 关键事实速查

| 事实 | 值 / 位置 |
|---|---|
| 排序锁 | TTL 2 分钟、30 秒续租、续租失败一次即放弃（`key_ordered_consumer.go` `orderingLockTTL` / `orderingLockRenewInterval`） |
| 写库调用 | `handleDBWriteOp` 忽略 ctx，`store.Save(msg)`；先 SQL → 落点复核 → 写缓存，之后才核锁（`runDBOp` / `handleTask`） |
| 停机 | `Stop()` 第一行 `c.cancel()`；排空上限 10 秒 |
| 分区与并行 | `go/db/etc/db.yaml` PartitionCnt=10、SubShardCount=8，每 zone 80 路；K8s 所有 zone 共用 GroupID `db_rpc_consumer_group`（`tools/scripts/k8s_deploy.ps1` ConfigMap 模板） |
| Kafka 客户端 | go/db 用 sarama v1.43.1（eager 再平衡）；login 用 sarama v1.46.2；data_service / guild 用 segmentio/kafka-go v0.4.47；C++ 用 librdkafka 默认 consistent_random（CRC32） |
| 死信 | `kafka:dead:queue:{topic}`，生产代码没有消费者，只有 merge_zone 预检读 |
| 写路径往返 | Kafka 来源且 owner_epoch>0 的写：main-redis 9 次 + placement-redis 2 次；逐条见附录 C |
| proto2mysql Save | UPDATE by PK → 0 行则 INSERT → 1062 则重试 UPDATE → 仍 0 行则 FOR UPDATE 归类；1~4 条语句；无版本条件 |
| proto2mysql 模块 | `github.com/luyuancpp/proto2mysql`，各 go.mod 用 `replace ... => github.com/luyuan-cpp/proto2mysql v0.2.0` |
| 压测基线 | 最新 R19（06-18），之后加的排序锁（08-04）/ epoch（09-18）/ 落点 MGET（09-29）都没压测过 |

未核实项见瓶颈文档 §8。

---

## 7. 协作约束（AGENTS.md 里与本项相关的）

- Claude 不跑任何构建 / 测试 / 压测，给 Codex 可直接执行的命令与通过标准；没有 Codex 结果前不说「编译通过」「测试绿」。
- 主仓库不 `git push` / `git tag`；`git commit` 只在负责人明确要求时做，且只提交自己的文件。proto2mysql 例外：负责人已授权 commit + push，打 tag 仍要问。
- 所有功能两个版本都要做（AGENTS.md §12）：交付说明写明 Java 版状态并在 Java 仓库 `PARITY.md` 登记。
- 改动超过 30 个文件、需要换代 topic、触碰生产前先停下报告。
- `docs/PROGRESS.md` 只追加、不改旧条目；新文档在 `docs/README.md` 登记。
- 压测必须按 AGENTS.md §6（先存 prev-summary、清数据、三个时刻拉 snapshot、只读 `stress_summarize.ps1` 输出）。
