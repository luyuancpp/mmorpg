# 存盘管线（go/db）上线待做

> **用途**：存盘管线已经落码、但还没有验证或还没有做完的上线前事项。负责人 2026-10-13 的要求是「先实现代码，先不管测试，测试记录到上线待做里面」，所以代码先落，验证全部记在这里。
> **执行者**：编译、测试、故障注入由 Codex 执行（AGENTS.md §10.1）；部署与集群操作由负责人执行（AGENTS.md §9）。
> **维护**：每做完一项，在该项后面写日期、执行者和结果（通过 / 失败 + 证据位置），不要删行。新增事项追加在对应小节末尾。
> **相关**：[交接](../handoff/godb-pipeline-handoff-20261013.md)、[Codex 任务单](../handoff/godb-pipeline-codex-tasksheet-20261013.md)、[L-04 设计](../design/db-task-store-unavailable-deferral.md)、[条件写设计](../design/db-write-fencing-mysql.md)、[瓶颈文档](../design/db-task-pipeline-bottlenecks.md)。

---

## 1. 第 0 批与 proto2mysql 条件写原语

| # | 事项 | 怎么做 | 状态 |
|---|---|---|---|
| 1.1 | 第 0 批（子 shard 打散、login 自锁、位点重置判定、汇总脚本 DB 段）的编译与单测 | [Codex 任务单](../handoff/godb-pipeline-codex-tasksheet-20261013.md) 任务 A | 待 Codex。负责人已自行安装 Go 1.27.2；Go 模块下载的同意未明确 |
| 1.2 | proto2mysql `SaveConditional` 的离线、真库（MySQL 8.4 / TiDB 8.5）、竞态测试 | 同上，任务 B | 待 Codex。负责人已同意拉取两个镜像 |
| 1.3 | 1.2 全绿后合入 proto2mysql main，不打 tag；go/db 用伪版本钉 main 上那个提交 | 负责人 2026-10-13 的决定 | 等 1.2 |

---

## 2. L-04 第 1 批（存储不可用时延后、不进死信；业务池读写超时）

代码已落：第 1 批随 2026-10-14 的提交 `ebccaf04d`（已推送）入库；之后的复核修订在工作区未提交。文件清单见 [L-04 设计](../design/db-task-store-unavailable-deferral.md) §7。

### 2.1 编译与静态检查

工作目录 `go/db`：

```sh
gofmt -l internal/storeerr internal/kafka internal/logic/pkg/proto_sql internal/config internal/metrics
go vet ./...
go build ./...
```

通过标准：`gofmt -l` 无输出；vet 与 build 退出码 0。失败时保留第一条 error 及其前后 20 行。

状态：2026-10-14 提交 `ebccaf04d` 的说明记录「已检查范围的 Go 格式检查通过」（范围未写明；vet 与 build 未记录）。之后的复核修订（未提交）需要重跑。

### 2.2 已随代码写的单测

工作目录 `go/db`：

```sh
go test ./internal/storeerr/... ./internal/logic/pkg/proto_sql/... ./internal/kafka/... ./internal/config/... ./internal/metrics/... -count=1 -v
```

- 新增：`internal/storeerr/storeerr_test.go`（4 个用例）、`internal/logic/pkg/proto_sql/store_health_test.go`（7 个用例）。
- 改写：`internal/kafka/placement_route_test.go` 的 `TestStoreUnavailable_WriteKeepsBudgetReadFailsFast`（读任务改为立即回失败回执）；`key_ordered_consumer_test.go` 的 `recordingHarness`（handler 签名改为返回 error）。
- 基线：在改动前的提交上跑同一条命令对比失败集合（L-04 提交之前就是当前 HEAD；提交之后用它的父提交建临时 worktree）。`internal/kafka` 的既有用例依赖 miniredis 监听 127.0.0.1，沙箱拦回环时先申请放行。
- 通过标准：新增用例全部 PASS；既有用例的失败集合是基线失败集合的子集。

状态：2026-10-14 提交 `ebccaf04d` 的说明记录「storeerr、proto_sql、config、metrics 定向单测通过」；`internal/kafka` 包未跑（本机缺 `miniredis/v2 v2.36.1` 模块且离线模式阻止下载）。这是复核修订之前的代码；修订改了 `proto_sql`（探活、冷却计数，新增 2 条用例）、`metrics`、`internal/kafka/placement_route.go`，需要重跑；`internal/kafka` 需要先解决模块下载（Go 模块下载的同意仍未明确）。

### 2.3 还没写的单测（上线前补）

| 用例 | 断言 |
|---|---|
| `retryBudgetExempt` 新分支 | 四类存储侧标记下写豁免、读不豁免；原有四种豁免不变；未打标记的 `net.Error`（来自 Redis）不豁免 |
| `deferReason` 全表 | 每个原因各一例，含「Redis 侧超时归 redis_unavailable」 |
| 重试循环每拍上限 | ready 里 3 条必失败的任务一拍只认领 3 次；两个消费者共用一个 miniredis 时每个各自至多 3 次 |
| 主库冷却时整拍跳过 | 注入实现了 `storeHealth` 的替身，冷却时一拍认领 0 次，三个队列长度照常采样 |
| 读任务预算用完 | 不进死信，收据被摘掉，`db_task_read_abandoned_total{reason="budget_exhausted"}` 加一 |
| 写任务存储类延后 | 重试来源的写因 `conn_lost` 延后时重试次数不变，连续超过 3 轮仍在 ready；载荷逐字节不变 |
| 计数 | `db_task_deferred_total`、`db_task_dead_letter_total`（含毒消息经 `persistPoisonMessage` 计数）按原因递增 |
| `runDBOp` 上报 | handler 返回 `conn_lost` 时调用 `ReportConnLost`，成功时调用 `ReportHealthy`，复核或缓存回写失败时都不调用 |
| 业务池配置 | `newServiceMysqlConfig` 设了三个超时与两个会话变量；负数不设；`NewMysqlConfig`（迁移用）不带它们 |
| 探活不经过连接池 | `pingStore` 有连接器时新建连接 Ping 后关闭；池里只有死连接时不误判；池满时不误判 |
| 冷却指标收集器 | `NewStoreCooldownCollector` 按 source 输出三类 pool 的序列 |

状态：未写。

### 2.4 故障注入

只在本地 compose 或 kind 做，一个 zone，用 robot 或 `go/db/cmd/data_stress` 产生写。保留：go/db 日志里含 cooldown / deferred 的行、故障前中后三个时刻的 `/metrics`、三个列表的 LLEN。

| # | 注入 | 通过标准 |
|---|---|---|
| 2.4.1 | `docker pause` MySQL 容器 30 秒再 `unpause` | 期间 `db_task_deferred_total{reason=~"conn_lost\|store_cooldown"}` 增长，`db_store_cooldown_stores{pool="home"}` 变成 1；死信长度不变；go/db 进程没有挂死（`Stop()` 能在宽限期内退出）；恢复后 ready 排空，抽样玩家在库里的行等于最后一次写入 |
| 2.4.2 | `SET GLOBAL max_connections` 设为当前连接数，再 KILL go/db 的空闲连接逼它重连撞 1040 | 期间 `reason="conn_refused"` 增长；**不进冷却**；池内已有连接的任务照常落库，ready 不按全量入流增长；死信长度不变 |
| 2.4.3 | 在 dev 库上用一个未提交事务读 `player_database` 持有元数据锁，另一会话发起 `ALTER TABLE` 排队，让后续写排在它后面 | 写在约 10 秒后以 `reason="store_busy"`（1205）延后，没有挂到 30 秒的读超时；释放后写入恢复，库内行等于最后一次写入；做完撤掉临时 DDL |
| 2.4.4 | 1226：`ALTER USER … WITH MAX_USER_CONNECTIONS` 压低应用账号的连接上限 | 错误号确实是 1226，归 `conn_refused` |

状态：未做。

### 2.5 上线前置

| # | 事项 | 状态 |
|---|---|---|
| 2.5.1 | 目标环境的 go/db 指标端口能抓到（db ConfigMap 的 `MetricsListenAddr: ":9160"` 已在 HEAD 的 `k8s_deploy.ps1` 里），有 ServiceMonitor / PodMonitor | 待确认 |
| 2.5.2 | `kubectl apply -n observability -f deploy/k8s/db-task-retry-alerts.yaml`（仓库不部署 Prometheus，手工 apply）| 待做 |
| 2.5.3 | 上线前记录各 zone 现有死信长度（`LLEN kafka:dead:queue:db_task_zone_<zone>`，换代后带 `_g<N>`），否则 `DbTaskDeadLetterNonEmpty` 部署后常亮；存量死信由负责人决定怎么处理 | 待做 |
| 2.5.4 | 与 G2b 那个会话对齐：`deploy/k8s/mysql-connection-alerts.yaml` 描述里「go/db 把 1040 当普通失败 → 进死信」在本批部署后过时 | 待对齐 |
| 2.5.5 | `DbTaskRetryQueueGrowing` 的阈值（现为 1000）按压测数据定 | 待 R20 |
| 2.5.6 | 本批提交后在 Java 仓库 `PARITY.md` 追加一行「不适用（Java 没有 Kafka 存盘链）」，备注指向第 60 行 | 待提交后 |
| 2.5.7 | TiDB 阶段：确认 TiDB 接受会话变量 `lock_wait_timeout`，不接受时 `SessionLockWaitSeconds` 写 -1 | 待 TiDB 阶段 |

### 2.6 部署顺序与红线

- 逐 zone 滚动，一次一个 Pod，低峰，照瓶颈文档 §4.5 每换一个 Pod 核对 lag、重试队列、processing、死信。
- **G2b 的 `mysql.yaml`（apply 会重建唯一的 MySQL）**：在本批于该环境所有 zone 部署完之前，有真实存档的环境不要 apply。部署完之后 apply 仍是一次存储中断：期间的写只在 Redis 的 ready 里有一份，**同一窗口不得重启或清空 Redis**，先确认 Redis 内存余量。负责人 2026-10-13 对「何时 apply」的答复是先实现代码、测试记入本清单，具体窗口未定。
- 条件写落地之前：同一窗口不跑会阻塞 DML 的 DDL，不暂停 MySQL（L-04 设计 §8 的残余窗口）。

---

## 3. 后续批次（已授权或已分工，尚未落码）

| 项 | 状态 |
|---|---|
| MySQL 行内围栏（条件写）| 负责人在另一会话确认由「玩家消息投递与位置管理」会话落码；该会话先出设计 v0.3，等 L-04 落地与 1.3 之后再动 go/db |
| ① 读写分区对齐 | 负责人 2026-10-13 授权，并接受「db 积压超过 8 秒时进场失败」的行为变化；排在 L-04 之后 |
| ③ 消费组按 zone 拆 | 负责人 2026-10-13 授权，并定为**开服门槛**（开服要支持单 zone 回档或合服）；前置：Kafka 镜像钉版本 |
| ④ 重试只交给分区属主 | 负责人 2026-10-13 定为先做并修订 TiDB 决策 D5；未授权开工 |
| ④B2 换代 topic（在线加分区）| 负责人 2026-10-13 定为要做，排在条件写之后 |
| WAITAOF（go/db 的重试与死信入队）、scene 发 DBTask 失败后的脏快照、Lua `tonumber`（go/db 一处）、db_task 的 retention.bytes 与 PVC | 负责人 2026-10-13 定为归本交接；未授权开工 |
| L-04 第 2 批：死信查看与重新入队工具 | 未授权 |
