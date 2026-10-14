# go/db 存盘任务：存储不可用时延后、不进死信（L-04）设计评审稿

> **状态**：设计评审稿 v0.2（2026-10-13，Claude）。已过一轮对抗评审（§13），**待负责人授权；未落码、未编译、未测试**（AGENTS.md §10.1）。
> **来源**：`tidb-phase1-and-launch-readiness.md` 的 L-04（「go/db 遇到 1040 或拿不到连接时改为延后，不消耗重试预算；死信加消费者和告警」）；[交接](../handoff/godb-pipeline-handoff-20261013.md) §4.1 第 1 项；[瓶颈文档](db-task-pipeline-bottlenecks.md) §4.4 的缺口 G2；全局单点交接的同名 P0。附录 A「mysql-connection-budget」批 A 是本文的起点，它没有经过独立复核，本文按 HEAD 重新核对后改写。
> **证据口径**：[C] 在 HEAD `0aa1027bc` 读代码确认；[D] 文档记载；[I] 推断。全部是静态阅读，没有实测。
> **缩写**：KOC = `go/db/internal/kafka/key_ordered_consumer.go`；PR = `go/db/internal/kafka/placement_route.go`；SR = `go/db/internal/logic/pkg/proto_sql/store_registry.go`。

---

## 0. 结论速览

1. **问题比「3 秒内耗尽重试」更紧。** 重试循环同一拍内可以反复认领同一条任务，MySQL 抖动不到 1 秒就可能把一笔存盘推进没有消费者的死信（§1）。
2. **第 1 批**（约 9 个文件，不动 proto、不动 DDL）：
   - handler 返回 `error`，保住驱动错误的类型；
   - 把「存储此刻不可用」的错误单独分类，写任务延后且不耗预算；
   - 已打开的库在运行期失联时进入冷却，复用现有的「落点库打不开」那条路；
   - 重试循环每拍每条任务至多尝试一次；
   - 加延后计数、死信计数、三个队列长度，以及三条告警。
3. **第 2 批**（另行评审）：死信的查看与重新入队工具；主库不可用时是否改为原地等待、让 Kafka 承担积压。
4. **L-04 管不到「MySQL 卡住不返回」**：连接没有读写超时，SQL 挂住时不产生任何错误。它要靠交接 §4.1 第 2 项里的读写超时补上，建议与本批同批或更早落地（§8）。
5. 第 1 批在所有 zone 部署之前，交接 §2.4 的约束不变：不要在有真实存档的环境 apply 会触发 MySQL 重建的清单。

---

## 1. 现状（HEAD 事实）

### 1.1 一笔写失败之后

- handler 返回的是字符串，不是 `error`：`handleDBWriteOp` 把驱动错误 `fmt.Sprintf` 成文本（KOC:829-830），`runDBOp` 再 `errors.New`（KOC:2040-2042）。到上层已经无法用 `errors.Is` / `errors.As` 判断是什么错 [C]。proto2mysql v0.2.0 自己是全程 `%w` 包装的，类型丢在 go/db 这三行。
- 失败的任务进 `deferFailedTask`（KOC:2072-2125）：Kafka 来源的 LPUSH 到共享的 ready 列表后 ack；重试来源的把收据搬回 ready。
- 重试预算在认领时判定（KOC:1177-1191）：`RetryCount >= 3` 进死信，否则加一。一笔写总共执行 4 次。
- 死信 `kafka:dead:queue:{topic}` 没有消费者、没有重放工具、没有指标、没有告警；全仓唯一的读者是合服预检的 LLEN [C]。

### 1.2 哪些失败不耗预算

`retryBudgetExempt`（PR:357-367）只豁免四种：排序锁被占、落点冻结、复核发现落点变了、落点库打不开（只对写）。

**SQL 返回的任何错误都耗预算**，包括 1040、连接断开、MySQL 重启。

「落点库打不开」的豁免实际只覆盖**从没打开过的额外库**：`StoreRegistry.Store` 对已打开的库直接返回缓存的句柄，不探活（SR:281-287）。本 zone 的主库在启动时就打开了，所以它任何时候都不在豁免里 [C]。

### 1.3 重试没有最小间隔

- 重试循环是 1 秒一拍、每拍最多认领 200 条（KOC:49、:1141-1154）。
- 搬回 ready 是 LPUSH 到队头，认领从队尾取，循环只在「队列空」或「满 200 次」时结束。队列里任务不多时，同一条任务可以在同一拍里被反复认领 [C]。
- 所以「3 次重试」可以在远小于 3 秒的时间里用完；被豁免的任务则没有任何最小间隔，积压时退化成每实例每秒最多 200 次的空转，每次都是一串 Redis 往返，每条每轮打一行 ERROR（KOC:2105）。

### 1.4 没有任何可观测

没有重试、延后、死信的计数，没有队列长度（`go/db/internal/metrics/metrics.go` 里只有 `frozen_deferred` 这一个相关的 label 值）[C]。仓库没有部署 redis_exporter。K8s 上 go/db 的指标端口目前没开：manifest 声明了 9160，但 ConfigMap 模板没写 `MetricsListenAddr`（`tools/scripts/k8s_deploy.ps1:3431-3474`）。

### 1.5 后果

MySQL 连接数超限、主从切换、重建（`mysql.yaml` 是 `strategy: Recreate`）、TiDB 滚动，都会让在跑的 go/db 把存盘推进死信。在线玩家不受影响（在线数据以 Redis 父键为准），但 MySQL 落后；父键一丢就是回档。

---

## 2. 原则

**重试预算只花在任务自身的问题上。** 数据超限、表结构不符、消息解不出来，重试多少次都不会好，耗完预算进死信等人处理是对的。存储此刻连不上、只读、忙，是环境问题，会随外部状态自己消失；为它耗预算，就是把一笔完好的存盘丢掉。

这与现有豁免的理由一致（PR:349-356 的注释），本文只是把同一条原则用到 SQL 错误上。

**读任务不同**：读不改数据，等待它的 login 最多等 8 秒（`entergamelogic.go:380`）。读没有「不能丢」的东西，只需要尽快告诉等待方结果。

---

## 3. 设计（第 1 批）

### 3.1 handler 返回 `error`

- `dbOpHandler`（KOC:327-332）的返回值由 `string` 改为 `error`；`handleDBWriteOp`、`handleDBReadOp` 用 `%w` 包住原始错误；`runDBOp` 直接返回，不再 `errors.New`。`publishDBOpResult` 一并改。
- 测试替身 `recordingHarness` 按现签名整张替换 `dbOpHandlers`（`key_ordered_consumer_test.go:130-155`），要同步改，用例数不能减少。
- 条件写之后会在这个基础上再扩成 `(writeOutcome, error)`（见 [db-write-fencing-mysql.md](db-write-fencing-mysql.md) §4.1）。两步分开做，避免互相返工。

### 3.2 错误分类

新增一个纯函数，把 SQL 错误归成四类。判定只用 `errors.Is` / `errors.As`，不匹配错误文本。

| 类 | 判定 | 指标 reason | 写任务 |
|---|---|---|---|
| 连接不可用 | `*mysql.MySQLError` 的 Number 为 1040（连接数满）、1203（用户连接数满）、1053（服务端正在关闭）；`driver.ErrBadConn`；`mysql.ErrInvalidConn`；`sql.ErrConnDone`；`net.Error`（拨号被拒、超时、连接重置）；`io.EOF` / `io.ErrUnexpectedEOF`；语句上下文超时（`context.DeadlineExceeded`）| `conn_unavailable` | 延后，不耗预算 |
| 只读 | Number 为 1290、1836（主从切换期间旧主只读）| `store_readonly` | 延后，不耗预算 |
| 忙 | Number 为 1205（锁等待超时）、1213（死锁）；TiDB 的 9007、8002（乐观事务写冲突）| `store_busy` | 延后，不耗预算 |
| 其它 | 以上都不是 | `other` | 照旧耗预算 |

- **交接点名的范围是第一类。** 第二、三类按同一条原则应当一并豁免，但它们不在交接的原始范围里，请在评审时逐类确认（§11 第 2 条）。
- 1205 / 1213 值得单独说：单行自动提交的写撞上它们几乎都是瞬时的；但如果某条 DDL 长时间拿着元数据锁，1205 会持续出现。豁免之后任务不会丢，告警会响（§3.6）；不豁免则是 3 次之后丢盘。
- TiDB 的错误号以 proto2mysql 集成用例里的可重试集合为准（9007、1213、8002）；其余 TiDB 瞬时错误（9001~9005 一类）等 TiDB 阶段按官方文档核对后再加，本批不凭记忆写。
- 驱动版本是 go-sql-driver v1.9.3（`go/db/go.mod:8`）。`*mysql.MySQLError` 的 `Number` 在本模块已有用法（`proto_sql/db.go:209-211`）；`mysql.ErrInvalidConn` 在同版本的 trade、data_service 测试里有用法。本机没有模块缓存，没有对着驱动源码核对，编译时由 Codex 确认。
- 语句上下文超时目前不会出现（SQL 用的是 `context.Background()`）；交接 §4.1 第 2 项落地后才有，分类先写好。
- 上下文被取消（`context.Canceled`：分区被收回、进程退出）不归入任何一类，走现有路径。

### 3.3 写任务：延后，不耗预算

- `retryBudgetExempt` 增加一个分支：错误属于前三类且 `op == "write"` 时豁免。
- 载荷格式不变（重试次数在 DBTask 的 `retry_count` 里，KOC:2074-2081 的「认领时加一、豁免时减回」机制不变）。新旧实例可以互相认领对方的重试收据；旧实例遇到连接错误照旧耗预算，混跑时最坏退回今天的行为。
- 先后顺序仍然靠排序锁和游标：延后的旧写回来时，如果同一玩家更新的写已经落库，它会被游标判过期丢弃（KOC:1579-1588）。条件写落地后由围栏兜底。

### 3.4 运行期失联的库进入冷却

`StoreRegistry` 已经有一套「打开失败后冷却」：失败时间记在 `failures` 里，冷却期内对同一个库的请求直接返回上次的错误，不再拨号（SR:49-54、:317-319）。它只管没打开过的库。

把同一套机制用到已打开的库上：

- handler 把错误归为「连接不可用」或「只读」时，调用注册表的新方法（例如 `ReportUnavailable(storageID, err)`），记下失败时间。
- `Store()` 对已打开的库多看一眼冷却：冷却期内返回包着 `ErrStoreUnavailable` 的错误。之后的路径全部是现成的：`routeTask` 把它包成 `errPlacementStoreUnavailable`（PR:309-316），写任务延后且不耗预算。
- 冷却到期后，下一条任务照常拿到句柄去执行，成功就清掉失败记录，失败就重新进入冷却。
- 冷却时长沿用同一个常量（10 秒），不做指数退避。存盘是异步的，多等几秒玩家无感；固定值最容易推理。

这样做的效果：

- 库不可用期间，绝大多数任务在选库这一步就被挡回去，不去拨号。没有冷却时，每条任务都要在 worker 上等到拨号失败；拨号卡住时同一个子分片里的任务会一起被拖住。
- 「忙」这一类不进冷却：它是某一行、某一条语句的事，不代表整个库不可用。

### 3.5 重试循环：每拍每条至多一次，主库冷却时不排空

- 每拍开始时读一次 ready 的长度 `n`，本拍最多认领 `min(n, 200)` 条。这样无论因为什么原因被搬回去，一条任务在一拍里至多尝试一次，「重试间隔」才真的是 1 秒。多一次 LLEN，换掉空转。多实例共用 ready 时 `n` 是近似值，仍然能把每实例每拍的认领次数限制在 `n` 以内。
- 本 zone 的主库在冷却期内时，这一拍直接跳过，不认领。否则每条任务都要走一遍「认领 → 取锁 → 读游标 → 选库失败 → 搬回 → 解锁」，全是白花的 Redis 往返。落到其它库的重试任务会因此多等一个冷却期，可以接受。
- 这两条同时把「3 次重试最短可以在 1 秒内用完」改成至少 3 秒。

### 3.6 读任务：立即回失败回执

现状：读任务遇到落点库打不开时照常耗预算，3 次后进死信；不给 login 回失败回执，login 等满 8 秒超时（唯一的失败回执是 `failReadTask`，PR:332-347，只用于「必须有落点记录而没有」）。重试成功时通常已经没人在等了。

改为：读任务遇到存储不可用（冷却期内选库失败，或 SQL 返回前两类错误）时，**立即调用 `failReadTask` 回失败回执，然后 ack**。不进重试队列，不进死信。

- login 秒级失败而不是等满 8 秒，客户端可以立刻重试。
- 死信里不再混入读任务。否则 MySQL 每次不可用，每一次登录尝试都会留下一条读死信，§3.8 的死信告警就失去判别力。
- 这改变了 `player-storage-placement.md` §6.1 实现口径里「读照常耗预算，几次后进死信」的说法，落码时要同步改那份文档（它目前有另一个会话在改，先对齐）。
- 这是登录路径上可见的行为变化（失败得更快），请负责人确认（§11 第 3 条）。如果不接受，退一步的做法是：读任务照旧重试，但预算用完时丢弃并计数，不进死信。

### 3.7 指标

都是低基数，不带 player_id。

| 指标 | label | 说明 |
|---|---|---|
| `db_task_deferred_total` | `op`；`reason` ∈ conn_unavailable / store_readonly / store_busy / store_open_failed / ordering_lock_busy / placement_frozen / placement_moved / redis_unavailable / other | 在 `deferFailedTask` 一处计数，覆盖所有延后原因，一眼能看出任务为什么没落库 |
| `db_task_dead_letter_total` | `reason` ∈ budget_exhausted / ordering_incomparable / stale_topic / missing_required / poison / legacy_payload / invalid_partition | 死信的每个入口各计一次（KOC:1123、:2127、:2145、:2180）|
| `db_retry_queue_length` | `queue` ∈ ready / processing / dead | 每 15 秒 LLEN 采样一次。processing 取本实例自己的列表。采样失败保留上一个值并计错误数 |
| `db_store_cooldown` | `pool` ∈ home / extra_zone / extra_non_zone | 该类库当前有多少个在冷却；类别沿用连接池指标的取值 |

ready 和 dead 是同一个 topic 的所有实例共用的列表，每个副本报的是同一个数，告警里用 `max`。

连接池本身的指标已经由另一会话的 G2b 导出（`mmorpg_sql_pool_*{sql_client="db", pool}`），本文不再注册第二个收集器。

### 3.8 告警

新文件 `deploy/k8s/db-task-retry-alerts.yaml`，写法同 `owner-epoch-alerts.yaml`（`PrometheusRule`，手工 `kubectl apply`）。

| 告警 | 条件 | 级别 | 含义 |
|---|---|---|---|
| `DbTaskDeadLetterNonEmpty` | `max(db_retry_queue_length{queue="dead"}) > 0` 持续 1 分钟 | critical | 有存盘或坏消息等人处理 |
| `DbTaskDeferredByStore` | `conn_unavailable` / `store_readonly` / `store_busy` / `store_open_failed` 的延后速率大于 0 持续 2 分钟 | critical | 存盘正在因为存储问题延后，MySQL 在落后 |
| `DbTaskRetryQueueGrowing` | `max(db_retry_queue_length{queue="ready"})` 持续 10 分钟上升，或超过阈值 | warning；阈值待压测确认 | 重试队列排不动，Redis 内存在涨 |

- `owner-epoch-alerts.yaml` 里记着一条经验：计数器序列从无到 1 时 `increase` 会漏第一次（:57-62）。死信告警用的是队列长度的绝对值，不受影响；延后告警用速率，持续条件能盖住这个问题。
- **前提**：K8s 上 go/db 的指标端口要先能抓到（db ConfigMap 的 `MetricsListenAddr`，属于另一会话的 G2b，HEAD 上还没有）；仓库不部署 Prometheus，规则文件是手工 apply 的。这两条不满足时告警是哑的，要在发布检查里写明。
- 另一会话正在写的 `deploy/k8s/mysql-connection-alerts.yaml` 在描述里提到「go/db 把 1040 当普通失败 → 进死信」和手工查死信长度的步骤。本批落地后那段描述会过时，两份文件的分工要先对齐（§9）。

### 3.9 日志

- 存储不可用引起的延后打 WARN，并且限速（每个库每个冷却期一条汇总）。现在是每条每轮一行 ERROR，一次 MySQL 重启会刷出上万行，把真故障淹掉。
- 进入和离开冷却各打一条，带库的类别和原因。

---

## 4. 第 2 批（另行评审，这里只列方向）

### 4.1 死信工具

L-04 的原文是「死信加消费者和告警」。不建议做自动消费者：任务进死信都有原因，自动重放等于把分类白做。建议做一个命令行工具（例如 `go/db/cmd/deadletter`）：

- 列出：按格式、表、原因汇总。死信里现在混着四种格式（带分区的重试载荷、不带分区的旧载荷、裸 DBTask、带 `0x03` 前缀的毒消息 JSON）[C]，工具要都能解。
- 查看：解出单条的内容。
- 重新入队：搬回 ready，重试次数清零；默认只演练不执行。顺序安全靠现有的游标（以及将来的围栏）：重新入队的旧写如果已被更新的写超过，会被丢弃。
- 新进死信的条目带上原因和时间（现在原因只在日志里）。死信没有任何程序读者，加一种新格式不影响兼容。

条件写上线后 `stale_owner` / `legacy_after_seq` 也进死信，那份设计的处置步骤依赖这个工具。

### 4.2 主库不可用时原地等待

第 1 批的做法是把失败的写搬进 Redis 的 ready 列表。库不可用期间，新到的写会以存盘速率持续流进这个列表（每条是整行数据），Kafka 的位点照常前进。Redis 是共享的单实例，登录也在用。

另一种做法是 Kafka 来源的写在主库冷却期间不搬走、不 ack，在 worker 上等到冷却结束再重试。通道满了之后消费自然停下，积压留在 Kafka。好处是 Redis 不涨、顺序不乱、恢复时没有重放风暴；代价是消费整体停住（库不可用时本来也落不了库），以及额外库不可用时不能用这个办法（会拖住同一子分片里落主库的任务）。

是否做、何时做，取决于「存盘速率 × 单条大小 × 可容忍的不可用时长」与 Redis 内存余量的比较，需要 R20 的数据。今天的行为同样是把载荷堆在 Redis（堆在死信里），所以第 1 批不会比现在更差。

### 4.3 Redis 侧的环境故障

同一条原则也适用于这些现在耗预算的失败：落点查询失败（PR:274）、复核读失败（PR:246）、游标读失败（KOC:1575）、epoch 守卫读失败（KOC:661）、游标写失败（KOC:1639）、缓存发布失败（KOC:841）。它们多数只在「落点 Redis 与主 Redis 分开部署、且只有其中一个不可用」时才会真的耗到预算（主 Redis 不可用时重试队列本身也不可用）。dev 与 K8s 现在是同一台，所以排在第 2 批。指标里先用 `redis_unavailable` 把它们数出来。

---

## 5. 兼容与上线

- 不改 proto、不改载荷格式、不改 Redis 键、不做 DDL。
- 新旧实例并存：新实例对存储不可用的写不耗预算，旧实例照旧耗。最坏退回今天的行为。
- 逐 zone 滚动，一次一个 Pod；每个 zone 观察 `db_task_deferred_total` 平时为 0、死信长度不变、`db_task_stage_seconds` 没有回归。
- 回滚：换回旧镜像即可，没有需要清理的状态。
- 发布纪律照瓶颈文档 §4.5（低峰、同窗口不重启 Redis / MySQL）。
- 故障注入（暂停 MySQL、压低连接上限）只在 kind 或 staging 做。

**对交接 §2.4 的影响**：第 1 批在某个环境的所有 zone 部署之后，在那个环境 apply 会重建 MySQL 的清单不再丢盘，但仍是一次存储中断：期间的写堆在 Redis 的 ready 列表里。要在低峰做，并先确认 Redis 内存余量。部署之前，§2.4 的约束原样有效。

---

## 6. 测试与 Codex 验证

前提：Claude 没有编译、没有运行任何测试，下面由 Codex 执行；失败即停，不连续重试。

**单测**（工作目录 `go/db`）：

```sh
gofmt -l .
go vet ./...
go test ./internal/kafka/... ./internal/logic/pkg/proto_sql/... ./internal/metrics/... -count=1
```

新增用例必须覆盖：

- 分类函数（表驱动）：每一类的每个判据，以及它们被 `%w` 包了一层之后；不应命中的：1062、1406、大字段闸拒绝、`context.Canceled`、普通文本错误。
- `retryBudgetExempt`：三类错误下写豁免、读不走这条路；原有四种豁免不变。
- `deferFailedTask`：重试来源的写因连接不可用被延后时重试次数不变，连续超过 3 轮仍在 ready，不进死信；载荷逐字节不变；`db_task_deferred_total` 递增。
- 冷却：用注册表现有的假时钟（`store_registry_test.go` 的 `fakeClock`）验证进入、期内直接返回、到期后重试、成功后清除；「忙」不进冷却。
- 重试循环：ready 里有 3 条必失败的任务时，一拍只认领 3 次；主库冷却时一拍认领 0 次。
- 读任务：存储不可用时立刻有失败回执，ready 与死信长度都不变。
- handler 签名改动后原有用例全部保留。

目前没有任何测试调用真实的 `handleDBWriteOp` / `handleDBReadOp`；`go/db/internal/dbtest/fakedriver.go` 可以注入任意错误，用它补上「驱动错误 → 分类 → 去向」这一段。

**故障注入**（本地 compose 或 kind，一个 zone，用 robot 或 `cmd/data_stress` 产生写）：

1. 暂停 MySQL 容器 30 秒再恢复。
2. 把 `max_connections` 设成当前连接数，再杀掉 go/db 的空闲连接，逼它重连时撞 1040。

两种情况都要满足：期间 `db_task_deferred_total{reason="conn_unavailable"}` 大于 0；死信长度不变；恢复后写入恢复，ready 排空；抽样玩家在库里的行等于最后一次写入。

保留：go/db 日志里含冷却与延后的行、故障前中后三个时刻的 `/metrics`、三个队列的 LLEN。

注意：第 1 种注入在没有读写超时的情况下，已经发出的语句会一直挂着不返回（§8），这本身就是要观察的现象，如实记录。

**通过标准**：单测全绿、无 SKIP；两种注入下死信长度为 0。

---

## 7. 改动清单（第 1 批）

| 文件 | 改动 |
|---|---|
| `go/db/internal/kafka/key_ordered_consumer.go` | handler 签名；`runDBOp`；`deferFailedTask` 的计数与日志；重试循环的每拍上限与冷却跳过；死信入口计数；队列长度采样 |
| `go/db/internal/kafka/placement_route.go` | 豁免分支；读任务的失败回执路径 |
| `go/db/internal/kafka/store_errors.go`（新）| 错误分类的纯函数 |
| `go/db/internal/logic/pkg/proto_sql/store_registry.go` | 已打开的库的冷却 |
| `go/db/internal/metrics/metrics.go` | 四个新指标 |
| 测试 3~4 个 | `key_ordered_consumer_test.go`、`placement_route_test.go`、`store_registry_test.go`、新的分类测试 |
| `deploy/k8s/db-task-retry-alerts.yaml`（新）| 三条告警 |
| 文档 | `player-storage-placement.md` §6.1（读任务口径）、`tidb-phase1-and-launch-readiness.md` L-04 行的状态、`docs/PROGRESS.md` |

约 9~10 个代码与清单文件，不动 proto、不动 DDL、不跨语言。动代码前请负责人授权（交接 §5 第 3 项）。

`key_ordered_consumer.go` 是多条修复线共用的文件，合入顺序按瓶颈文档 §5.2：本批在子 shard 修复之后、读写分区对齐与条件写之前。

---

## 8. 管不到的：MySQL 卡住不返回

`newMysqlConfig` 没有设读写超时（`proto_sql/db.go:108-121`），handler 也不传 ctx。MySQL 卡住时（例如排在一条 DDL 的元数据锁后面，或者进程被暂停）：

- SQL 不返回，也不报错，本文的分类根本没机会运行。
- 该子分片的协程永久阻塞 → 子通道满 → 分发协程阻塞 → 任务通道满 → 消费循环和**全实例唯一的重试协程**都阻塞（KOC:1217-1218）。本实例所有分区的重试停止，`Stop()` 挂在等待上直到被强杀 [C]。

这要靠交接 §4.1 第 2 项里的两条来解决：DSN 补 ReadTimeout / WriteTimeout；handler 接 ctx。超时之后驱动返回的是连接类错误，正好落进本文的第一类。

**建议**：把「DSN 读写超时」这一条从 G1 治标小 PR 里提出来，与本批同批或更早落地。没有它，「暂停 MySQL」这种最常见的故障形态下 L-04 不生效。请负责人定（§11 第 4 条）。

---

## 9. 与其它工作的关系

| 工作 | 关系 |
|---|---|
| 条件写（[db-write-fencing-mysql.md](db-write-fencing-mysql.md)）| 本批先落地。条件写在 `error` 签名上扩展；它的 `stale_owner` / `legacy_after_seq` 进死信，依赖本批的死信告警和第 2 批的工具 |
| G1 治标小 PR（交接 §4.1 第 2 项）| 读写超时是本批的前置（§8）；其余几条互不依赖 |
| G2b（另一会话）| 连接池指标已由它导出；db ConfigMap 的指标端口由它补；它新写的 `mysql-connection-alerts.yaml` 与本批的告警要对齐分工 |
| WAITAOF（全局单点交接的 P0）| 改的是同一段代码（`pushPayloadDurably`、`moveRetryReceipt` 与四个死信写入点）。归属待定（交接 §5 第 17 项）。本批让更多的写在 ready 列表里停留更久，Redis 接高可用之前这一条更值得做 |
| 重试只交给分区属主（④）| 同一段代码。本批不改重试的归属 |
| 连接预算批 B | 无关 |

L-04 的归属三份交接说法不一：全局单点交接把它列在自己的阶段 0，开服交接说「一起做」，本交接自行认领。请负责人定下后通知另外两个会话（交接 §5 第 4 项）。

---

## 10. Java 版（AGENTS.md §12）

**不适用**：Java 版没有 go/db 这样的消费进程，也没有重试队列和死信；存盘由 scene 进程内的存储线程池直接执行。

等价的缺口是另一种形态：Java 的最终写回在 5 秒内重试 3 次后放弃，实例已移出内存，最新状态丢弃（`StoragePlayerRepository.java:459-465`）。这一条已经在 Java 仓库 `PARITY.md` 第 60 行登记为「Java 待做：最终写回失败后保留最新状态持续重试直到成功」，不需要新开一行。

功能落码后在 `PARITY.md` 追加一行「不适用（Java 没有 Kafka 存盘链）」，备注指向第 60 行。设计阶段不登记。

---

## 11. 待拍板与未核实

**待拍板**：

| # | 事项 | 建议 |
|---|---|---|
| 1 | 第 1 批开工授权；L-04 归本交接（交接 §5 第 3、4 项）| 归本交接，先于条件写 |
| 2 | 「只读」「忙」两类是否与「连接不可用」一起豁免（§3.2）| 一起 |
| 3 | 读任务改为立即回失败回执（§3.6）| 采纳；不接受时用退一步的做法 |
| 4 | DSN 读写超时是否与本批同批（§8）| 同批或更早 |
| 5 | 告警文件与另一会话的 `mysql-connection-alerts.yaml` 怎么分工（§3.8）| 连接数、连接池归那份；重试、延后、死信归本批 |
| 6 | 第 2 批三项各自是否立项（§4）| 死信工具立项；原地等待等 R20 数据；Redis 侧故障等落点 Redis 独立部署时再做 |

**未核实**：

- go-sql-driver v1.9.3 把断连表现成哪种错误（`ErrInvalidConn`、`ErrBadConn` 还是网络错误），以及 `database/sql` 对 `ErrBadConn` 是否先内部重试。分类把三种都列了，不依赖这一点。
- `database/sql` 在连接池耗尽时的等待行为。本批不改变它。
- 1040 → 死信这条链路本身是读代码推断的，没有实测（PROGRESS.md:7190）。§6 的故障注入就是为了实测它。
- TiDB 的瞬时错误号集合。
- go-redis 对已取消上下文的行为：停机时在途任务失败会打 `retry reschedule failed` / `DATA-LOSS PREVENTED`，属于现状，本批不处理。

---

## 12. 考虑过而不采用的方案

| 方案 | 不采用的原因 |
|---|---|
| 按错误文本匹配（`strings.Contains(err, "Too many connections")`）| 脆弱。login 的生产者就因为文本匹配误中而自锁过（瓶颈文档 G0）|
| 只把预算从 3 调大 | 治不了根：重建一次 MySQL 是分钟级，调多大都会耗完 |
| 所有 SQL 错误都豁免 | 表结构不符、数据超限这类错误会无限重试，永远不会被人看到 |
| 给载荷加「最早重试时刻」字段 | 要改载荷格式，新旧实例互认会出问题；库级冷却加每拍上限已经够用 |
| 指数退避 | 固定冷却已经把拨号压到每个冷却期一轮，再退避只是让恢复变慢 |
| 自动重放死信 | 任务进死信都有原因，自动重放会把分类白做 |
| 第 1 批就做原地等待 | 改变消费的整体行为，要有数据支撑；与现有「落点库打不开」的处理也不一致 |

---

## 13. 修订历史

- v0.1（2026-10-13）：初稿。
- v0.2（2026-10-13）：对抗评审后的修订，见下。

评审记录待补（本稿提交评审后填写）。
