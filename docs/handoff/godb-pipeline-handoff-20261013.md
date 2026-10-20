# 交接：玩家存盘管线（go/db）瓶颈修复与条件写（2026-10-13）

> **交出方**：Claude（会话 2026-10-12 ~ 10-13）。**接手方**：新账号的 Claude。编译 / 测试 / 压测仍由 Codex 执行（AGENTS.md §10.1），新账号自己不跑。
> **状态**：本会话主仓库的改动已随本地提交 `d5cbb8a8e`（作者 luyuan-cpp，2026-10-13 03:25，「Record existing changes… No build or tests were run」，**未 push**）与其它会话的改动一起入库；proto2mysql 的改动在已推送的功能分支上。**全部未编译、未测试**。
> **接手核对（2026-10-13）**：本行以上的「未 push」与 §2.1、§3.1 的提交状态已过时，§3 的命令有订正，见文末 §9。
> **本文是第 2 版**：第 1 版经两路核对（事实 / 接手者视角）后按 40 余条意见重写。推导与证据在 [db-task-pipeline-bottlenecks.md](../design/db-task-pipeline-bottlenecks.md)（下称「瓶颈文档」），逐文件方案与复核意见在[附录](godb-pipeline-handoff-20261013-appendix.md)。

---

## 0. 接手第一步

1. 走 AGENTS.md「会话启动门禁」。`docs/PROGRESS.md` 末尾按顺序读这几条（标题可 grep）：
   - 10-12「补充:映射持久化提案、同玩家存储串行的决策考古…」——缺口 1–4 的原始出处。注意其中「Java 存盘已是 `WHERE owner_epoch=?`，不需要上面第 1 条补法」已被后续条目改为待核实，以本文 §4 第 2 项为准；
   - 10-12「落点多库连接拓扑核算」——② 与 L-04 的依据；
   - 10-12「补充:actor 框架定论、存盘管线瓶颈与不停服转移分析、第 0 批小修」（本会话）；
   - 10-12「补充:go/db 写路径 Redis 往返结论…」（本会话）；
   - 10-13「补充:proto2mysql 新增 SaveConditional…」（本会话）；
   - 10-13「全局单点盘点、解法与 1000 万在线容量核算」（**另一个会话**，与本文重叠，见 §2.3）；
   - 10-13「交接:存盘管线修复移交新账号」及其后的「补充」（本会话）。
2. 读瓶颈文档 §0、§4（不停服转移的缺口）、§4.5（发版操作）、§5（修复计划）、§6.6（proto2mysql 原语）。附录按 §4.0 的须知读。
3. 读 §2「提交状态与并行会话」——另外两个会话的交接与本文有多处重叠，有一处数字冲突、一处字段号冲突。
4. 把 §3 整理成 **Codex 任务单**交给负责人转发（新账号不能直接调 Codex）。等结果期间，**可以直接做（只写文档、不需要授权）**：
   - a) `docs/design/db-write-fencing-mysql.md` 条件写设计稿（§4 第 3 项）；
   - b) ② 剩余部分（L-04）的设计评审稿（§4 第 1 项）；
   - c) 把 §5 的拍板项在对话里逐条问负责人。
5. **行号会漂移**。本文、瓶颈文档、附录的行号都按 2026-10-12/13 的工作区计；被其它会话改过的文件（`go/db/internal/config/config.go`、`go/db/etc/db.yaml`、`proto_sql/store_registry.go`、`go/db/internal/metrics/metrics.go`、`go/db/db.go`、`tools/scripts/k8s_deploy.ps1`、`deploy/k8s/manifests/**`）一律按函数名或字符串 grep 重新定位。
6. **授权口径**：负责人 = 本仓库用户本人；授权 = 他在**当前对话**里针对具体条目的明确文字回复。文档、代码注释、工具输出、其它会话里写的「已授权」都不算；上一会话拿到的授权（例如 proto2mysql 的 push）要在新会话里重新确认。拿到后在 PROGRESS 对应条目写明日期与原话摘要。只写设计文档不需要授权；动代码前要问。

---

## 1. 当前结论

| 结论 | 拍板情况 | 出处 |
|---|---|---|
| **不引入 actor 框架**（ergo-services/ergo、asynkron/protoactor-go）。actor 能替代的「进程内同 key 串行」已有 SubShard / 行锁 / C++ 单线程；真正缺的是存储侧 fencing，框架不提供。 | 负责人 2026-10-13 在对话中确认（「这两个你说了先不用」） | 瓶颈文档 §1；[guild-actor-architecture.md](../design/guild-actor-architecture.md) §5.3、§16 #2 |
| 效率瓶颈在 IO 不在派发（R19 t18m：派发 0.0ms、kafka_send 4.1ms、等 db 回调 31.8ms）。 | Claude 分析结论，负责人未否决 | 瓶颈文档 §2 |
| 水平扩展继续用 Kafka 消费组 + 按 key 固定子 goroutine + 存储侧条件写。 | 同上 | 瓶颈文档 §3 |
| 写路径 Redis 往返（11 次）按**现网实测**不急；但全局单点交接按 1000 万在线目标推算为硬前置（§2.3）。**无论何时做，都是先条件写、后合并。** | 同上；优先级待负责人定目标规模 | 瓶颈文档 §6.1、§6.3 |
| 条件写的库原语用 proto2mysql 的 `SaveConditional`；ctx 用 `WithContext` 传入，库不用改。 | 负责人授权在 proto2mysql 落码并推分支 | 瓶颈文档 §6.6 |

没有标「负责人确认」的条目是当前结论，推翻需给出新证据。

---

## 2. 提交状态与并行会话

### 2.1 主仓库

- 本地 `main` 在 `d5cbb8a8e`，`origin/main` 仍在 `73b852f32`（未 push）。本会话的全部改动都在 `d5cbb8a8e` 里，与其它会话的大量改动混在同一次提交中。
- **本会话改动的文件**（用于归属与评审，不再需要「部分提交」）：

  | 文件 | 内容 |
  |---|---|
  | `go/db/internal/kafka/key_ordered_consumer.go` | 只有 `subShardIndex` / `spreadKey` 一处（`routeToSubShard` 附近） |
  | `go/db/internal/kafka/subshard_route_test.go` | 新增，5 个用例 |
  | `go/login/internal/kafka/key_ordered_producer.go` | `markPartitionUnavailableLocked` 及两处调用 |
  | `go/login/internal/kafka/key_ordered_producer_lock_test.go` | 新增 |
  | `tools/scripts/kafka_offset_reset.ps1` | `Assert-KafkaToolOutputClean` 与调用处 |
  | `tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1` | 新增，7 例 |
  | `tools/scripts/stress_summarize.ps1` | DB 段 label 顺序 |
  | `tools/scripts/test_stress_summarize.ps1` | DB 段回归用例 |
  | `docs/design/db-task-pipeline-bottlenecks.md` | 新增 |
  | `docs/design/guild-actor-architecture.md` | §5.3、§15、§16 #2、§17 第 1 条 |
  | `docs/handoff/godb-pipeline-handoff-20261013.md`、`…-appendix.md` | 本文与附录 |
  | `docs/README.md` | 3 行：bottlenecks、本文、附录 |
  | `docs/PROGRESS.md` | §0 列出的「本会话」几条 |

- 本文与瓶颈文档链接了别的会话的交接 / 规格 / 复验清单，它们也都在 `d5cbb8a8e` 里，链接有效。

### 2.2 proto2mysql

- 本机 checkout `D:\luyuan\proto2mysql`；分支 `feat/conditional-save` 已推送（commit `e81a1e7`，[GitHub](https://github.com/luyuan-cpp/proto2mysql/tree/feat/conditional-save)）。main 未动、未打 tag。主仓库所有 go.mod 仍钉 v0.2.0。

### 2.3 与另外两份交接的重叠（排期时合并，不要各做一份）

| 交接 | 重叠条目 | 建议归属 / 注意 |
|---|---|---|
| [handoff-launch-readiness-and-d1-20261013.md](handoff-launch-readiness-and-d1-20261013.md)（规格在 [launch-readiness-20261013/](launch-readiness-20261013/)） | **G2b**（MySQL 连接预算与连接池可观测性）= 本文 ② 的一部分 | **G2b 的权威进度以那份 §4 第 1 项为准**。按文件核对：**已落** `mysql.yaml`（max_connections 500→1800、内存 limit 5Gi、注释允许 `SET PERSIST`）、gateway Hikari、`go/shared/sqlpoolstats` 及其在 go/db 与 data_service 的注册、db manifest 的 9160 端口注解；**未落** `k8s_deploy.ps1` 的总账五个函数与门禁（`Get-MysqlConnectionPlan` / `Get-MysqlConnectionLedger` 只存在于 spec_G2b.md，`db.yaml` / `data-service.yaml` 新注释指向的函数目前不存在）、`mysql-connection-alerts.yaml`、db ConfigMap 的 `MetricsListenAddr`（不补的话 K8s 上 go/db 的指标抓不到，§4 第 4 项的新打点也一样）；friend / trade 没接 sqlpoolstats。G2b **明确不做**「1040 不耗重试预算」（登记为 L-04）。（那份交接把本文第 1 版的错误说法引为「go/db 交接(§7)」，实为本文 §2，本版已订正。） |
| 同上 §4 第 10 项 | 「同玩家串行缺口：MySQL 条件写」= 本文 §4 第 3 项；「Kafka 存盘失败后 MySQL 永久落后（scene 发 DBTask 失败时脏比较快照不作废）」；Lua `tonumber` 比较 | 条件写归本文（库原语已落）；后两项属存盘管线，建议归本文、单独成 PR，请负责人定 |
| 同上止血第 4 件 | db_task topic 显式设 `retention.bytes` 与 PVC | 归那边；本文只提供「回档 / 重放需要的最短缓冲时长」 |
| [handoff-global-singletons-10m-ccu-20261013.md](handoff-global-singletons-10m-ccu-20261013.md) | P0「go/db 连接类错误耗重试预算，进无消费者死信」 | = 本文 ② 剩余（L-04）。**本文认领**，请负责人通知那个会话不要重复做 |
| 同上 | P0「Redis 高可用会丢已确认的存盘：重试 / 死信入队补 WAITAOF」 | 改的是 `key_ordered_consumer.go` 的 `deferFailedTask` / 重试入队，与本文 ②A、④ 同一段代码；建议归本文，排在 ④ 之前或同批，请负责人定 |
| 同上 | P1「db 消费组全 zone 共用」 | = 本文 ③。**不能按那份 §3 的写法直接改组名**：`key_ordered_consumer.go` 里 `Offsets.Initial = sarama.OffsetOldest`，新组没有已提交位点会从最早保留位点全量重放，游标缺键时还会放行旧写。必须走附录 A「consumer-group-per-zone」的交接闸 / 位点播种 / 收尾流程。另外 `k8s_deploy.ps1` 的 login ConfigMap 也有一个同名 `GroupID` 字段（login 代码只声明未使用），改名时单独说明 |
| 同上 | P1「每次存盘 5 条 SQL，4 条无效（未开 clientFoundRows）」 | 推断（待真库核实）：条件写落地后玩家表每次写都改 fence 列，UPDATE 影响 1 行，无效 SQL 在玩家表上自然消失；`clientFoundRows` 只对条件写之前和非玩家表有收益。开了之后 `SaveConditional` 的「条件成立但整行没变」报 `SaveUpdated` 而非 `SaveUnchanged`，go/db 的分类不要依赖 `SaveUnchanged` |
| 同上 | 「go/db 每条写任务约 11.5 条控制面命令合并成 1~2 次 EVAL」列为目标规模硬前置（共享 Redis 约 60% 命令来自它） | 与本文 §1 第 4 行的关系见那一行 |
| 同上 §4.5 | DBTask 加 `string reply_instance = 9;` | **与本文 `save_seq = 9` 撞号**（`proto/db/db_task.proto` 当前最后一个字段是 `owner_epoch = 8`）。见 §5 第 13 项 |
| 两份都有 | Redis 键改 `{player_id}` hash tag 并分片 | 与瓶颈文档 §6.5「不要为 Cluster 改 hash tag」冲突，且影响条件写的围栏缓存键命名（§4 第 3 项）。见 §5 第 16 项 |

### 2.4 部署先后的硬约束

- **G2b 的 `mysql.yaml` 改了 Deployment 模板，apply 会触发 Recreate，唯一的 MySQL 要停一次。** 在 ② 剩余部分（L-04：写任务遇连接不可用时延后、不耗重试预算）合入并部署之前，停机期间的写任务会耗尽 3 次重试、进入没有消费者的死信，存盘静默丢失。所以：L-04 部署前，任何有真实存档的环境都不要 apply 那份 mysql.yaml；确需先 apply，就在维护窗口内先停写、确认 db_task lag、重试队列、死信都为 0，再滚动 MySQL。
- 新账号只负责写规程、给 Codex 命令，**不碰任何集群**。

---

## 3. 交 Codex 验证

### 3.0 怎么交、结果写哪

- 把 §3.1、§3.2 整理成一份任务单：工作目录 / 命令 / 前置 / 通过标准 / 失败时保留 `go test -v` 失败用例全文与编译错误前 20 行；在对话里交给负责人转给 Codex，结果由负责人贴回。
- 写回：① PROGRESS 追加「补充:存盘管线第 0 批与 proto2mysql 的 Codex 复验结果」；② 瓶颈文档 §7 表加「验证」列，填通过 / 失败与日期；③ proto2mysql 的结果写进它的 `docs/release-notes-v0.3.0.md`「验证」节（提交与否见 §3.2）。
- 红了就停下归因，不连续重试（AGENTS.md §10.2）。

### 3.1 主仓库第 0 批（`D:\luyuan\xuanming-server-mmo-cpp`，HEAD `d5cbb8a8e`）

前置：Go ≥ 1.26.5（go/db 的 go.mod）、PowerShell 7（两个脚本测试都 `#requires -Version 7`）。

```sh
# a. go/db 新用例（5 个全绿）
cd go/db && go test ./internal/kafka/ -run 'TestSpreadKey|TestSubShardIndex|TestRouteToSubShard' -count=1 -v
# b. go/db 整包（无新增失败）
cd go/db && go test ./internal/kafka/... -count=1
# c. go/login 回归用例（通过）
cd go/login && go vet ./internal/kafka/ && go test ./internal/kafka/ -run TestSendTaskLeaderNotAvailableDoesNotDeadlock -count=1 -v
# d. go/login 整包（无新增失败）
cd go/login && go test ./internal/kafka/... -count=1
# e. 位点重置判定（7 例 PASS）
pwsh -File tools/scripts/tests/kafka_offset_reset_output_guard.tests.ps1
# f. 汇总脚本（打印 PASS，DB 段 6 个平均值都出现）
pwsh -File tools/scripts/test_stress_summarize.ps1
```

- `d5cbb8a8e` 同时带进了其它会话的大量改动（go/db 的 config / metrics / db.go / store_registry、go/login 多处、go/shared 的 sqlpoolstats / kafkautil / placement 等），它们同样未编译。a–d 失败时先看失败点在哪份改动里，**分开汇报**；需要基线时在 `73b852f32` 的临时 worktree 里跑同一命令对比。
- c 的可选红 / 绿：`git worktree add <临时目录> 73b852f32`，只把 `go/login/internal/kafka/key_ordered_producer_lock_test.go` 拷进去（不拷修复），在 `<临时目录>/go/login` 跑同一条命令，应以 "deadlocked" 超时失败；跑完删除 worktree，不动当前工作区。

### 3.2 proto2mysql（`D:\luyuan\proto2mysql`，分支 `feat/conditional-save`）

```sh
go vet ./... && go test -count=1 ./...
cd tools/proto2sql && go test -count=1 ./... && cd ../..
# 真库：按 docs/testing.md 起库并 CREATE DATABASE proto2mysql_test；用环境变量传 DSN，不要改已入库的 testdata/db.json
PROTO2MYSQL_INTEGRATION=1 PROTO2MYSQL_TEST_DSN='root:<pwd>@tcp(127.0.0.1:13307)/proto2mysql_test' go test -count=1 -run SaveConditional -v .   # MySQL 8.4
PROTO2MYSQL_INTEGRATION=1 PROTO2MYSQL_TEST_DSN='root:@tcp(127.0.0.1:14000)/proto2mysql_test' go test -count=1 -run SaveConditional -v .          # TiDB 8.5（tidb_retry_limit > 0）
# race：需 CGO_ENABLED=1 与 gcc；Windows 上没有就在 Linux / WSL 跑，并写明平台
go test -race -count=1 -run SaveConditional .
```

- 拉 `mysql:8.4`、`pingcap/tidb:v8.5.1` 镜像属于下载，**先征得负责人同意**。
- 通过标准：全绿；真库用例零 SKIP（`-v` 输出里不得有 `SKIP: TestSaveConditional`）；既有用例无新增失败。
- 通过之后**分别**在对话里问负责人：① 是否合入 proto2mysql main；② 是否打 `v0.3.0`；③ 上一会话的 commit / push 授权是否沿用。没有明确答复前，只把结果写回 release-notes，不提交、不推送。

### 3.3 打了 v0.3.0 之后升级主仓库

- 只升 go/db：`go/db/go.mod` 的 require 改为 v0.3.0，**replace 两侧都改**为 `replace github.com/luyuancpp/proto2mysql v0.3.0 => github.com/luyuan-cpp/proto2mysql v0.3.0`（模块路径是 luyuancpp，GitHub 组织是 luyuan-cpp；只改 require 会让 replace 失效）。由 Codex 在 go/db 跑 `go mod tidy` 并提交 go.sum（联网下载，先征得同意）。
- 其它带 replace 的模块（go/data_service、go/friend、go/guild、go/schemamigrate、go/trade、tools/proto_generator/protogen）保持 v0.2.0；friend / guild / trade 的 replace 必须与 schemamigrate 逐字一致（见 go/guild/go.mod 里那段注释）。
- launch-readiness 交接写的是「全程不改 go.mod / go.sum」，升级前和那边对齐，避免同一轮 Codex 验证里混改。

---

## 3.5 发版红线（条件写落地之前）

- 任何 go/db 发版都按瓶颈文档 §4.5 执行：低峰；多个 zone 依次滚、一次一个 Pod；同窗口不重启 Redis / MySQL、不跑阻塞 DML 的 DDL；每换一个 Pod 核对消费组 lag、重试队列、各 processing 列表、`kafka:dead:queue:*` 归零，日志无 `claim drain timed out` / `DATA-LOSS PREVENTED` / `retry reschedule failed` / `Release lock failed: lock expired`；某 Pod 被强杀时至少等 2 分 40 秒再换下一个。
- §4.5 给的版本下限 `be6b4ab1d` 本身从未编译运行；[placement-reverify-checklist-20261012.md](placement-reverify-checklist-20261012.md) 的双 db 实例项跑通之前不要把它当线上基线。线上实际版本与副本数未知，发版前先查清。
- HEAD 的 `deploy/k8s/manifests/go-svc/db.yaml` 已是 2 副本 + maxSurge 1：每次滚动都有两个 go/db 进程重叠，**G1 回档缝每次都暴露**（瓶颈文档 §4.3 场景 3）。所以 §4 第 2 项（G1 治标）值得先做。
- 再加 §2.4 的 MySQL Recreate 约束。

---

## 4. 剩余待办

### 4.0 附录阅读须知

- 附录约 43 万字节，超过单次读取上限，用 offset / limit 按节读。章节起始行（先 `grep -n '^###' 附录` 确认）：A 节 L8；④ retry-owner L12；③ consumer-group L116；① partition-hash L218；⑤ guild L312；② mysql-connection-budget L440；B 节 L534（redis-merge L538、batch-amortize L623、**mysql-fencing L719**）；C 节往返盘点 L808。
- 每节真正要并入实现的是「复核阻断项」与「复核要求的修正」两块；「改动」块的行号要重新 grep。
- **附录 B「mysql-fencing」里已作废的部分**：要求 proto2mysql 新增 `UpdateByPrimaryKeyContext` / `InsertContext` 的条目、新建 `fenced_save.go` 手写 UPDATE / SELECT / INSERT 的条目、off 模式「用 UpdateByPrimaryKeyContext 加 Insert」的写法——一律改为 `SaveConditional(msg, SaveOptions{FenceFields, AllowEqual, PreserveFields})`，映射以瓶颈文档 §6.6 为准（off 只带 `PreserveFields`）。「Save 到底是什么语句」的疑问已结案（UPDATE → INSERT → 1062 重试 → FOR UPDATE 归类）。M0（汇总脚本 DB 段）已落码。其余条目（proto 字段、C++ 发号、缓存、配置、指标、merge_zone 键、告警、迁移 Stage）仍有效。
- **附录 A「mysql-connection-budget」**没有经过独立复核；其批 B（压缩各连接池到 40 / 4/1 / 8/2、禁止 SET PERSIST、不改 mysql.yaml）与 G2b 已落的「抬上限 1800 + 5Gi」路线**互相冲突**，作废待定（§5 第 12 项），在那之前不要按它改任何数字。

### 4.1 顺序

> 超过 30 个文件的批次开工前要授权（AGENTS.md §10.2）。低于 30 文件但跨语言 / 动 proto / 要 DDL / 改变玩家可见行为的，也要授权（理由写在各行）。

| 顺序 | 项 | 要做什么 | 规模 / 授权 | 依据 |
|---|---|---|---|---|
| 0 | 第 0 批验证 | §3 | — | — |
| 1 | **② 剩余（L-04，= 全局单点交接 P0）** | 写任务遇 1040 / 1203 / ErrBadConn / 连接错误时延后、不耗重试预算；`db_task_deferred_total{reason}`；「重试队列持续增长」「deferred 持续 >0」「死信长度 >0」告警。先出设计评审稿（附录 ② 未经独立复核）。`dbOpHandler` 先按这一项改成返回 `error`，条件写再在其上扩展为 `(fenceOutcome, error)`，避免互相返工。 | 约 4–8 文件；动代码前问 | 附录 A ② 批 A；瓶颈文档 §4.4 G2；tidb-phase1 L-04 |
| 2 | **G1 治标小 PR** | ① `proto_sql/db.go` 的 `newMysqlConfig` 补 ReadTimeout / WriteTimeout；② `handleDBWriteOp` 接 ctx（`store.WithContext(ctx)`），ctx 随排序锁失租取消；③ 缓存回写挪到 verify 之后；④ 续租失败改为有上限的退避重试；⑤ 对 `Release lock failed: lock expired` 加计数与告警。**撤不回已进入 MySQL 的语句，不是栅栏**，只缩小窗口（瓶颈文档 §4.4 末段）。 | 约 3–5 文件；不动 proto / DDL，可与第 3 项设计评审并行；动代码前问 | 瓶颈文档 §4.4 G1「治标」 |
| 3 | **条件写 go/db 侧详细设计**（只写 `docs/design/db-write-fencing-mysql.md`，不需授权） | 见下方「条件写设计必须覆盖」。写完交对抗评审，再请负责人授权实现。 | 实现约 20–25 文件，跨 C++ / Go / proto / schema；要授权（字段号上线不可回收、每库 DDL、跨 C++） | 附录 B mysql-fencing（按 §4.0 须知读）；瓶颈文档 §4.4、§6.4、§6.6 |
| 4 | 测量打点 + R20 基线 | go/db 加 `db_task_redis_step_seconds{step}`（11 个固定取值）、stage=sql / task_total / queue_wait / kafka_e2e、`db_ordering_guard_total`、`db_subshard_batch_size`、go-redis PoolStats（sql.DBStats 已由 G2b 做）。**R19 没有 DB 段基线**（prev-summary 的 DB 段全是 "-"，原始快照目录已不在），所以 R20 = 现行代码 + 打点，作为 DB 段的新基线；robot / login 段仍与 R19 对比。「突发全量存盘」场景的触发方式（GM 命令或机器人全员同时下线等）先写进 `stress_round20.ps1` 的设计，交 Codex 前请负责人确认。压测按 AGENTS.md §6 全流程。 | 约 5 文件；依赖 G2b 补上 db ConfigMap 的 `MetricsListenAddr` | 瓶颈文档 §6.3 第 0 步 |
| 5 | ① 读写分区对齐 | 以 C++ 实际在用的 CRC32 mod N 为契约（`go/shared/kafkautil/dbtask_partition.go` + golden 表），login 主生产者设 Partitioner，删掉从未生效的一致性哈希环与「不可用分区重映射」。**上线前先用 kcat 抽样**核对 C++ 写消息是否全守契约（C++ 是否所有调用方都不传 partition 未逐一核对）。 | 约 23 文件；要授权（改变 login 进场行为：db 积压超过 8 秒时从「读到旧档」变成「进场失败」） | 附录 A ①（sound-with-fixes） |
| 6 | ③ 消费组按 zone 拆 | `db_rpc_consumer_group_z{id}`；交接闸在入组之前；新组播种旧组已提交位点；收尾删除旧组本 topic 位点；merge_zone 只认唯一权威组；逐 zone 金丝雀。前置：Kafka 镜像钉版本（§4.2）。**不能直接改组名**（§2.3）。 | 28–33 文件，要授权 | 附录 A ③（6 个阻断项） |
| 7 | ④ 重试只交给分区属主 | `kafka:retry:queue:{topic}:p{N}`，`RetryRouting` 开关 shared → owner，p* 搬回工具，`replicas ≤ PartitionCnt` 门禁。前置：复验清单的双 db 实例项跑通。与 WAITAOF（§2.3）同一段代码，合并排期。 | 批 1 27–29 文件，要授权 | 附录 A ④ |
| 8 | ② 批 B | **作废待定**（§4.0、§5 第 12 项）。 | — | — |
| 9 | ⑤ guild 阶段 0 | 先起服再注册、POD_IP、停机预算、连接池上限、K8s 清单、guild-stress 压测 A–F；不做 actor / write-behind（违反资产 fail-closed）。 | 超 35 文件，拆 4 个 PR，要授权 | 附录 A ⑤（5 个阻断项） |

**条件写设计必须覆盖**（第 3 项）：

- 字段：`proto/db/db_task.proto` 的 DBTask 加 `uint64 save_seq`（号码见 §5 第 13 项，与 reply_instance 撞 9）；`proto/common/database/mysql_database_table.proto` 的 player_database 加 `fence_owner_epoch = 18`、`fence_save_seq = 19`（当前最后是 `settlement_ledger = 17`），player_database_1 加 `= 3`、`= 4`（当前最后是 `stress_test_probe = 2`），都声明在 message 末尾。落码前 grep 确认没被帮会剩余批次先占；帮会二期的加列纪律要求新增字段在同一次改动里追加进各处放行清单，清单位置以 `docs/design/guild-phase2/90-consistency.md` 的「G-03 `player_database` 加列与迁移」一节原文为准（含 `03-names.md` §3.15a 等）。
- 复核阻断项（附录 B mysql-fencing「复核阻断项 / 修正」）：
  - **B1**：stale_owner 进死信并告警，不能直接 ack。所有**只清 Redis** 的流程（AGENTS.md §6.2 第 2 步的 FLUSHALL、dev_tools、k8s_zone_rollback、Redis HA 故障 runbook）要同时把玩家表围栏归零（`UPDATE … SET fence_owner_epoch=0, fence_save_seq=0`），否则条件写上线后全部存盘会被判为 stale_owner。改 AGENTS.md 需负责人同意（§5 第 14 项）。长期把 epoch 下限并入 D1 映射落库（launch-readiness 那边）。
  - **B2**：`save_seq` 初值不能只靠墙钟。C++ 新实体的 lastSaveSeq 取 `max(PlayerAllData.last_save_seq, 同 epoch 时的 fence_save_seq)`，再与 `nowMs<<10` 取 max；`NextSaveSeq(last, nowMs) = max(last+1, nowMs<<10)` 在实体重建后 last=0 只剩墙钟。
  - **B3**：class-3 只在 fence=(0,0) 时写，否则进死信。
  - 判 older 之后先 `route.confirm()` 再 ack；off 模式不能调 Save（会把围栏清零），改用 `PreserveFields`；Stage 2（删 Redis 守卫）要有机器门禁；PITR 回档后要先清积压。
- go/db 映射与 `SaveConditional` 语义：瓶颈文档 §6.6；先判 epoch 再判 seq（G3）。
- 围栏缓存：不要另开独立键 `{MsgType}:{Key}:fence`；优先放进数据键的值封装里一次写入；必须分键时键名预留 `{player_id}` hash tag，并写明与 D12 / CZ-2 改写方案的关系（§5 第 16 项）。
- MySQL 读写超时（第 2 项已做则复用）。
- 构建：proto 改动后的重生与编译交 Codex，命令以 AGENTS.md §4 为准（`cd go && build.bat`；附录里的 proto-gen-run 写法先核实是否还存在）；C++ 受影响工程按依赖顺序 MSBuild `/m:1` 串行；新增 `save_seq.h` 等要同步 `CMakeLists.txt` 与 `.vcxproj`；不手改 `generated/`。
- Java（AGENTS.md §12）：附录 B 已核对 `game-player-store/.../PlayerMapper.java`：Java 由持有 epoch 的 scene 同步写库（`WHERE player_id=? AND owner_epoch=? [AND owner_released=0]`），中间没有 go/db 这样的执行实例，「同 epoch 僵尸写」场景不成立，**初判不适用**。新账号再确认 Java 没有异步 / 批量落库路径后，在 Java 仓库 `PARITY.md` 登记「不适用（Java 内部）」，备注：C++/Go 是单调高水位，Java 是等值比较；新 proto 字段经 ContractSync 进 game-proto，加字段兼容，不改 Java 代码。

### 4.2 零碎小项（各自单独成 PR）

| 项 | 说明 |
|---|---|
| Kafka 镜像钉版本 | `apache/kafka:latest` 在 `deploy/docker-compose.yml`（两处）、`deploy/k8s/manifests/infra/kafka.yaml`、`kafka-topic-init.yaml`，四处钉到同一具体版本；`deploy/k8s/README.md` 已登记这是浮动 tag。③ 的前置 |
| 回档 Step 2 | `tools/scripts/k8s_zone_rollback.ps1` 的 Step 2 在 zone-down 之后，本 zone 的 db 已停，LAG 必然排不空，现在只是等满超时再 WARN。按附录 A ③ 的 B5 / F6 修正：删除该步（Step 5 会把位点拨回 TargetTime），或挪到 zone-down 之前、按本 topic 过滤、只作提示；**不要改成 throw**。等 D1 第 0 批对该文件的改动稳定后再动 |
| `kafka:expand:status` | 生产代码从不写入 expanding，`processDBTask` 对每个任务（读写都算）白读一次 Redis，读失败时还会改走 `tryLockAndProcess`。删除需负责人决定（§5 第 5 项） |
| go/db 停机顺序 | `Stop()` 第一行 `cancel()`，有积压时必等满 10 秒；改为先撤拉取再等 worker；`deploy/k8s/manifests/go-svc/db.yaml` 加 `minReadySeconds`（G6） |
| `tools/scripts/go_services.ps1` | 本机停 db 用 `Stop-Process -Force`，等同崩溃；改为先 Ctrl+C、超时再强杀（G9） |
| 过时说法订正 | `docs/design/ARCH.md`「playerId 一致性哈希到同一分区」、`docs/design/db-task-kafka-partition-contract.md` 第 9 行、go/db 几处注释（出处：PROGRESS 10-12「补充:映射持久化提案、同玩家存储串行的决策考古」条目「缺口按严重度」第 2 条）；`proto_sql/store_registry.go` 里 grep `REPLACE INTO` 的那句注释、`go/db/internal/migrate/plan.go` 里 grep `ON DUPLICATE KEY` 的三处（Save 实际是 UPDATE → INSERT → 1062 重试 → FOR UPDATE 归类；「无主键时会追加新行」的结论不变，改文案时保留） |
| docs 索引自检 | `python tools/scripts/gen_docs_index.py --check`（本机无 Python，交 Codex） |
| Java PARITY 登记 | Java 仓库 `D:\luyuan\xuanming-server-mmo\PARITY.md`：子 shard、login 自锁、位点重置、①③④ 登记「不适用（Java 没有 Kafka 存盘链）」；② 的 Druid 连接总账、⑤ guild 指标与 Dubbo 注册顺序「待做」；条件写见 §4.1 |

---

## 5. 需要负责人拍板

1. proto2mysql 验证通过后：是否合入 main、是否打 `v0.3.0`、上一会话的 push 授权是否沿用（三件分别问）。
2. 条件写实现的开工授权（设计稿写完、评审过之后）。
3. 第 1、2、4–9 项各自的开工授权。
4. ② 剩余（L-04）归本文还是全局单点会话（本文建议归本文）。
5. `kafka:expand:status` 死代码是否删除。
6. 往返优化立项判据里「排空 SLO」的数值，以及是否以 1000 万在线为目标规模（决定往返合并是否提前）。
7. 换代 topic（④B2）建议不做；真要做须先落条件写并修订 TiDB 决策 D5。
8. `d5cbb8a8e` 何时 push（主仓库 push 按 AGENTS.md §9 由负责人自己做）。
9. ① 的行为变化（db 积压超过 8 秒时进场失败）是否接受。
10. ④ 与 TiDB 决策 D5「重试队列不动」的排期冲突。
11. G2b 的 mysql.yaml（触发 Recreate）何时、按什么窗口 apply，是否等 L-04 先落地（§2.4）。
12. ② 批 B 路线：池压缩（附录 A）还是抬上限（G2b 已落）；`SET PERSIST` 是否允许。
13. DBTask 9 号给 `save_seq` 还是 `reply_instance`（另一个顺延到 10）；定下后两份交接都改，先落码的一方在 proto 注释里写明另一号已预留。
    - **2026-10-18 更正**：已定 9 给 `save_seq`、10 给 `reply_instance`；`string reply_instance = 10;` 已写进 `proto/db/db_task.proto`（task:result 读回执通知分频道第 1 批，未编译、待 Codex 验证），10 号不再是「预留」。§2.3 表中「与本文 `save_seq = 9` 撞号」一行同此。
14. 条件写上线后「只清 Redis 必须同时归零围栏」是否写进 AGENTS.md §6.2。
15. ③ 是否是开服门槛（开服是否支持单 zone 回档或合服）。
16. Redis 分片的 hash tag 方案与瓶颈文档 §6.5 的统一裁决（影响围栏缓存键）。
17. 重叠项归属：WAITAOF、db_task retention、scene 发 DBTask 失败的脏快照、Lua `tonumber`（§2.3）。

---

## 6. 关键事实速查

| 事实 | 值 / 位置 |
|---|---|
| 排序锁 | TTL 2 分钟、30 秒续租、续租失败一次即放弃（`key_ordered_consumer.go` 的 `orderingLockTTL` / `orderingLockRenewInterval`） |
| 写库调用 | `handleDBWriteOp` 忽略 ctx，`store.Save(msg)`；先 SQL → 落点复核 → 写缓存，之后才核锁（`runDBOp` / `handleTask`） |
| 停机 | `Stop()` 第一行 `c.cancel()`；排空上限 10 秒 |
| 新消费组起点 | `Offsets.Initial = sarama.OffsetOldest`（新组无位点时全量重放） |
| 分区与并行 | `go/db/etc/db.yaml` PartitionCnt=10、SubShardCount=8，每 zone 80 路；K8s 上所有 zone 共用 `db_rpc_consumer_group`（`k8s_deploy.ps1` 里 grep `GroupID: "db_rpc_consumer_group"`，db 与 login 两处） |
| Kafka 客户端 | go/db sarama v1.43.1（eager 再平衡）；login sarama v1.46.2；data_service / guild segmentio/kafka-go v0.4.47；C++ librdkafka 未配 partitioner，默认 consistent_random（keyed 消息 CRC32），所有写入方是否都守这条**未核实** |
| 死信 | `kafka:dead:queue:{topic}`，生产代码没有消费者，只有 merge_zone 预检读 |
| 写路径往返 | Kafka 来源且 owner_epoch>0 的写：main-redis 9 次 + placement-redis 2 次；逐条见附录 C |
| proto2mysql Save | UPDATE by PK → 0 行则 INSERT → 1062 则重试 UPDATE → 仍 0 行则 FOR UPDATE 归类；1~4 条语句；无版本条件 |
| proto2mysql 模块 | 模块路径 `github.com/luyuancpp/proto2mysql`，各 go.mod 用 `replace … => github.com/luyuan-cpp/proto2mysql v0.2.0`；含 replace 的有 go/db、go/data_service、go/friend、go/guild、go/schemamigrate、go/trade、tools/proto_generator/protogen |
| MySQL | `deploy/k8s/manifests/infra/mysql.yaml`：max_connections 1800、内存 limit 5Gi（G2b，待压测确认）、`strategy: Recreate` |
| 压测基线 | 最新 R19（06-18），DB 段无数据；之后加的排序锁（08-04）/ epoch（09-18）/ 落点 MGET（09-29）都没压测过 |

未核实项见瓶颈文档 §8。

---

## 7. 协作约束（AGENTS.md 里与本项相关的）

- Claude 不跑任何构建 / 测试 / 压测，给 Codex 可直接执行的命令与通过标准；没有 Codex 结果前不说「编译通过」「测试绿」「已验证」。
- 主仓库不 `git push` / `git tag`；`git commit` 只在负责人明确要求时做。proto2mysql 的 commit / push 需在新会话里重新确认授权，打 tag 必须问。
- 所有功能两个版本都要做（AGENTS.md §12）：交付说明写明 Java 版状态并在 Java 仓库 `PARITY.md` 登记。
- 改动超过 30 个文件、需要换代 topic、触碰生产前先停下报告；不碰任何集群。
- 不回退、覆盖或顺手整理其它会话的改动；改共享文件前先 `git log` / `git status` 看有没有别的会话在动。
- `docs/PROGRESS.md` 只追加、不改旧条目；新文档在 `docs/README.md` 登记。
- 压测按 AGENTS.md §6 全流程（先存 prev-summary、清数据、三个时刻拉 snapshot、只读 `stress_summarize.ps1` 输出）。
- 下载（镜像、Go 模块）先征得负责人同意。

---

## 8. 给新账号的开场提示词（可直接复制）

```text
你接手 go/db 玩家存盘管线的工作。先走 AGENTS.md 会话启动门禁，然后读 docs/handoff/godb-pipeline-handoff-20261013.md（第 2 版），按它 §0 的顺序读材料。

本轮只做这四件事：
1. 核对交接 §2 的提交状态与归属（G2b 的进度以 docs/handoff/handoff-launch-readiness-and-d1-20261013.md §4 第 1 项为准）；
2. 把交接 §3 整理成一份 Codex 任务单交给我（我转给 Codex），结果回来后按 §3.0 写回；
3. 在等结果期间写 docs/design/db-write-fencing-mysql.md 设计稿（§4.1 第 3 项，覆盖「条件写设计必须覆盖」全部条目），以及 ② 剩余部分（L-04）的设计稿；
4. 把交接 §5 的拍板项逐条问我。

约束：你不编译、不测试、不压测；不动其它会话的改动；不 commit / push / tag，除非我在对话里明确说；授权只认我在当前对话里的答复。全部用中文。
```

---

## 9. 接手核对订正（2026-10-13，接手会话）

接手后对 §2、§3、§6 做了一轮只读核对（没有 fetch、没有编译、没有运行）。上文保持原样，与事实不符的地方以本节为准。

### 9.1 提交状态（§2.1）

| 上文说法 | 核对结果 |
|---|---|
| 本地 `main` 在 `d5cbb8a8e` | 在 `0aa1027bc`。它只改了 4 个文档（本文第 2 版、附录的阅读须知、瓶颈文档两处订正、PROGRESS 一条），代码与 `d5cbb8a8e` 逐字节相同 |
| `origin/main` 仍在 `73b852f32`，未 push | 按本地跟踪引用，两个提交都已推送：`d5cbb8a8e` 于 03:30:54，`0aa1027bc` 于 03:34:20。文首状态行、§2.1 第一条、§3.1 标题、§5 第 8 项都已过时 |
| 本会话的全部改动都在 `d5cbb8a8e` 里 | 8 个代码与脚本文件、文档初版在 `d5cbb8a8e`；本文第 2 版等文档订正在 `0aa1027bc` |

- §2.1 表格的 14 个文件都在 `d5cbb8a8e` 的改动里，描述的符号都在；`key_ordered_consumer.go`、`key_ordered_producer.go`、两个脚本、`guild-actor-architecture.md` 没有表格之外的改动。
- go/db、go/login 的 `internal/kafka` 包里没有其它会话的文件；全仓 `go.mod` / `go.sum` 没变。但这两个包的编译闭包会带上其它会话改过的包（`db/internal/config`、`db/internal/metrics`、`proto_sql`、`shared/sqlpoolstats`、`shared/kafkautil`、`shared/placement`、`login/internal/config`）。
- **主工作区从 04:00 起有另一个会话在写未提交的改动**（G2b 的后半：`k8s_deploy.ps1` 与契约测试、`mysql.yaml`、`mysql-connection-alerts.yaml`、`sqlpoolstats`、几份设计文档等）。§2.3 第一行的「已落 / 未落」清单会随之变化，用之前重新 `git status`。
- 本机另有一个 Codex 的 worktree 停在 `73b852f32`，不含待验证的代码。

### 9.2 G2b 与重叠项（§2.3、§2.4）

- G2b 在 HEAD 上的状态与开服交接 §4 第 1 项的 40 项判定全部一致。措辞上要留意：`Assert-MySqlConnectionBudget` 这个门禁名出自本文附录，G2b 的规格只打 WARN；friend / trade 不接连接池指标是 G2b 的范围外事项，不是「未落」。
- 「重试 / 死信入队补 WAITAOF」那一项有三个写入点，只有重试与死信入队在 go/db；owner_epoch 铸造在 scene_manager，账号目录在 login。
- 「db_task topic 的 retention.bytes 与 PVC」按开服交接原文归全局单点交接，不是开服交接。
- 开服交接说的「Lua `tonumber`」是四个服务里 double 精度的隐患，与附录处理的 nil 判断不是同一处；四处里只有 `luaMarkAppliedEpoch` 在 go/db。
- §2.4「停机期间耗尽 3 次重试」只对连接池已打开的在跑实例成立；重试循环同一拍内可以反复认领，耗尽可以远快于 3 秒。
- 另两份交接仍按本文第 1 版的编号引用本文，也仍写着「工作区未提交」。L-04 的归属三份说法不一（§5 第 4 项）。

### 9.3 Codex 验证（§3）

§3.1 的六条命令不能照抄顺序执行（每行都以相对路径 `cd` 开头）；`test_stress_summarize.ps1` 没有 `#requires -Version 7`；两个 Go 包此前没有任何运行记录，基线对比要列为必做。订正后的版本是 [godb-pipeline-codex-tasksheet-20261013.md](godb-pipeline-codex-tasksheet-20261013.md)，以它为准。

### 9.4 条件写的两处前提（§4.1）

- B1：`redis-cli FLUSHALL` 会同时清掉账号目录，旧 player_id 不会再被用到，所以它不是 B1 的主要触发场景；真正会触发的是 Redis 从旧备份恢复、AOF 丢尾、手工删键。上文点名的「Redis HA 故障 runbook」在仓库里不存在，`dev_tools.ps1` 也没有清 Redis 的命令。D1 提案里没有「epoch 下限落库」的条目。
- B2：父键存在时 login 不读子表，C++ 拿不到 MySQL 行里的围栏值；Redis 父键也不是「已发出 DBTask」的上界。播种方案因此改了。

两处的处理见 [db-write-fencing-mysql.md](../design/db-write-fencing-mysql.md) §0.1、§7、§9.1。

### 9.5 关键事实（§6）

- 压测基线：`prev-summary.txt` 的 DB 段是 6 个阶段耗时列为 "-"，ok / err 计数有值。
- C++ 侧 db_task 只有一个生产点，不传 partition，这一点已静态确认；没确认的只剩线上实际的消息分布。
- 排序锁在 Redis 里的键名带前缀：`distributed:lock:kafka:ordering:{topic}:{key}:{msgType}`。

### 9.6 本轮新增的文档

- [db-write-fencing-mysql.md](../design/db-write-fencing-mysql.md)：条件写设计稿（§4.1 第 3 项）。
- [db-task-store-unavailable-deferral.md](../design/db-task-store-unavailable-deferral.md)：L-04 设计评审稿（§4.1 第 1 项）。
- [godb-pipeline-codex-tasksheet-20261013.md](godb-pipeline-codex-tasksheet-20261013.md)：Codex 任务单。
