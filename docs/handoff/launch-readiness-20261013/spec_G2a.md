# G2a 快照审计管道扩容 —— 实现规格

> 状态:规格稿(Claude,2026-10-12,只读代码与文档写成;**未编译、未测试,全部运行证据待 Codex**)。
> 2026-10-12 已过一轮对抗式评审,成立的问题已改进正文(标「评审」字样),逐条处理见文末「评审处理记录」。
> 范围:go/data_service 的 player_snapshot 消费者(按分区并发 + 批量落库)、lag / 落库耗时指标与告警、
> 审计 topic 离线换代 runbook、消息大小链路核对。transaction_log 消费者只加两行指标,不改行为。
> 硬约束:不改变「快照是否必达 / 是否可丢」的任何现有语义;凡涉及这类语义的点全部列在 §15 交负责人拍板。

---

## 0. 结论(先读这节)

1. **今天的拓扑下,「按分区并发」本身几乎不加吞吐,真正的杠杆是「批量落库」和「换代扩分区」。**
   data-service 每个 zone 2 副本(`deploy/k8s/manifests/go-svc/data-service.yaml:35`),所有 zone 共用一个消费组
   (`go/data_service/etc/data_service.yaml:241`)。zone 数 Z ≥ 2 时实例数 2Z ≥ 4 > 3 个分区,每个实例最多分到 1 个分区,
   单实例内再按分区并发没有东西可并发。全球并行度上限 = min(分区数 P, 2Z);每条循环的吞吐上限 = 1 / 单条落库耗时。
   本机 MySQL 开着 `sync_binlog = 1`、`innodb_flush_log_at_trx_commit = 1`(`deploy/k8s/manifests/infra/mysql.yaml:66-67`),
   逐条自动提交 = 每条快照两次 fsync;攒 B 条一条语句 = 每 B 条两次 fsync。
2. **方案三件事,按这个顺序落:**
   - (a) 代码:消费者改为「每分区一个 goroutine + 可配批量 + 实例级落库并发上限」,store 加多行 ODKU 批量写;
     **默认配置 = 现状逐字等价**(批 1 条、并发 1),开关在 yaml。
   - (b) 观测:broker 侧权威的消费组 lag / 保留期丢失量探针、落库耗时 / 批大小 / 单条字节直方图、8 条告警。
   - (c) 运维:开服前按离线换代流程把 `player_snapshot_topic` 换到新一代、更多分区(建议 12,待拍板)。
3. **第 4 项(消息大小)核对结论:快照的真实上限约 1,000,000 字节,在 C++ 生产端,不在消费端。**
   超过即在 `produce()` 本地被拒、只打 LOG_ERROR、快照丢失;消费端 16MiB 的上限从来到不了。这是既有行为,
   本规格**不改**,列为 §15 拍板项。
4. **还有一处既有的「静默可丢」比积压更早到:broker 的 `log.retention.bytes`(prod 每分区 512MiB)。**
   审计 topic 只显式声明了 `retention.ms`(30 天),积压超过每分区 512MiB 时未消费的快照会被删。本规格只加**检测**
   (`retention_gap` 指标 + critical 告警),是否改保留策略交负责人(§15)。
   **【评审补】换算成时间才看得出严重性**:可容忍的积压时长 T_safe = P × retention.bytes ÷ (R × 平均快照字节)。
   按 §12 的示例(R=128/s、300KB)约 **41 秒**(P=3)/ **约 164 秒**(P=12);即使快照只有 10KB 也只有约 20 / 80 分钟。
   也就是说,「30 天保留、宁可积压不丢」在开服速率下**不成立**:一次几分钟的 DB 抖动、或 supervisor 默认 60s 的冷却,
   就足以让未消费的快照被删。20Gi 的 Kafka 盘也撑不起真正的 30 天(§10 第 0 步)。所以 §15 Q-1 是**开服前必须拍板**的阻塞项,
   §4.7 的积压告警按「删除线的时间」设阈值与 `for`,§16 的「放慢审计保护同步路径」只在 Q-1 拍板之后才可用。

---

## 1. 现状事实(全部附 文件:行号)

### 1.1 生产端(C++ scene)

- 进场拍 LOGIN:`cpp/libs/services/scene/player/system/player_lifecycle.cpp:3555`;退场拍 LOGOUT:`:2737`(在存盘之前)。
  两处都**忽略返回值**,发送失败不重试。
- `SnapshotSystem::CaptureAndSend`(`cpp/libs/modules/snapshot/snapshot_system.cpp:16-115`):
  整份 `PlayerAllData` 序列化成两个 blob(`:33-50`)→ 号段取 snapshot_id,取不到 fail-closed 跳过(`:55-67`)→
  序列化整条 `PlayerSnapshotEntry`(`:84-97`)→ key = player_id 发往 `audit::AuditTopicName(kPlayerSnapshotTopicBase)`(`:99-107`)→
  成功打 `LOG_INFO ... size=<bytes>`(`:109-112`)。失败打 `LOG_ERROR ... Kafka send failed ... err=` 并返回 0(`:102-107`)。
- 生产者配置只设了 `bootstrap.servers` / `dr_cb` / `enable.sparse.connections` / `enable.idempotence`
  (`cpp/libs/engine/infra/messaging/kafka/kafka_producer.cpp:59-76`),**没设 `message.max.bytes`、没开压缩**。
- `ERR_MSG_SIZE_TOO_LARGE` 落入 `ClassifyProduceResult` 的 default → `kReject`(`kafka_producer_policy.h:35-44`),
  计 `IncProduceRejected` 并打限频日志(`kafka_producer.cpp:161-165`)。
- topic 有效名 = `<基名>_g<AuditTopicGeneration>`(`cpp/libs/modules/audit/audit_topic.h:31-58`;
  `bin/etc/base_deploy_config.yaml:138-156`,当前 1)。**transaction_log 与 player_snapshot 共用同一个世代号**。

### 1.2 topic、分区、世代

- `Kafka.TopicGeneration: 1`、`SnapshotPartitions: 3`、`TransactionLogPartitions: 6`、`RetentionMs: 2592000000`
  (`go/data_service/etc/data_service.yaml:232-242`);代码默认值同(`go/data_service/internal/config/config.go:458-473`)。
- 分区数是不可变契约:`kafkautil.EnsureTopics` 只建缺失 topic、不原地扩,并建 broker 侧 marker topic 锁死分区数
  (`go/shared/kafkautil/topic_init.go:80-91`、`:159-184`)。改分区只能换代(`data_service.yaml:229-231`;
  `docs/design/db-task-kafka-partition-contract.md:32-69` 的离线流程)。
- 预建分区数写死在本机两处:`deploy/docker-compose.yml:189-194`(6 / 3)、`tools/scripts/start_game.ps1:148`、`:160-161`;
  K8s 由 `k8s_deploy.ps1` 从服务 yaml 注入(`tools/scripts/k8s_deploy.ps1:5592-5621`;`deploy/k8s/manifests/infra/kafka-topic-init.yaml:69-73`)。
- broker 开 auto.create + `num.partitions=1`(`deploy/docker-compose.yml:44`),所以 scene 必须晚于预建 / data-service 启动。
- `deploy/k8s/README.md:673-674` 写的换代顺序是「建 g2 → 滚 data-service 与 scene → 老 topic 排空后删」:
  data-service 一旦切到 g2 就不再消费 g1,而 scene 还没滚完时仍在写 g1 —— **这段顺序会丢 g1 尾部**(见 §10,本规格修正)。

### 1.3 消费端(go/data_service/internal/kafka)

- 每实例一个 kafka-go group reader(`consumer.go:125-138`;`MaxBytes: 16<<20` `:133`;`CommitInterval: 0` 同步提交 `:136`),
  一个 goroutine(`start.go:133-141`),`NewSnapshotConsumer(SnapshotConsumerConfig{}, ...)` 用零值配置(`start.go:137`)。
- 逐条:Fetch → decode → `InsertSnapshotIfGuidAbsent`(有界重试 5 次 × 1s)→ 提交这一条(`snapshot_consumer.go:73-114`)。
  重试用尽:`markFailed`、**不提交**、返回 `ErrConsumerStoppedOnDB`(`:97-104`);supervisor 关 reader、冷却 60s 后从断点重建
  (`start.go:156-175`,`ResumeAfter` 默认 60s `:38`)。
- 坏消息(解不出 / guid=0 / player=0 / 超 `MaxBlobBytes`=16MiB−1)跳过并提交(`snapshot_consumer.go:21-24`、`:119-139`)。
- 关停:等消费者退出最多 8s(`data_service.go:59-62`、`:130-135`)。
- 对照:transaction_log 攒批 200 条 / 200ms(`txlog_consumer.go:41-57`),落库成功后提交每分区最高 offset(`:188-205`)。
- 指标只有 `data_service_kafka_consumer_up{consumer}` 与 `data_service_kafka_consumer_messages_total{consumer,outcome}`
  (`go/data_service/internal/metrics/metrics.go:92-105`);**没有 lag、没有落库耗时**,仓库里也没有任何针对它们的告警规则
  (现有规则文件只有 `deploy/k8s/owner-epoch-alerts.yaml`、`deploy/k8s/scene-manager-alerts.yaml`)。

### 1.4 落库(go/data_service/internal/store/snapshot_store.go)

- 唯一键形态:`INSERT ... ON DUPLICATE KEY UPDATE id = LAST_INSERT_ID(id)`(`:81-83`、`:119`),
  affected=1 → 新插入,affected=0 → guid 已存在(`:413-437`);依赖 DSN 不开 CLIENT_FOUND_ROWS(`mysql.go:33-36`,
  单测 `TestSnapshotDSNKeepsAffectedRowsSemantics`)。
- 唯一键不在(迁移未跑 / DDL 回滚 / TiDB)→ 退回 `INSERT ... SELECT ... WHERE NOT EXISTS`(`:128-132`),由建 store 时一次探测决定(`:176-229`)。
- 1213 就地重跑最多 3 次(`:67`、`:377-381`、`:470-481`);唯一键形态的 1062 一律当故障(`:401-412`)。
- 同 guid 并发写者只在 rebalance 重叠瞬间出现;「首个插入者回滚 → 两后到者成环」靠有界重试吸收(`:308-351`)。
- 连接:每个 store 一个池,`SnapshotMySQL.MaxOpenConn: 5`(`data_service.yaml:92-98`;`config.go:516-523`);
  GM 快照 / 回档读路径共用这个池。

### 1.5 部署与资源

- data-service 2 副本、每 zone 一套、`limits.memory: 512Mi`(`data-service.yaml:35`、`:105-111`);
  注释已写明「消费并行度的上限是分区数」(`:33`)。
- Kafka PVC 20Gi(`deploy/k8s/manifests/infra/kafka.yaml:363`);每分区 `log.retention.bytes` prod / prod-like 512MiB、dev 128MiB
  (`k8s_deploy.ps1:1168-1205`;注入 `kafka.yaml:276-278`);compose 128MiB(`deploy/docker-compose.yml:38`)。
  审计 topic 只覆盖了 `retention.ms`(`topic_init.go:115-125`、`:189-203`)。

### 1.6 Java 版现状(`D:\luyuan\xuanming-server-mmo`)

- `PARITY.md:70`:Java 快照经 `game-player-snapshot-g<N>`(3 分区、键 = 玩家号)由 game-data 第二条 `ConsumerLoop` 落库;
  生产端超过 `game.scene.snapshot-max-bytes`(缺省 1000000)丢弃并计数。
- `game-data/src/main/java/com/game/data/consume/ConsumerLoop.java:24-37`:一个 topic 一条线程一个 KafkaConsumer,
  **一次 poll 的整批在一个库事务里落库**、`commitSync`;数据错误逐行重试后进毒丸日志跳过;已有 lag 量表(`:303`、`:314-319`)。

---

## 2. 目标 / 非目标

**目标**
1. 消费改为按分区并发:每分区独立 goroutine,分区内顺序与「先落库后提交」语义不变。
2. 批量落库:批大小 / 字节 / 等待上限可配;snapshot_guid 去重语义不变(含批内重复、跨批重放、rebalance 重叠)。
3. 分区扩容只走离线换代,给出可执行 runbook,并修正 `deploy/k8s/README.md:673-674` 的丢数顺序。
4. 低基数的 lag / 保留期丢失 / 落库耗时 / 批大小 / 单条字节指标 + 告警规则。
5. 核对并记录消息大小链路。

**非目标(明确不做)**
- 不改 C++ 生产端(不调 `message.max.bytes`、不加压缩、不加快照大小上限)—— §15 拍板。
- 不改 topic 保留策略(`retention.bytes`)—— §15 拍板。
- 不做「坏行跳过」(Java 的毒丸语义)、不做开服期快照抽样 / 降级 —— §15 拍板。
- 不做双代并行消费(在线换代)—— §15 拍板。
- 不拆审计表到独立库实例 —— §16 列为后续项。
- transaction_log 消费者行为不动(只加落库耗时 / 批大小指标)。

---

## 3. 方案总览

```
             kafka-go group Reader(每实例一个,不变)
                         │ FetchMessage
                    ┌────┴─────┐  dispatcher(Run 所在 goroutine)
                    │ map[partition]*worker,懒创建
          ┌─────────┼──────────┐
       worker p0  worker p1  worker pK      每个 worker:FIFO 通道 → 攒批 → flush
          │          │          │
          └── sem(MaxInflightFlushes,实例级)──┘ → SnapshotStore.InsertSnapshotsIfGuidAbsent(多行 ODKU)
                                                 → 成功后 commit 本分区本批最高 offset(commitMu 串行)
```

- 分区内顺序:同一分区只由一个 worker 处理,通道 FIFO,批内行按 offset 序拼进一条语句,提交只在落库成功后、只提交本分区。
- 任一 worker 停下(DB 重试用尽 / panic)→ 取消整个 Run → 等所有 worker 退出 → 返回错误 → 现有 supervisor 冷却后重建
  (与现状「一条循环停下 = 整个消费者停下」一致)。
- 默认配置(批 1 条、并发 1、不等待)下:`len(rows)==1` 走**原来的单行接口与原来的重试**,每条消息单独提交 —— 与现状**语义等价**,
  现有 5 条单测不改即应通过(§8 U-0)。【评审改】不写「逐字等价」:单分区时处理顺序与现状相同;一个实例拥有多个分区时,
  DB 并发仍是 1(信号量),但各分区的解码 / 提交会与另一分区的落库重叠,停机错误文本多了 `partition=`,`LastError` 是双 `%w` 包装
  (仍可 `errors.Is` 底层错误)。U-0 能通过的前提之一是 `commit` 对空切片不发提交(§4.3,评审 M-2)。

---

## 4. 详细设计

### 4.1 配置键(`go/data_service/internal/config/config.go` 的 `KafkaConfig`,平铺,`json:",optional"`)

go-zero 不下钻一个未出现的结构(`config.go:529-535` 同款坑),所以**零值 = 没配**,默认值由 `Normalize()` 在代码里填;
`Validate` 拒绝而不纠正,照 `RouteStoreConfig.Normalize/Validate`(`config.go:316-378`)的写法。

| 键 | 零值归一为 | 合法范围 | 含义 | 本次 yaml 写 | 建议开服值(待压测) |
|---|---|---|---|---|---|
| `SnapshotBatchMaxRows` | 1 | [1, 256] | 每分区一批最多多少条消息(含被跳过的坏消息);1 = 逐条(现状) | `1` | 16 |
| `SnapshotBatchMaxBytes` | 4194304 | [65536, 33554432] | 一批 data 字节的**软上限**:每加入一条后检查,累计 ≥ 它即落库,所以一批最多超出上限「一条消息」的大小;只有批为空时到达的超大单条才独占一批(评审改:原文「单条比它大时那一批只有它自己」不成立) | `4194304` | 4194304 |
| `SnapshotFlushIntervalMs` | 0 | [0, 5000] | 批非空后最多再等多久;0 = 不等,只把**已排队**的消息拼进来 | `0` | 200 |
| `SnapshotMaxInflightFlushes` | 1 | [1, max(1, 池上限−1)] | 本实例同时落库的分区数上限(信号量);池上限 = `SnapshotMySQL.MaxOpenConn`(≤0 时按 5,同 `mysql.go:71-75`) | `1` | 2 |
| `LagProbeIntervalMs` | 0 | 0 或 [5000, 600000] | 消费组 lag 探针周期;0 = 关 | `30000` | 30000 |

【评审补】`LagProbeIntervalMs` 是本批唯一在 yaml 里**默认打开**的新行为(K8s ConfigMap 镜像 yaml,所有 profile 都开)。
它只读 broker 元数据、不碰消费与落库,且没有它 §4.7 的积压 / 保留期告警全部是哑的,所以刻意打开;这是对「新行为默认关闭」的
有意偏离,登记为 §13 D-7。

新增常量(config 包,文档与 yaml 照抄这一份):
```go
const (
	DefaultSnapshotBatchMaxBytes = 4 << 20
	MaxSnapshotBatchMaxRows      = 256      // 与 store.MaxSnapshotBatchRows 相等,单测钉住
	MinSnapshotBatchMaxBytes     = 64 << 10
	// 远低于 MySQL 8.0 默认 max_allowed_packet 64MiB,也低于 TiDB txn-total-size-limit 100MB;
	// 但**高于** TiKV raft-entry-max-size 默认 8MB(staging 配 32MB)—— 今天 TiDB 上建不出唯一键、批量不生效(退路逐行),
	// 将来 TiDB 形态的去重若让 uniqueGuidKey=true,批字节须 ≤ 4MiB(超了由 §4.3 的逐条兜底接住,不会卡死,只是变慢)。
	MaxSnapshotBatchMaxBytes     = 32 << 20
	MaxSnapshotFlushIntervalMs   = 5000
	MinLagProbeIntervalMs        = 5000
	MaxLagProbeIntervalMs        = 600000
)

// Normalize 只填本批新增的五个键;已有键(TopicGeneration 等)的语义不在这里动。
func (k KafkaConfig) Normalize() KafkaConfig
// ValidateConsumers 校验新增键;snapshotPoolMaxOpen 传 c.SnapshotMySQL.MaxOpenConn。
func (k KafkaConfig) ValidateConsumers(snapshotPoolMaxOpen int) error
```
- 为什么 `MaxInflightFlushes ≤ 池上限−1`:快照池同时服务 GM 快照 / 回档读(§1.4),积压时不能被消费者占满;
  也**不新增任何 MySQL 连接**(连接预算已超,launch_verdict / `tidb-phase1-and-launch-readiness.md` L-08)。
- 装配点:`data_service.go` 紧跟 RouteStore 校验(`data_service.go:94-100`)之后:
  ```go
  c.Kafka = c.Kafka.Normalize()
  if err := c.Kafka.ValidateConsumers(c.SnapshotMySQL.MaxOpenConn); err != nil {
      logx.Errorf("[startup] invalid Kafka consumer config; refusing to start: %v", err)
      fmt.Fprintf(os.Stderr, "data_service refuses to start: invalid Kafka consumer config: %v\n", err)
      logx.Close()
      os.Exit(1)
  }
  ```
  `-migrate` 在它之前返回(`data_service.go:87-89`),不受影响。与 Brokers 是否为空无关(与 RouteStore 同理:一份「开 Kafka 才炸」的配置不该等到开的那一刻才发现)。
- `config.go:466` 的注释「单条消息几百 KB,消费者不攒批」改为指向本节。

### 4.2 store:`InsertSnapshotsIfGuidAbsent`(`go/data_service/internal/store/snapshot_store.go`)

**常量拆分(文本与现状逐字相同,只是拼接):**
```go
// snapshotValuesTupleSQL 一行的九个占位符,顺序 = snapshotInsertArgs。
const snapshotValuesTupleSQL = `(?, ?, ?, ?, ?, ?, ?, ?, ?)`

const insertSnapshotSQL = `INSERT INTO player_snapshot
	 (player_id, zone_id, snapshot_type, created_at, reason, operator, data, snapshot_guid, source)
	 VALUES ` + snapshotValuesTupleSQL

const onDuplicateKeepIDSQL = "\n\t ON DUPLICATE KEY UPDATE `id` = LAST_INSERT_ID(`id`)"

const insertSnapshotOnDuplicateKeepSQL = insertSnapshotSQL + onDuplicateKeepIDSQL

// MaxSnapshotBatchRows 是一条多行语句的行数硬上限。9×256=2304 个占位符,远低于预处理语句 65535 的上限。
const MaxSnapshotBatchRows = 256

// insertSnapshotsOnDuplicateKeepSQL 拼 n(>=1)行的 ODKU;n==1 时与 insertSnapshotOnDuplicateKeepSQL 逐字相同。
func insertSnapshotsOnDuplicateKeepSQL(n int) string {
	var b strings.Builder
	b.Grow(len(insertSnapshotSQL) + (n-1)*(len(snapshotValuesTupleSQL)+4) + len(onDuplicateKeepIDSQL))
	b.WriteString(insertSnapshotSQL)
	for i := 1; i < n; i++ {
		b.WriteString(",\n\t ")
		b.WriteString(snapshotValuesTupleSQL)
	}
	b.WriteString(onDuplicateKeepIDSQL)
	return b.String()
}
```
`TestInsertSnapshotOnDuplicateKeepSQLShape`(`snapshot_store_test.go:348-371`)断言 ODKU 以 `insertSnapshotSQL` 开头、占位符数相等,拆分后仍成立。
需要在文件 import 里加 `strings`。

**方法:**
```go
// InsertSnapshotsIfGuidAbsent 按 snapshot_guid 去重批量落库,返回本次真正新插入的行数。
//
// 语义与逐行调用 InsertSnapshotIfGuidAbsent 完全相同,只是一次往返、一次提交:
//   - 唯一键形态:一条多行 ODKU。每行独立判重:撞 uk_snapshot_guid_nz(库里已有,或同一语句里前面某行刚插)
//     的行 affected 记 0、一列不改;新行记 1。所以 RowsAffected = 新插入行数。同 guid 在批内出现两次时,
//     先出现(offset 小)的那条落库 —— 与逐条消费「先到者赢」一致。
//   - 退路形态(唯一键不在):逐行调 InsertSnapshotIfGuidAbsent。不把 NOT EXISTS 拼成多行:它的去重靠 RR 加锁读,
//     拼成一条会把间隙锁范围扩到整批,成环更多;退路本来只在迁移未跑 / 回滚窗口出现,不值得为它做批量。
//   - 不回带每行 id:消费者只拿 id 打 duplicate 日志;批量路径改为每批一条汇总日志(§13 偏离 D-2)。
//     也因此没有单行路径「判定已存在 → 点查 id → 查不到」那个并发删除窗口(:440-460)。
//
// 原子性:单条自动提交语句,要么整批落、要么整批不落(1213 被选为牺牲者时整条回滚),就地重跑安全。
// 失败一律返回 error、不返回部分成功(退路形态逐行时例外:已落的行留在库里,重放时被判重,结果不变)。
func (s *SnapshotStore) InsertSnapshotsIfGuidAbsent(ctx context.Context, rows []*SnapshotRow) (int64, error) {
	if len(rows) == 0 {
		return 0, nil
	}
	if len(rows) > MaxSnapshotBatchRows {
		return 0, fmt.Errorf("snapshot batch of %d rows exceeds MaxSnapshotBatchRows=%d", len(rows), MaxSnapshotBatchRows)
	}
	for _, r := range rows {
		if r.SnapshotGuid == 0 {
			return 0, fmt.Errorf("snapshot_guid must be non-zero for check-then-insert")
		}
	}
	if !s.uniqueGuidKey {
		var inserted int64
		for _, r := range rows {
			_, ok, err := s.InsertSnapshotIfGuidAbsent(ctx, r)
			if err != nil {
				return inserted, err
			}
			if ok {
				inserted++
			}
		}
		return inserted, nil
	}

	query := insertSnapshotsOnDuplicateKeepSQL(len(rows))
	args := make([]any, 0, len(rows)*9)
	for _, r := range rows {
		args = append(args, snapshotInsertArgs(r)...)
	}
	var res sql.Result
	reruns, err := retryOnDeadlock(ctx, snapshotGuidInsertAttempts, snapshotGuidDeadlockBackoff, func() error {
		var execErr error
		res, execErr = s.db.ExecContext(ctx, query, args...)
		return execErr
	})
	if reruns > 0 {
		s.deadlockRetries.Add(uint64(reruns))
		logx.Infof("[SnapshotStore] snapshot batch of %d (first guid=%d) 在唯一键路径上因 1213 就地重跑了 %d 次,结果 err=%v;"+
			"偶发属 rebalance 重叠,持续高频查消费组是否反复 rebalance(同 InsertSnapshotIfGuidAbsent 的说明)",
			len(rows), rows[0].SnapshotGuid, reruns, err)
	}
	if err != nil {
		// 1062 在唯一键形态下不是去重结果(ODKU 已就地吸收 uk_snapshot_guid_nz),说明撞了别的唯一约束:按故障上报。
		return 0, fmt.Errorf("insert snapshot batch of %d (first guid %d): %w", len(rows), rows[0].SnapshotGuid, err)
	}
	affected, err := res.RowsAffected()
	if err != nil {
		return 0, fmt.Errorf("insert snapshot batch of %d rows affected: %w", len(rows), err)
	}
	if affected < 0 || affected > int64(len(rows)) {
		// ODKU 的 affected 只可能是 Σ{1 新插入, 0 原值不变};出现 2(真改了已有行)或超出行数,
		// 说明 CLIENT_FOUND_ROWS 被打开或语句被改坏 —— 拿它当插入数会把重放算成新插入。fail-closed,不让消费者提交。
		return 0, fmt.Errorf("insert snapshot batch of %d: rows affected %d outside [0, %d]", len(rows), affected, len(rows))
	}
	return affected, nil
}
```
- 为什么不按 guid 排序再插:常态下不同 guid 只取插入意向锁、彼此相容(`:298-306`),排序不减少任何锁冲突;
  rebalance 重叠时两个实例的批都按同一分区的 offset 序拼,天然同序。保持 offset 序还让 `id` 顺序 = 同一玩家的 offset 顺序,
  `ListSceneSnapshotsByPlayer` 的 `ORDER BY created_at DESC, id DESC`(`:697`)在同秒并列时的次序与逐条消费一致。
- 包大小:go-sql-driver 默认 `maxAllowedPacket` 64MiB(DSN 未覆盖,`mysql.go:33-36`),服务端未配 `max_allowed_packet`
  (`mysql.yaml` 没有这一项,MySQL 8.0 默认 64MiB)。批字节软上限 4MiB + 最多一条超出(实际 ≤ 约 1MB,§11)远低于它。
  任何「批太大」类错误(1153 / 驱动 ErrPktTooLarge / TiDB 8004 事务过大)都会触发消费者的逐条兜底(§4.3),不会卡死。
- 【待 Codex 在目标库核对】InnoDB 对单事务 / 单行外部存储 BLOB 有「≤ 10% redo 容量」的限制(MySQL 8.0.30+ 默认
  `innodb_redo_log_capacity`=100MB → 10MB,`mysql.yaml` 未配)。批字节上限 4MiB 在它之下;若改大 `SnapshotBatchMaxBytes`,不要超过 10% redo 容量。

### 4.3 消费者(`go/data_service/internal/kafka/snapshot_consumer.go` + 新文件 `snapshot_worker.go`)

**接口与配置:**
```go
// SnapshotSink 是消费者需要的落库能力,由 *store.SnapshotStore 实现。
type SnapshotSink interface {
	InsertSnapshotIfGuidAbsent(ctx context.Context, row *store.SnapshotRow) (id uint64, inserted bool, err error)
	// InsertSnapshotsIfGuidAbsent 批量去重落库,返回新插入行数;批内与库内重复都不计。
	InsertSnapshotsIfGuidAbsent(ctx context.Context, rows []*store.SnapshotRow) (int64, error)
}

type SnapshotConsumerConfig struct {
	DBMaxAttempts  int           // 默认 5
	DBRetryBackoff time.Duration // 默认 1s
	FetchBackoff   time.Duration // 默认 1s
	MaxBlobBytes   int           // 默认 MaxSnapshotBlobBytes

	// 以下四项零值 = 2026-10-12 之前的逐条串行行为(装配见 data_service.go,取值见 config.KafkaConfig)。
	BatchMaxRows       int           // <=1:逐条
	BatchMaxBytes      int           // <=0:4 MiB(软上限)
	FlushInterval      time.Duration // <=0:不等待,只拼已排队的
	MaxInflightFlushes int           // <=0:1
}

type SnapshotConsumer struct {
	cfg      SnapshotConsumerConfig
	reader   MessageReader
	sink     SnapshotSink
	health   *Health
	sem      chan struct{} // 容量 MaxInflightFlushes:实例级落库并发上限
	commitMu sync.Mutex    // 串行化 CommitMessages:不依赖 kafka-go Reader 并发提交的未写明承诺
}
```
`NewSnapshotConsumer` 在 `withDefaults` 之后建 `sem: make(chan struct{}, cfg.MaxInflightFlushes)`。

【评审 M-1,会编译失败】`data_service.go:362` 把 `svcCtx.SnapshotStore` 传给 `dskafka.Start` 的 `snapSink SnapshotSink` 参数,
而 `svcCtx.SnapshotStore` 的**静态类型**是接口 `svc.SnapshotStore`(`internal/svc/servicecontext.go:23-37`),不是 `*store.SnapshotStore`。
`SnapshotSink` 加了批量方法而 `svc.SnapshotStore` 没加,这一行就编译不过。必须同批:
- `internal/svc/servicecontext.go` 的 `SnapshotStore` 接口紧跟 `InsertSnapshotIfGuidAbsent` 加
  `InsertSnapshotsIfGuidAbsent(context.Context, []*store.SnapshotRow) (int64, error)`(注释:player_snapshot_topic 消费者的批量入口);
  该文件工作区已有他人改动,只追加这一行与注释,不整理别处。
- `internal/logic/rollback_recall_test.go` 的 `fakeSnapshotStore`(`:31`)实现了 `svc.SnapshotStore`,接口加方法后测试包编译不过,
  补一个桩:`return 0, errors.New("not used by rollback tests")`(回档用例不走这条路;返回错误而不是 (0,nil),免得将来误用时静默)。
- 装配处不加类型断言(`*store.SnapshotStore` 本来就满足两个接口),保持静态类型检查。

`withDefaults` 里 `BatchMaxRows` 还要**上限钳到 `store.MaxSnapshotBatchRows`**(kafka 包已 import store):yaml 路径有 `ValidateConsumers` 拒绝 >256,
但直接构造 `SnapshotConsumerConfig` 的调用方(单测、将来的工具)传 300 时,store 每批都会拒绝 → 每批都走逐条兜底 → 静默变慢且只有 info 告警。
钳住时打一条 Error 日志说明被钳。

**worker 与批:**
```go
// snapshotBatch 是一个分区正在攒的一批。msgs 是要提交的全部消息(含被跳过的坏消息),
// rows[i] 来自 msgs[rowMsgIdx[i]] —— 逐条兜底时据此只提交失败行之前的前缀。
type snapshotBatch struct {
	msgs      []kafkago.Message
	rows      []*store.SnapshotRow
	rowMsgIdx []int
	bytes     int
}

type snapshotPartitionWorker struct {
	c         *SnapshotConsumer
	partition int
	in        chan kafkago.Message // 容量 = BatchMaxRows;FIFO 是分区内顺序的唯一来源
}
```

**Run(dispatcher):**
```go
// Run 阻塞消费直到 ctx 结束(返回 nil)或任一分区停下(返回 errors.Is(err, ErrConsumerStoppedOnDB)
// 或 errSnapshotWorkerAborted)。返回前一定等所有分区 worker 退出 —— supervisor 随后才 Close reader,
// 不会出现「reader 已关、worker 还在提交」。
func (c *SnapshotConsumer) Run(ctx context.Context) error {
	c.health.markRunning()
	defer c.health.markStopped()

	runCtx, cancelRun := context.WithCancel(ctx)
	defer cancelRun()
	stop := &runStop{cancel: cancelRun}
	var wg sync.WaitGroup
	workers := make(map[int]*snapshotPartitionWorker)

	for runCtx.Err() == nil {
		msg, err := c.reader.FetchMessage(runCtx)
		if err != nil {
			if runCtx.Err() != nil {
				break
			}
			logx.Errorf("[%s] fetch failed, retry in %s: %v", ConsumerSnapshot, c.cfg.FetchBackoff, err)
			sleepCtx(runCtx, c.cfg.FetchBackoff)
			continue
		}
		w := workers[msg.Partition]
		if w == nil {
			w = c.startWorker(runCtx, msg.Partition, &wg, stop)
			workers[msg.Partition] = w
		}
		select {
		case w.in <- msg:
		case <-runCtx.Done(): // 某个分区已停:这条不投递、不提交,重建后重放
		}
	}
	cancelRun()
	wg.Wait()

	if err := stop.firstErr(); err != nil && ctx.Err() == nil {
		c.health.markFailed(err)
		return err
	}
	logx.Infof("[%s] consumer exiting on shutdown", ConsumerSnapshot)
	return nil
}

// runStop 记第一次失败并取消整个 Run。err 在 wg.Wait 之后读:worker 的 fail 发生在它的 wg.Done 之前。
type runStop struct {
	once   sync.Once
	err    error
	cancel context.CancelFunc
}
func (s *runStop) fail(err error) { s.once.Do(func() { s.err = err }); s.cancel() }
func (s *runStop) firstErr() error { return s.err }

var errSnapshotWorkerAborted = errors.New("player_snapshot partition worker aborted (panic); offsets of its in-flight batch NOT committed")

func (c *SnapshotConsumer) startWorker(runCtx context.Context, partition int, wg *sync.WaitGroup, stop *runStop) *snapshotPartitionWorker {
	w := &snapshotPartitionWorker{c: c, partition: partition, in: make(chan kafkago.Message, c.cfg.BatchMaxRows)}
	wg.Add(1)
	// 点位名是常量,不拼分区号(safego 的 point label 规则)。
	safego.Go("data_service.kafka.player_snapshot.partition", func() {
		defer wg.Done()
		completed := false
		defer func() {
			// panic 展开时也会跑到这里(早于 safego 的 recover):不让一个死掉的分区在其它分区继续跑时静默停摆。
			if !completed {
				stop.fail(errSnapshotWorkerAborted)
			}
		}()
		err := w.run(runCtx)
		completed = true
		if err != nil {
			stop.fail(err)
		}
	})
	return w
}
```
注意:现有 `TestSnapshotConsumer_StopsWithoutCommitWhenDBKeepsFailing`(`snapshot_consumer_test.go:114-132`)断言
`errors.Is(err, ErrConsumerStoppedOnDB)` **且** `ErrorIs(LastError(), errDBDown)`。所以 worker 返回的停机错误一律写成
`fmt.Errorf("%w: %w", ErrConsumerStoppedOnDB, cause)`(双 `%w`,Go ≥ 1.20;data_service 是 go 1.26.5,`go.mod:3`),
`markFailed` 收到的就是它,两条 `errors.Is` 都成立。

**worker 循环:**
```go
// run 处理一个分区直到 ctx 结束(返回 nil)或停机(返回带 ErrConsumerStoppedOnDB 的错误)。
func (w *snapshotPartitionWorker) run(ctx context.Context) error {
	cfg := w.c.cfg
	b := &snapshotBatch{}
	for {
		select { // 阻塞等这一批的第一条
		case <-ctx.Done():
			return nil
		case msg := <-w.in:
			w.add(b, msg)
		}
		deadline := time.Now().Add(cfg.FlushInterval) // 单调时钟
	fill:
		for !w.full(b) {
			select { // 先无等待地拿已排队的(FlushInterval=0 时的「自然攒批」)
			case msg := <-w.in:
				w.add(b, msg)
				continue
			default:
			}
			wait := time.Until(deadline)
			if wait <= 0 {
				break
			}
			timer := time.NewTimer(wait)
			select {
			case msg := <-w.in:
				timer.Stop()
				w.add(b, msg)
			case <-timer.C:
				break fill
			case <-ctx.Done():
				timer.Stop()
				return nil // 手里这批不落不提交,重建 / 重启后重放(与现状关停时丢弃在途一条同口径)
			}
		}
		if err := w.flush(ctx, b); err != nil {
			return err
		}
		// 先 clear 再截断:只截断时底层数组仍引用上一批的行(含 Data,最大约 1MB/条),空闲分区会一直钉住它们。
		clear(b.msgs)
		clear(b.rows)
		*b = snapshotBatch{msgs: b.msgs[:0], rows: b.rows[:0], rowMsgIdx: b.rowMsgIdx[:0]}
	}
}

func (w *snapshotPartitionWorker) full(b *snapshotBatch) bool {
	return len(b.msgs) >= w.c.cfg.BatchMaxRows || b.bytes >= w.c.cfg.BatchMaxBytes
}

func (w *snapshotPartitionWorker) add(b *snapshotBatch, msg kafkago.Message) {
	metrics.ObserveKafkaConsumerMessageBytes(ConsumerSnapshot, len(msg.Value)) // 含坏消息:单条大小分布是 §11 要的数据
	row, ok := w.c.decode(msg) // decode 原样复用(坏消息在里面记日志 + outcome 计数);它已把 Value 拷进 row.Data
	// 批里留着的 msgs 只用于提交与日志:kafka-go 提交只读 Topic / Partition / Offset。
	// 不剥掉 Value,每条快照在批里就有两份(msg.Value + row.Data),内存翻倍(R-3)。
	msg.Value, msg.Key, msg.Headers = nil, nil, nil
	b.msgs = append(b.msgs, msg)
	if ok {
		b.rows = append(b.rows, row)
		b.rowMsgIdx = append(b.rowMsgIdx, len(b.msgs)-1)
		b.bytes += len(row.Data)
	}
}
```
【评审 I-1】`Topic` 必须保留(kafka-go 按 Topic+Partition 提交);U-14 钉住「批里的 msgs 不带 Value、但带 Topic」。

**flush 与逐条兜底:**
```go
// flush 落库 → 成功后才提交本批最高 offset。
//   - 0 行(全是坏消息):只提交。
//   - 1 行:走原单行接口 + 原重试(现状逐字等价,含 "already stored as id=%d" 日志)。
//   - ≥2 行:批量接口 + 同样的有界重试;用尽后逐条兜底(isolate)。
//   - 失败时 ctx 已取消(关停 / 兄弟分区停机):不提交、不计 db_error、不算停机。
func (w *snapshotPartitionWorker) flush(ctx context.Context, b *snapshotBatch) error {
	c := w.c
	switch len(b.rows) {
	case 0:
	case 1:
		metrics.ObserveKafkaConsumerBatchRows(ConsumerSnapshot, 1)
		// 改为每次尝试前 acquire、尝试后 release;每次尝试内 observe flush_seconds(评审 I-3)
		id, inserted, err := c.insertWithRetry(ctx, b.rows[0])
		if err != nil {
			if ctx.Err() != nil {
				return nil
			}
			metrics.ObserveKafkaConsumerMessages(ConsumerSnapshot, "db_error", 1)
			logx.Errorf("[%s] STOPPED: partition=%d snapshot guid=%d player=%d could not be inserted after %d attempts, offset %d NOT committed (lag is preferred over loss): %v",
				ConsumerSnapshot, w.partition, b.rows[0].SnapshotGuid, b.rows[0].PlayerID, c.cfg.DBMaxAttempts, b.msgs[b.rowMsgIdx[0]].Offset, err)
			c.commit(ctx, b.msgs[:b.rowMsgIdx[0]]) // 夹在它前面的坏消息照常提交(现状也是逐条先提交了它们)
			return fmt.Errorf("%w: %w", ErrConsumerStoppedOnDB, err)
		}
		observeOne(id, inserted, b.rows[0]) // 原 inserted / duplicate 计数与日志
	default:
		metrics.ObserveKafkaConsumerBatchRows(ConsumerSnapshot, len(b.rows))
		inserted, err := c.insertBatchWithRetry(ctx, b.rows) // 每次尝试内 observe flush_seconds
		if err != nil {
			if ctx.Err() != nil {
				return nil
			}
			return w.isolate(ctx, b, err)
		}
		dup := len(b.rows) - int(inserted)
		metrics.ObserveKafkaConsumerMessages(ConsumerSnapshot, "inserted", int(inserted))
		metrics.ObserveKafkaConsumerMessages(ConsumerSnapshot, "duplicate", dup)
		if dup > 0 {
			logx.Infof("[%s] partition=%d batch of %d: %d duplicate snapshot(s) already stored, skipped (offsets %d..%d)",
				ConsumerSnapshot, w.partition, len(b.rows), dup, b.msgs[0].Offset, b.msgs[len(b.msgs)-1].Offset)
		}
	}
	c.commit(ctx, b.msgs)
	return nil
}

// isolate 是批量失败后的逐条兜底。它让批量**永远不会**比逐条更容易卡住:
//   - 批本身的问题(包过大、事务过大、某种只在多行语句上出现的错误):逐条全部成功 → 整批提交,继续消费;
//   - 真有一行插不进去(毒丸行 / DB 真挂了):停在那一行,只提交它之前的前缀 ——
//     与现状「逐条落,落到它为止,之前的都已提交」的结局逐字相同,并且日志能点名是哪个 guid。
// 每行走与单行路径**同一个** insertWithRetry(DBMaxAttempts 次有界重试)。【评审改】原稿每行只试一次:
// 批级失败若是「批本身的问题」,逐条阶段某行再撞一次瞬时 1205 / 断连就会停机 60s,比现状逐条消费更容易停;
// 用同一套重试,单行的容错就与现状逐字相同。代价:DB 真挂时多耗 DBMaxAttempts×DBRetryBackoff(默认约 4s)才停。
func (w *snapshotPartitionWorker) isolate(ctx context.Context, b *snapshotBatch, batchErr error) error {
	c := w.c
	logx.Errorf("[%s] partition=%d batch of %d rows failed after %d attempts, retrying row by row to isolate: %v",
		ConsumerSnapshot, w.partition, len(b.rows), c.cfg.DBMaxAttempts, batchErr)
	for i, row := range b.rows {
		id, inserted, err := c.insertWithRetry(ctx, row) // 每次尝试 acquire → sink.InsertSnapshotIfGuidAbsent → release
		if err != nil {
			c.commit(ctx, b.msgs[:b.rowMsgIdx[i]])
			if ctx.Err() != nil {
				return nil
			}
			metrics.ObserveKafkaConsumerMessages(ConsumerSnapshot, "db_error", 1)
			metrics.ObserveKafkaConsumerIsolation(ConsumerSnapshot, "stopped")
			logx.Errorf("[%s] STOPPED: partition=%d snapshot guid=%d player=%d could not be inserted (batch retried %d times, then row by row), offset %d NOT committed (lag is preferred over loss): %v",
				ConsumerSnapshot, w.partition, row.SnapshotGuid, row.PlayerID, c.cfg.DBMaxAttempts, b.msgs[b.rowMsgIdx[i]].Offset, err)
			return fmt.Errorf("%w: %w", ErrConsumerStoppedOnDB, err)
		}
		observeOne(id, inserted, row)
	}
	metrics.ObserveKafkaConsumerIsolation(ConsumerSnapshot, "recovered")
	c.commit(ctx, b.msgs)
	return nil
}
```
- `insertBatchWithRetry`:照 `TxLogConsumer.insertWithRetry`(`txlog_consumer.go:207-222`)的形状,每次尝试前 `acquire(ctx)`、
  尝试后 `release()`(`release` 用 defer 包在单次尝试的闭包里:sink panic 时也归还名额);退避期间**不占**信号量。
  `acquire` 因 ctx 取消失败时直接返回、不打 "attempt n failed" 日志。
- 【评审 I-15,建议】批级错误若是确定性的「批本身」问题(MySQL 1153 包过大 / 驱动 `mysql.ErrPktTooLarge` / TiDB 8004 事务过大),
  不必按 DBMaxAttempts×1s 退避重试,直接进 `isolate`;否则每个这样的批都要白等约 4s。只认这几个错误号,其余照常重试。
- `commit(ctx, msgs []kafkago.Message)`:签名从单条改为切片,内部 `highestPerPartition`(`consumer.go:106-121`)+ `commitMu`(defer 解锁);
  失败只记日志(现状 `snapshot_consumer.go:176-181`)。
  **【评审 M-2,必须】`highestPerPartition` 结果为空时直接返回、不调 `CommitMessages`**(照 `TxLogConsumer.commit` `txlog_consumer.go:226-229`)。
  flush / isolate 在「第一行就失败」时会调 `commit(ctx, b.msgs[:0])`;不加这条守卫,fakeReader 会把这次空提交记成一次 commit 调用,
  现有 `TestSnapshotConsumer_StopsWithoutCommitWhenDBKeepsFailing` 的 `assert.Empty(t, reader.commitCalls())`(`snapshot_consumer_test.go:129`)
  当场变红 —— U-0「一字不改全部通过」就不成立;真 kafka-go 也会白发一次空的提交请求。
- `flush_seconds` 的计时口径(评审 I-3):只计**单次 DB 尝试**的耗时(acquire 之后到 sink 返回),每次尝试 observe 一次;
  不含信号量等待、不含退避。原稿把 `start` 放在 `insertWithRetry` 之前,于是 p99 混进了本地信号量排队与 1s 退避,
  §4.7 的 `FlushSlow`「全局库开始排队」就会被本实例自己的并发上限或一次重试触发,指向错误的方向。
  `batch_rows` 仍按「一次 flush」observe 一次。ctx 已取消导致的失败不 observe(关停 / 兄弟停机不是 DB 慢)。
- `decode` 不改(`snapshot_consumer.go:119-156`)。
- 包注释(`consumer.go:1-15`)里「player_snapshot_topic → 逐条、按 snapshot_guid 先查后插」改为「按分区并发、可配批量(默认逐条)」。

### 4.4 Start / supervise / main 装配

- `StartConfig`(`start.go:17-36`)加:
  ```go
  Snapshot         SnapshotConsumerConfig // 批量 / 并发参数;零值 = 逐条串行
  LagProbeInterval time.Duration          // 0 = 不探 lag
  ```
- `start.go:137`:`NewSnapshotConsumer(cfg.Snapshot, reader, snapSink)`。
- `LagProbeInterval > 0` 时在 `Start` 里多起一条 goroutine,见 §4.5。【评审 I-2 改】**不进 `consumers.wg`**:
  `Consumers.Wait` 的存在理由是「先等消费者、再关连接池」(`start.go` 的 `Wait` 注释),探针不碰任何 store;而 sarama 的 admin 调用
  不收 ctx,broker 半死时一次 `Probe` 能挂满 sarama 的拨号 / 读超时(默认各 30s),进了 wg 就会让每次关停都超过 8s 的
  `kafkaShutdownGrace`,打出误导性的「consumers did not stop within 8s」。探针循环自己在 ctx 结束时退出并 `Close`;
  另见 §4.5 对 sarama 超时的收紧。
- 【评审 I-2 补】`newReader`(`consumer.go:125-138`)给 kafka-go reader 配 `ErrorLogger`(转 `logx.Errorf`,前缀 `[kafka-go]`)。
  kafka-go 在已提交位点早于日志起点时会**自动跳到起点**并经 ErrorLogger 打出「skipping from offset X to Y (N messages)」——
  这是保留期删掉未消费消息的**唯一精确记录**;现在没配 ErrorLogger,这一跳完全静默,§4.5 的 `retention_gap` 也会在消费者跳过、
  提交之后自行归零。两条消费者共用 `newReader`,这只加日志、不改消费行为。
  (措辞与「组模式下是否同样走这条分支」按 kafka-go v0.4.47 的 `reader.go` 核对;本机没有模块源码,属待核对项,见 R-1。
  若核对结果是组模式不经 ErrorLogger,就退回「worker 发现同分区 offset 不连续时记 Error + 计数」,审计 topic 无事务标记,
  正常情况下同一会话内 offset 连续。)
- `supervise`(`start.go:156-175`)不改:它不看错误类型,任何非 nil 都「关 reader → 冷却 → 重建」。
- `data_service.go:353-362` 的 `StartConfig` 字面量补:
  ```go
  Snapshot: dskafka.SnapshotConsumerConfig{
      BatchMaxRows:       kc.SnapshotBatchMaxRows,
      BatchMaxBytes:      kc.SnapshotBatchMaxBytes,
      FlushInterval:      time.Duration(kc.SnapshotFlushIntervalMs) * time.Millisecond,
      MaxInflightFlushes: kc.SnapshotMaxInflightFlushes,
  },
  LagProbeInterval: time.Duration(kc.LagProbeIntervalMs) * time.Millisecond,
  ```
- `data_service.go:342-343` 预注册处加 `metrics.PrimeKafkaConsumer(dskafka.ConsumerTxLog)`、`metrics.PrimeKafkaConsumer(dskafka.ConsumerSnapshot)`。
- 启动日志(`start.go:143-145`)补打 snapshot 的 batch_rows / batch_bytes / flush_interval / inflight 与 lag_probe 周期。
- transaction_log:`txlog_consumer.go:188-205` 的 `flush` 只在 `len(batch.rows) > 0` 时 `ObserveKafkaConsumerBatchRows(ConsumerTxLog, len(rows))`,
  `insertWithRetry`(`:207-222`)每次尝试内 `ObserveKafkaConsumerFlushAttempt(ConsumerTxLog, result, dur)`;行为不动。
  (评审 I-10:全是坏消息的批 rows=0 也会走 flush,不加条件会往 `batch_rows` 塞 0,拉低均值。)

### 4.5 lag 探针

**为什么不用 kafka-go 的 `Reader.Stats().Lag`、也不按消息的 HighWaterMark 算:** 组模式下前者只反映最后读过的分区;
后者是各实例各自看到的局部值,rebalance 后旧主人的数留成陈值。要的是 broker 上「这个组在这个 topic 上还差多少」,
与哪个实例拿着哪个分区无关 —— 每个实例报同一个数,告警取 `max`。

**`go/shared/kafkautil/group_lag.go`(新,shared 模块已直接依赖 sarama,`go/shared/go.mod:6`):**
```go
// GroupLag 是一个消费组在一个 topic 上的积压(全部分区求和;不按分区出指标,基数恒定)。
type GroupLag struct {
	// Lag = Σ max(0, end − max(committed, start)),end = 下一条要写的 offset(OffsetNewest),start = 日志起点(OffsetOldest)。
	// 没提交过的分区按「从 start 开始」算(消费者 StartOffset=FirstOffset,consumer.go:131)。
	Lag int64
	// RetentionGap = Σ max(0, start − committed),只算 committed >= 0 的分区:
	// 保留期(retention.ms 或 retention.bytes)已经删掉、而这个组还没消费到的消息条数 —— 审计数据已经丢了。
	RetentionGap int64
	// Uncommitted 是还没提交过 offset 的分区数(新组 / 新一代 topic 刚建)。
	Uncommitted int
}

// groupOffsetReader 只暴露算 lag 要的两个调用,单测注入 fake,不连真实 Kafka(同 topicAdmin 的做法,topic_init.go:65-70)。
type groupOffsetReader interface {
	GetOffset(topic string, partition int32, at int64) (int64, error)
	ListConsumerGroupOffsets(group string, topicPartitions map[string][]int32) (*sarama.OffsetFetchResponse, error)
}

func computeGroupLag(r groupOffsetReader, group, topic string, partitions int32) (GroupLag, error)
// 规则:partitions<=0 → error;OffsetFetch 整体失败、响应顶层 Err(v2+)!= ErrNoError、或某分区 block 为 nil / Err!=ErrNoError → error;
// committed > end(topic 被删重建过)→ 该分区 lag 记 0、返回 error 让调用方看见(不静默当 0)。
// 【评审 I-2,读取顺序是正确性要求】先 ListConsumerGroupOffsets 读 committed,**再**逐分区读 end / start。
//   反过来(先 end 后 committed)时,消费者在两次读之间提交一次就会出现 committed > end,繁忙 topic 上探针会周期性误报 error;
//   先读 committed 则 end 单调不减,committed <= end 恒成立(除非 topic 真被重建)。
//   代价:start 也是后读的,保留期恰好在两次读之间推进、而消费者其实已越过它时,会算出一个**瞬时**的 RetentionGap>0 假阳性
//   —— 所以 §4.7 的 RetentionGap 告警带 `for: 2m`(跨 ≥4 次探测仍为正才响),确证看 kafka-go ErrorLogger 的「skipping from offset」日志(§4.4)。

// GroupLagProber 持有一对长连接的 sarama Client + ClusterAdmin(NewClusterAdminFromClient;admin.Close 会一并关 client)。
// 它自己实现 groupOffsetReader(GetOffset 转 client、ListConsumerGroupOffsets 转 admin),Probe = computeGroupLag(p, ...)。
type GroupLagProber struct{ client sarama.Client; admin sarama.ClusterAdmin }
// cfg.Version = sarama.V3_0_0_0,与 EnsureTopics 一致(topic_init.go:81-82)。
// 【评审 I-2】收紧超时:Net.DialTimeout / ReadTimeout / WriteTimeout = 5s,Metadata.Retry.Max = 1。
// sarama 调用不收 ctx,默认 30s 的超时会让一轮探测在 broker 半死时挂半分钟以上、并拖慢关停。
func NewGroupLagProber(brokers []string) (*GroupLagProber, error)
func (p *GroupLagProber) Probe(group, topic string, partitions int32) (GroupLag, error)
func (p *GroupLagProber) Close() error
```
`GroupLag.Uncommitted` 不出指标(YAGNI),只在 >0 时随探针 Info 日志打出(新一代 topic / 新组刚起时可见)。

**`go/data_service/internal/kafka/lag_probe.go`(新):**
```go
type lagTarget struct{ consumer, group, topic string; partitions int32 }

// runLagProbe 每 interval 探一次两条消费者的组 lag,直到 ctx 结束。只读、不碰消费与落库:
// 探不到只记 lag_probe_total{result="error"} 并在下一轮重建连接,gauge 保留上一次的值(由「探针失明」告警兜)。
// 每一轮用 safego.Run 包住,一轮 panic 不杀循环。
func runLagProbe(ctx context.Context, brokers []string, interval time.Duration, targets []lagTarget)
```
- 每轮:prober 为 nil 时 `NewGroupLagProber`;对每个 target `Probe` → 成功 `metrics.SetKafkaConsumerLag(consumer, lag.Lag, lag.RetentionGap)` +
  `ObserveKafkaConsumerLagProbe(consumer, "ok")`;`RetentionGap > 0` 时打 Error(审计数据已被保留期删除,点名 topic / group / 条数);
  失败 `ObserveKafkaConsumerLagProbe(consumer, "error")` + Error 日志 + `Close` 并置 nil。
- 退出时 `Close`。点位名常量:`data_service.kafka.lag_probe`。
- targets 由 `Start` 用 `cfg.TxLogGroup/TxLogTopic/TxLogPartitions` 与 `cfg.SnapshotGroup/SnapshotTopic/SnapshotPartitions` 组装。

### 4.6 指标(`go/data_service/internal/metrics/metrics.go`;Subsystem `data_service`;label 只有 consumer / result / outcome)

| 名称 | 类型 | label | 说明 |
|---|---|---|---|
| `data_service_kafka_consumer_flush_seconds` | Histogram | consumer, result(ok\|error) | **单次 DB 尝试**耗时(拿到信号量之后到 sink 返回;不含信号量等待、不含退避;ctx 取消的不计)。评审 I-3 改;桶 0.005,0.01,0.025,0.05,0.1,0.25,0.5,1,2.5,5,10,30 |
| `data_service_kafka_consumer_batch_rows` | Histogram | consumer | 每次 flush 的有效行数(rows=0 的纯坏消息批不计);桶 1,2,4,8,16,32,64,128,256 |
| `data_service_kafka_consumer_message_bytes` | Histogram | consumer | 每条消息 value 字节;桶 1KiB,16KiB,64KiB,128KiB,256KiB,512KiB,768KiB,1000000,2MiB,4MiB,8MiB,16MiB(1000000 那一档对着生产端上限,§11) |
| `data_service_kafka_consumer_lag_messages` | Gauge | consumer | §4.5 的 Lag |
| `data_service_kafka_consumer_retention_gap_messages` | Gauge | consumer | §4.5 的 RetentionGap;> 0 = 审计数据已丢 |
| `data_service_kafka_consumer_lag_probe_total` | Counter | consumer, result(ok\|error) | 探针结局 |
| `data_service_kafka_consumer_isolation_total` | Counter | consumer, result(recovered\|stopped) | 批量失败后的逐条兜底结局 |

新函数:`ObserveKafkaConsumerFlushAttempt(consumer, result string, d time.Duration)`、`ObserveKafkaConsumerBatchRows(consumer string, rows int)`、
`ObserveKafkaConsumerMessageBytes(consumer string, n int)`、
`SetKafkaConsumerLag(consumer string, lag, gap int64)`、`ObserveKafkaConsumerLagProbe(consumer, result string)`、
`ObserveKafkaConsumerIsolation(consumer, result string)`、`PrimeKafkaConsumer(consumer string)`(共七个;评审 I-3 把原 `ObserveKafkaConsumerFlush` 拆成前两个,
因为两者的观测粒度不同:前者每次尝试一次,后者每次 flush 一次)。
`PrimeKafkaConsumer` 把 `messages_total` 的六个 outcome(inserted / duplicate / decode_error / invalid / oversize / db_error)、
`lag_probe_total` 的 ok / error、`isolation_total` 的 recovered / stopped 逐个 `Add(0)` —— 理由同 `PrimeRollbackGuildCheck`:
一辈子可能只发生几次的结局,序列不预先存在时 `increase(...) > 0` 恰好漏掉第一次。
全部加进 `register()`(`metrics.go:310-325`)。`messages_total` 的 Help(`:104`)不变。

### 4.7 告警规则(新文件 `deploy/k8s/data-service-audit-alerts.yaml`,PrometheusRule,格式照 `deploy/k8s/owner-epoch-alerts.yaml:1-40`)

```yaml
spec:
  groups:
    - name: data-service.audit-pipeline
      interval: 30s
      rules:
        # 消费循环停了(DB 故障停机 / 从未起来)。用 10 分钟均值而不是 `== 0 for`:
        # DB 持续故障时 up 每 ResumeAfter(60s)翻一次 1→0,`for` 会被打断而永不触发。
        - alert: DataServiceAuditConsumerStopped
          expr: avg_over_time(data_service_kafka_consumer_up[10m]) < 0.5
          labels: { severity: warning }
        # 保留期已经删掉了还没消费的审计消息:数据已丢。所有实例报同一个组的值,取 max。
        # for: 2m —— 探针先读 committed 后读 start,保留期恰在两次读之间推进时会有一次瞬时假阳性(§4.5);
        # 真缺口在消费者跳过之前持续存在。确证看 data_service 日志 `[kafka-go] ... skipping from offset`(§4.4)。
        # 注意:消费者跳过并提交后本 gauge 会自行归零,告警随之 resolve —— 「已丢多少」以那条日志为准,不以 gauge 为准。
        - alert: DataServiceAuditRetentionGap
          expr: max by (consumer) (data_service_kafka_consumer_retention_gap_messages) > 0
          for: 2m
          labels: { severity: critical }
        # 积压。【评审 M-3 改】阈值与 for 必须按「离删除线还有多少**时间**」定,不能按条数拍:
        #   删除线(条)≈ P × retention.bytes ÷ 平均快照字节;3 × 512MiB ÷ ~300KB ≈ 5300 条,
        #   在 R=128/s 时约 41 秒就到(§12)。原稿「> 2000 条且持续 15 分钟」在这个速率下要等删了十几分钟才响,等于没有。
        # 现取「删除线的 30%」且 for: 2m;2000 条只是 P=3、300KB 的占位值,换代(P=12)或实测快照大小后
        # 按 §12 公式重算并在这里写明出处(L-14 压测)。retention.check.interval 为 2 分钟(k8s_deploy.ps1 profile),
        # 所以 for 不能再长。Q-1 拍板放宽 retention.bytes 后同步放宽本阈值。
        - alert: DataServiceAuditLagHigh
          expr: max by (consumer) (data_service_kafka_consumer_lag_messages{consumer="player_snapshot"}) > 1600
          for: 2m
          labels: { severity: critical }
        - alert: DataServiceTxLogLagHigh
          expr: max by (consumer) (data_service_kafka_consumer_lag_messages{consumer="transaction_log"}) > 100000
          for: 15m
          labels: { severity: warning }
        # 探针失明:15 分钟内一次都没探成、且有失败。lag 告警此时全部是哑的。
        - alert: DataServiceAuditLagProbeBlind
          expr: |
            sum by (consumer) (increase(data_service_kafka_consumer_lag_probe_total{result="ok"}[15m])) == 0
            and sum by (consumer) (increase(data_service_kafka_consumer_lag_probe_total{result="error"}[15m])) > 0
          labels: { severity: warning }
        # 落库变慢:单次 DB 尝试 p99 > 2s 持续 15 分钟(全局库开始排队的信号,§16)。
        # flush_seconds 只计单次 DB 尝试(§4.6,评审 I-3),不含本实例信号量排队与重试退避,所以它慢 = 库慢。
        - alert: DataServiceAuditFlushSlow
          expr: histogram_quantile(0.99, sum by (le, consumer) (rate(data_service_kafka_consumer_flush_seconds_bucket{result="ok"}[5m]))) > 2
          for: 15m
          labels: { severity: warning }
        # 被跳过的坏消息(解不出 / 主键 0 / 超 MEDIUMBLOB):这些审计记录不会落库。序列已由 PrimeKafkaConsumer 预置为 0。
        - alert: DataServiceAuditMessagesSkipped
          expr: sum by (consumer, outcome) (increase(data_service_kafka_consumer_messages_total{outcome=~"decode_error|invalid|oversize"}[15m])) > 0
          labels: { severity: warning }
        # 批量失败、逐条兜底成功:批本身有问题(多半是包 / 事务大小),趋势信号。
        - alert: DataServiceAuditBatchIsolated
          expr: sum by (consumer) (increase(data_service_kafka_consumer_isolation_total{result="recovered"}[1h])) > 0
          labels: { severity: info }
```
每条补 `annotations.summary/description`(写清处置:查 data_service 日志 `STOPPED:` 行、`kafka-consumer-groups.sh --describe`、§10 runbook)。
C++ 生产端的发送失败不在 Prometheus(scene 没有 Prometheus 端点,Java `PARITY.md:45`),在 runbook 里给 LogQL 值班查询:
`{app="scene"} |= "[SnapshotSystem] Kafka send failed"`。
(评审补:这条是 LOG_ERROR,任何日志级别都在;而 `Snapshot captured ... size=` 是 LOG_INFO,`snapshot_system.cpp:109-112`,
生产若把 scene 日志级别调到 WARN 就没有了 —— §11 / §9 第 8 步用它统计大小分布时须确认压测环境的 scene 日志级别 ≤ INFO。
另外「captured」是**入队**时刻打的,不代表投递成功;投递失败走 librdkafka 的 dr_cb,不在这两条日志里。)

### 4.8 部署生成器与契约测试

- `go/data_service/etc/data_service.yaml` 的 `Kafka:` 段(`:232-242`)补五个键(值见 §4.1「本次 yaml 写」),每个键一行中文注释说明零值语义与建议开服值。
- `tools/scripts/k8s_deploy.ps1`:`:3089-3101` 加五个 `Get-AuthoritativeScalar -RelativePath $dsYaml -KeyPath 'Kafka.<键>'`;
  `:3542-3553` 的 ConfigMap `Kafka:` 段照写五行。生成器不得自带常数(契约测试的核心判据)。
- `tools/scripts/tests/k8s_deploy_contract.tests.ps1`:新增用例「data-service ConfigMap 的 Kafka 关键值 == go/data_service/etc/data_service.yaml」,
  形状照 db 那条(`:77-95`),pairs 覆盖 `TopicGeneration`、`TransactionLogPartitions`、`SnapshotPartitions`、`RetentionMs` 与五个新键。

---

## 5. 错误语义

| 情形 | 行为 | offset | 指标 | health |
|---|---|---|---|---|
| 消息解不出 / guid=0 / player=0 / 超 `MaxBlobBytes` | 跳过,随本批一起提交(现状) | 提交 | messages_total{decode_error\|invalid\|oversize} | 不变 |
| 1 行批落库失败、重试用尽 | 停机(现状逐字) | 不提交该行;它之前的坏消息提交 | db_error+1;flush_seconds{error} | StoppedOnError,LastError 可 Is 底层错误 |
| ≥2 行批落库失败、重试用尽(或命中 I-15 的确定性「批太大」错误) | 逐条兜底(每行同单行路径的有界重试):全成功 → 继续;某行重试用尽 → 停在该行 | 只提交失败行之前的前缀 | isolation{recovered\|stopped};db_error+1(停时) | 停时 StoppedOnError |
| 落库失败时 ctx 已取消(进程关停 / 兄弟分区停机) | 安静退出 | 不提交本批 | 不计 db_error | 不 markFailed |
| 任一分区停机 | 取消整个 Run,等全部 worker 退出,返回错误;supervisor 冷却 `ResumeAfter`(默认 60s)后重建 | 各分区停在各自最后一次成功提交处 | up=0 | StoppedOnError |
| worker panic | safego 记 `safego_panic_total{point="data_service.kafka.player_snapshot.partition"}`;Run 返回 `errSnapshotWorkerAborted`,同上重建 | 该分区在途批不提交 | up=0 | StoppedOnError |
| 提交 offset 失败 | 只记日志(行已落库,重放被 guid 去重,现状) | — | — | 不变 |
| store:唯一键路径 1062 | 当故障(撞的是别的唯一约束,现状同口径) | 不提交 | db_error | — |
| store:1213 | 就地重跑 ≤3 次,再交消费者通用重试(现状) | — | — | — |
| store:affected 超出 [0, len] | 当故障(CLIENT_FOUND_ROWS 被打开 / 语句被改坏),fail-closed | 不提交 | db_error | — |
| 配置非法(§4.1 范围外) | 拒启,exit 1(同 RouteStore) | — | — | — |
| lag 探针失败 | 记 error、下一轮重建连接;不影响消费 | — | lag_probe_total{error} | 不变 |
| 关停时手里有未落的批 | 不 flush(与现状关停丢弃在途一条同口径),重启后重放 | 不提交 | — | — |

---

## 6. 「必达 / 可丢」语义不变的论证(逐条)

1. **不丢**:任何分区的任何提交 V 都满足「该分区 offset < V 的有效消息都已落库」。归纳:worker 从组的已提交位置开始按 FIFO 处理,
   批内行在一条语句里原子落库或兜底逐条落库,提交只取已落库前缀的最高 offset。rebalance 重叠期旧主人迟到的提交可能把位置
   调低(只导致重放,被 guid 去重)或调高(调高到的位置以下都已由它落库),不变量都成立。现有单循环有同样的重叠窗口。
2. **不重**:批量路径与单行路径都靠 `uk_snapshot_guid_nz` + ODKU 判重;批内重复、跨批重放、两实例重叠写同一批,都恰好一行。
   退路形态(无唯一键)逐行走原语句,去重语义与现状相同(TiDB 上原本就失效,见 §14 R-6,不是本项引入)。
3. **毒丸行**:现状 = 一直停在它那里(每 60s 重试一轮)。新实现兜底后结局逐字相同(停在同一行、之前全部已提交),不新增跳过。
4. **关停 / 停机**:未落库的批一律不提交,重放。
5. **生产端丢弃(>1MB)与保留期删除(>512MiB/分区)**:都是既有行为,本项只加检测,不改 —— §15 拍板。
   【评审补】第 1 条「不丢」的论证只在**保留期没删掉未消费消息**的前提下成立。开服速率下这个前提能维持多久见 §0 第 4 条与 §12
   (示例速率下是秒到分钟级),所以「本规格不改变必达语义」的准确说法是:**不引入新的丢失途径**;既有的保留期丢失途径在开服前
   必须由 Q-1 处理,否则 1~4 条的保证会被 broker 的删除绕过。

结论:本规格**没有**改变「快照是否必达 / 是否可丢」的任何语义;所有可能改变它的选项都列在 §15。

---

## 7. 文件清单

### 新增
| 文件 | 要点 |
|---|---|
| `go/data_service/internal/kafka/snapshot_worker.go` | `snapshotBatch`、`snapshotPartitionWorker`(run / full / add / flush / isolate)、`runStop`、`errSnapshotWorkerAborted` |
| `go/data_service/internal/kafka/lag_probe.go` | `lagTarget`、`runLagProbe` |
| `go/data_service/internal/kafka/snapshot_worker_test.go` | §8 U-1…U-14(无外部依赖) |
| `go/shared/kafkautil/group_lag.go` | `GroupLag`、`groupOffsetReader`、`computeGroupLag`、`GroupLagProber` |
| `go/shared/kafkautil/group_lag_test.go` | §8 S-1(fake,无外部依赖) |
| `deploy/k8s/data-service-audit-alerts.yaml` | §4.7 的 8 条规则 |
| `docs/ops/audit-topic-generation-bump-runbook.md` | §10 全文 |

### 修改
| 文件 | 要点 |
|---|---|
| `go/data_service/internal/kafka/snapshot_consumer.go` | `SnapshotSink` 加批量方法;配置加四项与 withDefaults(`BatchMaxRows` 钳到 `store.MaxSnapshotBatchRows`);`sem`、`commitMu`;`Run` 改为 dispatcher;`commit` 改切片且**空切片不提交**;`insertWithRetry` 每次尝试 acquire/release 并 observe 单次尝试耗时;新增 `insertBatchWithRetry`(删掉原稿的 `insertOnce`:isolate 改用 `insertWithRetry`);类型注释改「按分区并发、可配批量」 |
| `go/data_service/internal/kafka/start.go` | `StartConfig.Snapshot` / `LagProbeInterval`;`:137` 传配置;条件起 lag 探针(**不进** `consumers.wg`,评审 I-2);启动日志 |
| `go/data_service/internal/kafka/consumer.go` | 包注释 `:1-15` 更新;`newReader` 配 `ErrorLogger`(评审 I-2) |
| `go/data_service/internal/kafka/txlog_consumer.go` | `flush` 在 rows>0 时 `ObserveKafkaConsumerBatchRows`、`insertWithRetry` 每次尝试 `ObserveKafkaConsumerFlushAttempt`,行为不动 |
| `go/data_service/internal/kafka/fakes_test.go` | `fakeSnapSink` 加 `InsertSnapshotsIfGuidAbsent`(记批、`failBatch`、`failGuids`、`gate chan`、`panicOn`、在途并发峰值计数)。**`gate` 必须同时挡单行与批量两个方法,且在 ctx 取消时返回 ctx.Err()**(真 sink 的 ExecContext 就是这样;不收 ctx 的 gate 会让 U-10 / U-11 死等) |
| `go/data_service/internal/kafka/snapshot_consumer_test.go` | 现有 5 条**不改**;`TestStartConfig_Validate` 不改 |
| `go/data_service/internal/kafka/consumer_integration_test.go` | 加 I-7(批量模式真库) |
| `go/data_service/internal/svc/servicecontext.go` | **评审 M-1**:`SnapshotStore` 接口加 `InsertSnapshotsIfGuidAbsent`(否则 `data_service.go:362` 编译失败);工作区已有他人改动,只追加 |
| `go/data_service/internal/logic/rollback_recall_test.go` | **评审 M-1**:`fakeSnapshotStore` 补 `InsertSnapshotsIfGuidAbsent` 桩(返回错误),否则 logic 测试包编译失败 |
| `go/data_service/internal/store/snapshot_store.go` | §4.2 常量拆分 + `MaxSnapshotBatchRows` + `insertSnapshotsOnDuplicateKeepSQL` + `InsertSnapshotsIfGuidAbsent`;import `strings` |
| `go/data_service/internal/store/export_test.go` | 导出 `InsertSnapshotsOnDuplicateKeepSQLForTest(n)` 供真库用例 |
| `go/data_service/internal/store/snapshot_store_test.go` | §8 T-1…T-4 |
| `go/data_service/internal/store/snapshot_store_integration_test.go` | §8 I-1…I-6(`//go:build integration`) |
| `go/data_service/internal/config/config.go` | §4.1 五键 + 常量 + `Normalize` / `ValidateConsumers`;`:441-442`、`:466` 注释 |
| `go/data_service/internal/config/config_test.go` | §8 C-1、C-2 |
| `go/data_service/internal/metrics/metrics.go` / `metrics_test.go` | §4.6 七个指标 + 七个函数 + register;M-1(两文件工作区都已有他人改动,只追加) |
| `go/data_service/data_service.go` | `Normalize/ValidateConsumers` 拒启;`StartConfig` 字面量;`PrimeKafkaConsumer` ×2 |
| `go/data_service/etc/data_service.yaml` | `Kafka:` 段五个键 + 注释 |
| `tools/scripts/k8s_deploy.ps1` | `:3089-3101` 读五键;`:3542-3553` 写五键 |
| `tools/scripts/tests/k8s_deploy_contract.tests.ps1` | P-1 |
| `docs/design/data_service_role_and_scope.md` | `:58` 消费者一段:按分区并发 / 批量开关 / lag 探针 / 新指标 |
| `deploy/k8s/README.md` | `:673-674` 换代顺序改为「停写 → 排空 → 切」,指向 runbook(修正现有丢数顺序) |
| `docs/README.md` | 「运维手册」分类登记 `- [audit-topic-generation-bump-runbook.md](ops/audit-topic-generation-bump-runbook.md) — <runbook 首行标题>`,按文件名排序放在 `cross-zone-failure-test-runbook.md` 之前(`gen_docs_index.py` 的 `*runbook*` 规则已覆盖,无需改生成器) |

**不改**:AGENTS.md;docs/PROGRESS.md(Docs 阶段统一追加);C++ 任何文件;`data_service.yaml` 的 `TopicGeneration` / `SnapshotPartitions`
(换代是 §10 的独立运维变更,不与代码同批);`generated/` 下任何产物;go.mod(sarama 只在 shared 模块被直接 import)。

**可选(建议拆为 G2a-2,会碰他人未提交改动)**:`tools/scripts/stress_snap.ps1` 加 data_service `:9260` 的抓取(文件名 `t<N>m_ds.txt`),
`tools/scripts/stress_summarize.ps1`(工作区已有他人改动,须在其上追加、不整理)加「审计管道」一段:snapshot / txlog 的 lag、flush p50/p99、
batch_rows 均值、message_bytes p50/p99/max。AGENTS §6.1 规定复盘只读脚本输出,没有这一段就没法按纪律给出 G2a 的对比表。
【评审 I-8】所以 G2a-2 对**合入 G2a 代码**是可选的,对 **Q-7(把批量开关打开)是前置**:没有脚本出的对比表,不得把 yaml 从 1 改成 16。

---

## 8. 测试清单

| ID | 用例名 | 位置 | 外部依赖 | 断言要点 |
|---|---|---|---|---|
| U-0 | 现有 `TestSnapshotConsumer_InsertsSource1RowThenCommits` / `_DuplicateGuidStillCommits` / `_SkipsBadMessagesAndCommitsThem` / `_StopsWithoutCommitWhenDBKeepsFailing` / `_TransientDBErrorRecovers` | `snapshot_consumer_test.go:25-152` | 无 | **一字不改全部通过** = 默认配置与现状等价(含 5 次独立提交、`LastError` Is `errDBDown`) |
| U-1 | `TestSnapshotConsumer_DefaultConfigNeverCallsBatchSink` | `snapshot_worker_test.go` | 无 | 默认配置推 3 条同分区:单行调用 3、批量调用 0、提交 3 次且 offset 递增 |
| U-2 | `TestSnapshotConsumer_BatchesByRowsAndCommitsHighestOffset` | 同上 | 无 | `BatchMaxRows=3, FlushInterval=1h`:一次批量 3 行、行序 = offset 序、只提交一次 = 第 3 条 offset |
| U-3 | `TestSnapshotConsumer_BatchFlushesOnInterval` | 同上 | 无 | `BatchMaxRows=100, FlushInterval=20ms`:推 2 条,`Eventually` 一次批量 2 行 |
| U-4 | `TestSnapshotConsumer_BatchFlushesOnBytes` | 同上 | 无 | `BatchMaxBytes` 小于两条 data 之和:第二条进批即落,批 2 行;**批为空时**到达的超上限单条 = 一批 1 行(评审改:软上限只在加入后检查,非空批里来的超大条会与前面的同批) |
| U-5 | `TestSnapshotConsumer_ZeroIntervalDrainsQueuedWithoutWaiting` | 同上 | 无 | `FlushInterval=0, BatchMaxRows=10`:sink 首次调用(1 行,走**单行**接口)被 gate 挡住期间再推 4 条,放行后第二次调用是**批量**接口、正好 4 行 |
| U-6 | `TestSnapshotConsumer_InflightFlushesBoundedBySemaphore` | 同上 | 无 | 两分区、sink 记在途峰值:`MaxInflightFlushes=1` 峰值恒 1;`=2` 时用屏障证明两分区确实同时在途 |
| U-7 | `TestSnapshotConsumer_PerPartitionOrderAndMonotonicCommits` | 同上 | 无 | 两分区交错推 20 条:每分区 sink 看到的 guid 序 = offset 序;fakeReader 每分区提交序列严格递增 |
| U-8 | `TestSnapshotConsumer_BatchFailureIsolatesPoisonRowAndCommitsPrefix` | 同上 | 无 | 批 `[g1, g2, bad, X, g5]`,批量恒失败、单行仅 X 失败:g1/g2 落库;提交到 bad 的 offset;Run 返回 Is `ErrConsumerStoppedOnDB` 且 Is 底层错误;`StoppedOnError`;isolation{stopped}+1 |
| U-9 | `TestSnapshotConsumer_BatchFailureRecoveredRowByRowContinues` | 同上 | 无 | 批量恒失败、单行全成功:整批提交、继续消费下一批、health 健康;isolation{recovered}+1 |
| U-10 | `TestSnapshotConsumer_OnePartitionStopStopsAllWithoutCommittingInFlight` | 同上 | 无 | **`MaxInflightFlushes=2`**(=1 时 p1 占着唯一名额被 gate 挡住,p0 永远拿不到名额、永远不会失败,用例死等);p0 恒失败;p1 的 sink 被 ctx 感知的 gate 挡住(在途)→ p0 停机后 p1 在途批不提交;Run 返回错误且所有 worker 已退出(返回后 sink 不再被调) |
| U-11 | `TestSnapshotConsumer_ShutdownMidFlushIsNotAFailure` | 同上 | 无 | sink(ctx 感知的 gate)挡住时 cancel 父 ctx:Run 返回 nil、不 `StoppedOnError`、无提交、db_error 不增(用 `prometheus/testutil.ToFloat64` 前后对比) |
| U-12 | `TestSnapshotConsumer_WorkerPanicStopsRunInsteadOfHanging` | 同上 | 无 | sink panic:Run 在时限内返回 Is `errSnapshotWorkerAborted`;不提交该批;同一 consumer 的信号量名额已归还(panic 后在同一实例上再建一个 Run 不会卡在 acquire —— 或断言 `len(c.sem)==0`) |
| U-13 | `TestSnapshotConsumer_BatchDuplicatesCountedAndCommitted` | 同上 | 无 | 批量返回 inserted<len:duplicate 计数 = 差值;整批提交 |
| U-14 | `TestSnapshotConsumer_FirstRowFailureMakesNoEmptyCommitAndBatchDropsPayload` | 同上 | 无 | 评审 M-2 / I-1:①批 `[X]`(X 恒失败,无前置坏消息)→ `reader.commitCalls()` 为空(没有空提交);②批量模式下 fakeReader 收到的提交消息 `Value == nil` 且 `Topic` 保持原值(批里剥掉了载荷但保留了提交所需字段;现有 `snapMsg` 不填 Topic,本用例构造消息时显式填一个) |
| T-1 | `TestInsertSnapshotsOnDuplicateKeepSQLShape` | `snapshot_store_test.go` | 无 | n=1 与 `insertSnapshotOnDuplicateKeepSQL` 逐字相同;n=3 占位符 27、恰一个 `ON DUPLICATE KEY UPDATE`、以 `insertSnapshotSQL` 开头;`MaxSnapshotBatchRows*9 < 65535` |
| T-2 | `TestInsertSnapshotsRejectsZeroGuidAndOversizeBatchBeforeTouchingDB` | 同上 | 无 | `&SnapshotStore{uniqueGuidKey:true}`(db=nil):含 guid=0 / 超 256 行返回 error 且不 panic;空切片返回 (0,nil) |
| T-3 | `TestMaxSnapshotBatchRowsMatchesConfigCap` | 同上或 config 包 | 无 | `store.MaxSnapshotBatchRows == config.MaxSnapshotBatchMaxRows` |
| T-4 | 现有 `TestInsertSnapshotOnDuplicateKeepSQLShape` / `TestGuidDedupeStatementFollowsProbe` | `snapshot_store_test.go:298-371` | 无 | 不改即通过(常量拆分文本不变) |
| I-1 | `TestSnapshotStore_BatchDedupsAgainstStoredAndWithinBatch` | `snapshot_store_integration_test.go` | 本地 MySQL | 先单插 g1;批 `[g1, g2, g2', g3]`(g2' 同 guid 不同 data)→ inserted=2;三个 guid 各一行;g2 的 data = 先出现那条 |
| I-2 | `TestSnapshotStore_BatchReplayIsNoop` | 同上 | 本地 MySQL | 同批插两次:第二次 inserted=0、行数与 id 不变 |
| I-3 | `TestSnapshotStore_BatchIdsFollowRowOrder` | 同上 | 本地 MySQL | 同玩家 5 行:id 严格递增与行序一致 |
| I-4 | `TestSnapshotStore_BatchFallsBackWhenUniqueKeyAbsent` | 同上 | 本地 MySQL | 照 `TestSnapshotStore_FallsBackWhenUniqueKeyAbsent`(`:1066-1092`)删键后批量:去重正确、inserted 正确 |
| I-5 | `TestSnapshotStore_OverlappingBatchesFromTwoWritersLandOneRowEach` | 同上 | 本地 MySQL | 两 goroutine 并发插重叠批(模拟 rebalance 重叠)各 20 轮:每 guid 恰一行、无错误外泄(1213 被吸收);记录 `DeadlockRetriesForTest` |
| I-6 | `TestSnapshotStore_BatchOfLargeBlobsFitsPacket` | 同上 | 本地 MySQL | 8 行 × 512KiB data(共 4MiB,走 COM_STMT_SEND_LONG_DATA 路径)成功,字节原样读回 |
| I-7 | `TestSnapshotConsumer_RealStore_BatchModeDedupsAndCommits` | `consumer_integration_test.go` | 本地 MySQL | `BatchMaxRows=8, FlushInterval=50ms, MaxInflightFlushes=2`,两分区含重复 guid:落库行数精确、两分区提交到末尾、GM 读路径仍看不到 source=1 |
| C-1 | `TestKafkaConsumerKnobsDefaultToRowByRow` | `config_test.go` | 无 | yaml 无新键 → `Normalize` 后 1 / 4MiB / 0 / 1 / 0;整段 Kafka 缺失同样 |
| C-2 | `TestKafkaConsumerKnobsValidate` | 同上 | 无 | 表驱动:rows −1 / 257 拒;bytes 1 / 64MiB 拒;interval 5001 拒;inflight 5 在池 5 时拒、4 过、池 1 时 1 过、**池 0(未配,按 5)时 4 过 5 拒**;lag 1 / 4999 / 600001 拒,0 / 30000 过 |
| M-1 | `TestPrimeKafkaConsumerCreatesAllSeries` | `metrics_test.go` | 无 | 照 `TestPrimeRouteCreatesAllSeries`(`:98`)的写法:6 + 2 + 2 条序列存在且为 0 |
| S-1 | `TestComputeGroupLag` | `go/shared/kafkautil/group_lag_test.go` | 无 | fake:正常 / 未提交(-1)/ committed<start(gap)/ committed>end(error)/ block Err / 响应顶层 Err / 调用失败 七种;另断言**调用顺序**:fake 记录调用序列,`ListConsumerGroupOffsets` 先于任何 `GetOffset`(评审 I-2) |
| P-1 | `data-service ConfigMap 的 Kafka 关键值 == go/data_service/etc/data_service.yaml` | `k8s_deploy_contract.tests.ps1` | 无(DryRun) | 9 个键逐个相等 |
| R-1 | `promtool check rules` | `deploy/k8s/data-service-audit-alerts.yaml` 的 `spec.groups` | promtool(没有就如实记录) | 8 条、无语法错误 |
| E-1 | 本机端到端冒烟 | compose 全栈 | Kafka + MySQL + scene | 见 §9 第 7 步 |
| L-14 | 开服尖峰压测(含进退场快照) | AGENTS §6 全流程 | 压测环境 | 见 §9 第 8 步 |

---

## 9. 交给 Codex 的验证命令与通过标准

工作目录一律 `D:\luyuan\xuanming-server-mmo-cpp`(PowerShell)。先后顺序如下,任一步失败保留完整输出并停下,不自动重试。

1. **格式**
   `cd go\data_service; gofmt -l internal data_service.go` 与 `cd go\shared; gofmt -l kafkautil` —— 通过标准:两条都无输出。
2. **构建与静态检查**
   `cd go\shared; go build ./...; go vet ./kafkautil/...`
   `cd go\data_service; go build ./...; go vet ./...` —— 通过:零错误。若 `go vet` 报 shared 以外的既有问题,贴原文,不顺手修。
3. **单测(无外部依赖)**
   `cd go\shared; go test ./kafkautil/... -count=1`
   `cd go\data_service; go test ./internal/kafka/... ./internal/store/... ./internal/config/... ./internal/metrics/... -count=1`
   有 CGO 工具链时再加一遍 `-race`(并发用例 U-5…U-14 尤其需要);没有就如实记录「未跑 -race」。
   通过:全部 PASS;U-0 的五条现有用例必须在**未修改**的情况下通过。
   另跑 `cd go\data_service; go vet ./internal/logic/... ./internal/svc/...` 与 `go test ./internal/logic/... -count=1`
   —— 评审 M-1 改了 `svc.SnapshotStore` 接口,logic 测试包里的 fake 是第一个会编译失败的地方。
4. **真库集成(需本机 MySQL)**
   前置:`docker compose -f deploy\docker-compose.yml up -d mysql`;appuser 无建库权限时用 root:
   `$env:DATA_SERVICE_IT_MYSQL_USER='root'; $env:DATA_SERVICE_IT_MYSQL_PASSWORD='<本机 root 口令>'`(口令不进日志 / 不贴进汇报)。
   `cd go\data_service; go test -tags=integration ./internal/store/... ./internal/kafka/... -run "Snapshot" -count=1 -v`
   通过:全部 PASS;MySQL 上 I-1…I-7 **不得 Skip**(只有 TiDB 才允许那条「迁移没有建出唯一键」的 Skip)。
5. **部署生成器契约**:`pwsh -File tools\scripts\tests\k8s_deploy_contract.tests.ps1` —— 全部通过(含新增 P-1)。
6. **文档索引与告警**:`python tools\scripts\gen_docs_index.py --check` 退出码 0;
   抽出 `deploy\k8s\data-service-audit-alerts.yaml` 的 `spec.groups` 跑 `promtool check rules` —— 8 条无错误(没有 promtool 就记录未校验)。
7. **本机端到端冒烟(E-1)**:按平常方式起全栈(`start_game.ps1`),robot 登录 / 登出 10 个账号。
   - `curl -s http://127.0.0.1:9260/metrics | findstr kafka_consumer` 应见:`flush_seconds`、`batch_rows`、`message_bytes`、
     `lag_messages`(30s 后出现)、`retention_gap_messages = 0`、`lag_probe_total{result="ok"}` 递增、`up = 1`。
   - `player_snapshot` 表 `source=1` 行数 = scene 日志 `[SnapshotSystem] Snapshot captured` 行数;`kafka-consumer-groups.sh --describe --group data_service-player-snapshot` LAG 全 0。
   - 再把 yaml 切到建议值(16 / 4MiB / 200 / 2)重跑一遍,结果相同、`batch_rows` 均值 > 1。
8. **压测(L-14 的一部分,AGENTS §6 全流程,先存 prev-summary、清 Redis / MySQL / etcd、重置 offset)**:
   同一环境跑两轮 —— 默认(逐条)与建议值(批量)。必须产出:snapshot lag 曲线、flush p50/p99、batch_rows 均值、
   message_bytes p50/p99/max、data_service RSS(`process_resident_memory_bytes`,对照 512Mi 上限)、全局库 `Threads_running` / fsync 速率、
   scene 日志里 `Kafka send failed` 条数。用 `stress_summarize.ps1` 出对比表(需要 §7 的可选 G2a-2 才有这一段;没有就如实说明缺口)。
   **没有对比表不得声称吞吐提升**。

---

## 10. runbook:审计 topic 离线换代(写入 `docs/ops/audit-topic-generation-bump-runbook.md`)

> 适用:`player_snapshot_topic` / `transaction_log_topic` 改分区数。两者**共用一个世代号**(`data_service.yaml` `Kafka.TopicGeneration` ↔
> C++ `AuditTopicGeneration`),所以只想扩快照也会让流水换到新 topic(流水分区数可以不变)。
> 原则同 `db-task-kafka-partition-contract.md:32-69`:停写 → 排空 → 三方同改 → 消费者先起、生产者后起。
> **不在 TiDB Phase 1 迁移窗口里做**(D5「借迁移之机误动 topic/partition」,`global-data-layer-tidb-decision.md:208`)。

**0. 决策与预算(开工前)**
- 新分区数:快照 S、流水 T(T 默认保持 6)。S 的依据:G2a 之后全球并行落库数 ≈ Σ_实例 min(该实例分到的分区数, M) ≈ min(S, 2Z × M)
  (M = `SnapshotMaxInflightFlushes`)。【评审 M-4 改】原稿写 min(S, 2Z, 2Z×M),多出的「2Z」是 G2a 之前「每实例一条循环」的上限,
  G2a 之后一个实例可以同时写 M 个分区,它不再是上限(例:S=12、Z=2、M=2 → 8 路,不是 4 路)。
- 【评审 M-4 改】Kafka 盘预算必须按**整台 broker** 算,不能只算审计 topic。副本数 3、broker 3 台时每台 broker 持有每个分区的一份副本,
  最坏占用 ≈ Σ_所有分区 min(retention.bytes, 该分区写速 × retention.ms)。按 prod 每分区 512MiB 的上界:
  审计 (S+T)×512MiB **加上** 每个 zone 的 db_task(10 分区,`db-task-kafka-partition-contract.md`)Z×10×512MiB,再加命令 / game-events 等。
  例:S=12、T=6、Z=2 → 9GiB + 10GiB = 19GiB,**已经顶到 20Gi PVC(`kafka.yaml:363`)**;broker 盘满 = 所有 zone 的存盘与命令一起停,
  事故面远大于审计本身。所以 S=12 的前提是**同批扩 Kafka PVC**(或给审计 topic 单独设更小的 topic 级 `retention.bytes`,但那会缩短 §12 的
  T_safe,与 Q-1 冲突),由负责人在 Q-5 / Q-1 一起拍板。原稿「9GiB 远小于 20Gi」的结论不成立。
  S=24 时审计单项就是 15GiB,不建议(待拍板,§15 Q-5)。
- 开服前做:topic 里只有测试数据,排空是空操作,窗口最短。**开服后做 = 全服停服维护**(scene 是唯一生产者,停写 = 停所有 scene)。

**1. 同一次提交改配置(先不部署)**
- `go/data_service/etc/data_service.yaml`:`TopicGeneration: N+1`、`SnapshotPartitions: S`(、`TransactionLogPartitions: T`)。
- `go/data_service/internal/config/config.go:458-468` 的默认值与 `:441-442`、`:466` 注释同步(避免缺键时落回旧数;marker 会让不一致的实例启动失败,但别依赖它)。
- `bin/etc/base_deploy_config.yaml:156`:`AuditTopicGeneration: N+1`(K8s 的 node ConfigMap 由 `k8s_deploy.ps1` 从 data_service.yaml 镜像,不抄这份)。
- 本机两处写死的数:`deploy/docker-compose.yml:98`(`KAFKA_INIT_AUDIT_TOPIC_GENERATION` 默认值)、`:189-194`(6 / 3);
  `tools/scripts/start_game.ps1:148`、`:160-161`。
- 描述性文字:`deploy/k8s/README.md:656`、`:661`;`deploy/k8s/manifests/go-svc/data-service.yaml:33`;`kafka-topic-init.yaml:14`;
  `docs/design/data_service_role_and_scope.md:58`;§4.7 告警阈值注释按新 S 重算。
- 契约测试 `k8s_deploy_contract.tests.ps1:811-821`(AuditTopicGeneration 与 TopicGeneration 相等)应自动跟随。

**2. 停写**:停掉**所有 zone 的所有 scene**。核对 `kafka-get-offsets.sh` / `kafka-topics.sh --describe` 两次间隔 2 分钟,两个 g N topic 的 end offset 不再变化。

**3. 排空**:g N 的 data-service 保持运行。
- `kafka-consumer-groups.sh --bootstrap-server <b> --describe --group data_service-player-snapshot` 与 `--group data_service-transaction-log`:
  g N topic 的每个分区 LAG = 0。
- 同时看 `data_service_kafka_consumer_lag_messages` = 0、`retention_gap_messages` = 0、`kafka_consumer_up` = 1;
  data_service 日志无 `STOPPED:`。有 STOPPED 先按日志修 DB,**不许跳过**。
- `retention_gap_messages > 0` = 已有审计数据被保留期删除:记录条数与时间段,上报负责人,不能当作排空完成的障碍「清掉」。

**4. 停 data-service**(所有 zone)。

**5. 预建新代 topic**:K8s `pwsh -File tools/scripts/k8s_deploy.ps1 -Command infra-kafka-topics -WaitReady`;
compose `docker compose -f deploy/docker-compose.yml up kafka-topic-init`。
核对 `--describe --topic player_snapshot_topic_g<N+1>`:`PartitionCount: S`、`retention.ms=2592000000`。

**6. 起 data-service(新配置)**:日志应见 `[kafka] consumers started: ... topic=player_snapshot_topic_g<N+1> ... p=S`、
`kafka immutable partition marker ensured`;`kafka_consumer_up = 1`。
(评审补:K8s 上「新配置」= 用改过的仓库重新生成并 apply 两份 ConfigMap —— `go-svc-data-service-config`(Kafka.TopicGeneration / *Partitions)
与 `node-config`(AuditTopicGeneration,由 data_service.yaml 镜像)—— 再滚动 Pod;runbook 正文写出本仓实际使用的 `k8s_deploy.ps1` 子命令,
并在第 7 步前用 `kubectl get cm node-config -o yaml` 核对 `AuditTopicGeneration: N+1`,不要只靠契约测试。)

**7. 起 scene(新 `AuditTopicGeneration`)**:必须晚于第 5、6 步(broker 开着 auto.create + num.partitions=1,scene 抢先发会把新 topic 建成 1 分区,
data-service 的分区契约随即永久失败)。

**8. 验证**:一个账号登录 / 登出 → `player_snapshot` 出现 2 行新 guid;新 topic 的 group offset 前进、LAG ≈ 0;
LogQL `{app="scene"} |= "[SnapshotSystem] Kafka send failed"` 无新增。

**9. 收尾**:旧 g N topic 与 marker 至少保留一个审计窗口(30 天)再删;删除不属于本流程。

**回滚**:同样「停写 → 排空 g N+1 → 三方切回 g N」。g N 的组 offset 若已过 `offsets.retention.minutes`(Kafka 默认 7 天)被清,
消费者会从 g N 最早位置重读(StartOffset=FirstOffset,`consumer.go:131`)—— 由 guid / tx_id 去重保证幂等,只是多耗时间。
已有新写时直接把配置切回旧 topic,禁止(形成两段历史,与 db-task 契约同理)。

**对 `deploy/k8s/README.md:673-674` 的修正**:现文「建出 `*_g2` → 滚 data-service 与 scene → 老 topic 排空后再删」中,
data-service 先切到 g2 后不再消费 g1,而 scene 未滚完前仍在写 g1 —— g1 尾部无人消费,保留期后丢失。改为指向本 runbook 的顺序。

---

## 11. 第 4 项:单条快照大小 vs Kafka 消息上限(核对结论)

| 环节 | 上限 | 依据 |
|---|---|---|
| C++ 生产者 `message.max.bytes` | **1,000,000 字节**(librdkafka 默认;本仓未设) | `kafka_producer.cpp:59-76` 只设四项 |
| broker `message.max.bytes` / topic `max.message.bytes` | 1,048,588(Kafka 默认;compose、`kafka.yaml`、topic 预建都未设) | `docker-compose.yml:5-53`、`kafka-topic-init.yaml`、`topic_init.go:115-125` |
| kafka-go reader `MaxBytes` | 16MiB | `consumer.go:133` |
| 消费者 `MaxBlobBytes` | 16MiB−1(MEDIUMBLOB) | `snapshot_consumer.go:21-24` |
| MySQL `max_allowed_packet` | 64MiB(8.0 默认;未配) | `mysql.yaml` 无此项 |
| TiDB `txn-entry-size-limit` | 默认 6MB;dev 已配 32MiB | `tidb-phase1-and-launch-readiness.md:188` |

- **有效上限 = 约 1,000,000 字节,在生产端本地。** librdkafka 对 key+value 超过 `message.max.bytes` 的消息在 `produce()` 里直接返回
  `ERR_MSG_SIZE_TOO_LARGE`(按库的公开行为推断,本仓未实测),它不是 `QUEUE_FULL` / `FATAL`,落入 `kReject`(`kafka_producer_policy.h:35-44`):
  计 `IncProduceRejected`、打限频日志(`kafka_producer.cpp:161-165`),`CaptureAndSend` 打 `Kafka send failed ... err=` 返回 0(`snapshot_system.cpp:102-107`),
  调用方忽略返回值(`player_lifecycle.cpp:2737`、`:3555`)。**这个玩家这一次的快照就没有了,且 C++ 侧没有 Prometheus 计数。**
- 压缩救不了生产端:该检查发生在压缩之前(按库行为推断);压缩只影响 broker 侧按压缩后批大小的检查。
- **单条快照实际多大,本仓没有数据**(代码注释「几百 KB」,`config.go:466`,未实测)。G2a 带来两个数据源:
  消费端 `message_bytes` 直方图(只看得到发成功的),以及 scene 日志 `Snapshot captured ... size=` 与 `Kafka send failed` 行(Codex 在 §9 第 8 步统计 p50 / p99 / max 与失败条数)。
- 本规格不改它;选项见 §15 Q-2。若将来调高到 > 6MB,TiDB 的 `txn-entry-size-limit` 必须同步 ≥ 新上限,否则那条消息成为毒丸、卡住所在分区
  (`tidb-phase1-and-launch-readiness.md:507`)。

---

## 12. 容量估算方法(开服前填数,数字以压测为准)

- 快照速率 R = 全球(登录速率 + 登出速率);每分区 R/P(librdkafka 默认分区器按 key 哈希,近似均匀)。
- 每条落库循环的吞吐:逐条 ≈ 1 / t_row;批量 ≈ B / t_batch(B)。t_row、t_batch 用 `flush_seconds` 实测。
- 全球消费吞吐 ≈ min(P, 2Z × MaxInflightFlushes) × 每循环吞吐(G2a 之后;之前是 min(P, 2Z)),同时受全局库写入能力(fsync / 写带宽)封顶。
  (评审 M-4:原稿多写了一个 2Z。)
- 积压不丢的上限(条):P × `log.retention.bytes` / 平均快照字节;prod 下 3 × 512MiB / 300KB ≈ 5300 条。
- 【评审 M-3】**换算成时间**:T_safe = P × `log.retention.bytes` ÷ (R × 平均快照字节)。R=128/s、300KB 时 P=3 约 41s、P=12 约 164s;
  10KB 时约 20 / 80 分钟。T_safe 必须**大于**「最长可容忍的 DB 故障 + supervisor 冷却(`ResumeAfter` 60s)+ 积压追平时间」,
  否则任何一次 DB 抖动都会丢审计数据。broker 还按 `log.retention.check.interval.ms`(prod 2 分钟)成段删除,删除是阶梯式的,不是平滑的。
- 反过来算盘:真正保留 30 天需要 R × 平均字节 × 30d(R=128/s、300KB 约 98TB;10KB 约 3.3TB),20Gi PVC 差几个数量级 ——
  「30 天」在开服速率下只是名义值,Q-1 必须用这条算式给出一个**能落地**的保留时长与盘量。
- 全局库写带宽 ≈ R × 平均字节 ×(数据 + redo + binlog 约 3 倍)。例:R=128/s、300KB → 约 38MB/s 数据、~115MB/s 落盘(**示例,非实测**;R18 平均约 64 次进场/s,`stress-1zone-45k-2026-06-04-round18.md:9`、`:32`)。

---

## 13. 与现有设计的偏离与代价

| # | 偏离 | 理由 | 代价 |
|---|---|---|---|
| D-1 | `snapshot_consumer.go:31` 的「不攒批:单条几百 KB,逐条落、逐条提交」改为可配批量(默认仍逐条) | 首日吞吐的主要杠杆;批字节上限 4MiB 远低于包 / 事务上限,失败有逐条兜底 | 多一条批量 SQL 路径与 worker 并发代码;需要真库回归 I-1…I-7 |
| D-2 | 批量路径不再逐 guid 打 `already stored as id=%d`,改为每批一条汇总 | 多行 ODKU 不回带每行 id | 排查单个重复 guid 要靠 offset 区间 + 查库;逐条模式(默认)行为不变 |
| D-3 | 消费者装配前新增一处拒启校验 | 与 RouteStore / PlayerName 一致:拒绝而不纠正 | 一个写错的审计旋钮会让 data_service 起不来(发号、Load/Save 一起停)。部署前由契约测试与 yaml 默认值兜住 |
| D-4 | 新增 lag 探针(sarama 只读 admin 调用) | kafka-go 组模式下没有可信的 lag | 每实例每 30s 2×(P+1) 次 broker 请求;2Z 个实例重复探同一组(值相同,告警取 max) |
| D-5 | 一个分区停机仍停整个消费者(未做「坏分区隔离、其它分区继续」) | 与现状语义一致、最简单 | 一个毒丸行会拖住所有分区的落库(现状同样如此) |
| D-6 | 换代修正了 `deploy/k8s/README.md:673-674` 的顺序 | 原顺序会丢 g1 尾部 | 换代需要全服停写窗口(开服前做则代价最小) |
| D-7 | lag 探针在 yaml 里默认打开(`LagProbeIntervalMs: 30000`),偏离「新行为默认关闭」 | 只读 broker 元数据、不碰消费与落库;没有它 §4.7 的积压 / 保留期告警全哑 | 每实例一对 sarama 长连接;broker 不可达时每 30s 一条 Error(由 LagProbeBlind 收口) |
| D-8 | isolate 每行用与单行路径相同的有界重试(评审 I-5) | 单行容错与现状逐字相同,不因批量而更容易停机 | DB 真挂时停机前多耗约 DBMaxAttempts×DBRetryBackoff |

---

## 14. 风险

| # | 风险 | 级 | 缓解 |
|---|---|---|---|
| R-1 | kafka-go Reader 并发 `CommitMessages` 的安全性未在本仓核对(源码不在本机模块缓存) | 中 | 规格已用 `commitMu` 串行化,不依赖该承诺;实现者读 vendored `reader.go` 确认后可保留 mutex(无吞吐损失) |
| R-2 | rebalance 时已派发给 worker 的旧代消息仍会被落库并提交 | 低 | 「先落库后提交」使提交永远 ≤ 已落库前缀;最坏是重放,被 guid 去重(现状单循环有同样窗口) |
| R-3 | 内存:每实例在途 ≈ kafka-go 内部队列(QueueCapacity 默认 100 条)+ 拥有分区数 × (通道 BatchMaxRows + 批 BatchMaxRows) 条,单条最坏约 1MB。【评审 I-1 改】原稿漏算两件事:①批里的 msgs 若保留 Value,每条在批里是**两份**(msg.Value + decode 拷贝出的 row.Data)—— 已在 §4.3 `add` 剥掉;②Go GC 默认 GOGC=100,RSS 可到存活堆的约 2 倍。且 data-service 同进程跑 Load/Save 热路径(单个 PlayerAllData 可达十几 MB),被 OOMKill = 玩家数据面中断,不只是审计停 | **高** | 剥载荷后建议值 16、S=12、Z=1(每实例 6 分区)最坏存活约 100+6×32 ≈ 290 条 × 1MB,GC 余量下仍会顶破 512Mi。所以 Q-7 打开批量的前置:§9 第 8 步实测 RSS 峰值 + 在 Deployment 配 `GOMEMLIMIT`(如 limits 的 80%)或提 limits,二者由负责人拍板(§15 Q-9);未拍板前不打开批量 |
| R-4 | 批量提高单条语句的持锁 / 执行时间,GM 读快照时可能等更久 | 低 | `MaxInflightFlushes ≤ 池上限−1` 保留连接;`flush_seconds` 告警 |
| R-5 | dispatcher 在某分区通道满时阻塞,其它分区跟着等(队头阻塞) | 低 | 同一个库,慢是整体的;单实例多分区只在 2Z < S 时出现 |
| R-6 | TiDB 上建不出 `uk_snapshot_guid_nz` → 走退路 → 去重失效(既有,launch_verdict P1) | 高(既有) | 不是本项引入;批量在退路上逐行,不放大也不修复 |
| R-7 | 换代漏改任一处(C++ 世代号、compose、start_game)= 生产者写进没人消费的 topic,静默积压到保留期 | 高 | runbook 第 1 步清单;`start_game.ps1:132-134` 与契约测试核对三方相等 |
| R-8 | 保留字节先于 30 天到达,积压超过 P×512MiB 静默删除(既有) | 高(既有) | 本项加 `retention_gap` critical 告警;改策略 §15 Q-1 |
| R-9 | 生产端 >1MB 快照丢失,且 C++ 无指标(既有) | 中(既有) | runbook 给 LogQL;§15 Q-2 |
| R-10 | `innodb_redo_log_capacity` 10% 限制的确切口径(单行还是单事务)未核实 | 低 | 批上限 4MiB 在 10MB 之下;调大前先在目标库实测 |
| R-11 | 【评审补】DB 单次尝试没有超时(既有:`insertWithRetry` 只用 runCtx,DSN 也没设 readTimeout)。连接半死时一次 ExecContext 可挂到 TCP keepalive 判死,期间占着信号量名额,同实例其它分区一起停 | 中(既有,批量放大占用) | 建议(不阻塞本批):批量与单行尝试都包 `context.WithTimeout(ctx, 30s)`(常量,注释写 AGENTS §11.3);超时 = 可重试错误,语句若已在服务端提交,重试被 guid 去重。若采纳,U-0 不受影响 |
| R-12 | 【评审补】supervisor 冷却 `ResumeAfter` 默认 60s(既有)与开服速率下的 T_safe(§12,示例 41s / 164s)同一量级:一次 DB 故障 + 一次冷却就可能越过删除线 | 高(既有) | 不在本批改;Q-1 拍板时一起定(放宽 retention.bytes 使 T_safe ≫ 冷却 + 可容忍故障时长) |
| R-13 | 【评审补】lag 探针读取顺序带来的瞬时假阳性 / 假错误 | 低 | §4.5 规定先读 committed;RetentionGap 告警 `for: 2m`;确证看 kafka-go ErrorLogger 日志 |
| R-14 | 【评审补】TiDB 上若将来落地「TiDB 形态的去重」使 uniqueGuidKey=true,批量 ODKU 会在 TiDB 上生效:多行 ODKU 的 affected-rows 口径与 MySQL 一致与否、raft-entry-max-size(默认 8MB)对 4MiB 批的影响都未核实 | 中(条件性) | 那一批在 TiDB 上重跑 I-1 / I-2 / I-6 再打开批量;affected 越界已 fail-closed(§4.2) |

---

## 15. 待负责人拍板(凡涉及「必达 / 可丢」的都在这里,本规格一律未改)

| # | 问题 | 选项 | 影响「可丢」? | 建议 |
|---|---|---|---|---|
| Q-1 | 审计 topic 的 `retention.bytes`:现在继承 broker 每分区 512MiB,积压超过即删,「30 天」不是先到的上限。**【评审 M-3】开服前必须拍板(阻塞项)**:按 §12 换算,示例速率下 T_safe 只有几十秒到几分钟 | a) topic 级 `retention.bytes=-1`(真正 30 天;按 §12 算式在开服速率下是 TB 级盘,20Gi 远不够,基本不可行);b) 显式设一个按「可容忍 DB 故障时长 × R × 字节」算出的值,并同批扩 Kafka PVC(§10 第 0 步的整机盘预算);c) 维持 + 只告警(本规格已做 c,**开服速率下 c 等于接受丢审计**) | **是** | 回档负责人先定「可容忍的 DB 故障时长」(例如 2 小时),按 §12 算出 retention.bytes 与盘量后选 b |
| Q-2 | 快照 >约 1MB 在生产端丢弃 | a) 三处同调上限(生产者 `message.max.bytes`、topic `max.message.bytes`、broker / `replica.fetch.max.bytes`)+ TiDB entry 上限同步;b) 限制快照大小 + C++ 计数;c) 维持 | **是** | 先拿 §9 第 8 步的 size 分布再定 |
| Q-3 | 毒丸行(永远插不进的一行)现状是永久卡住整个消费者 | a) 维持(宁可积压);b) 照 Java 逐行重试后进毒丸日志跳过 | **是** | 维持,另加人工跳过工具时再议 |
| Q-4 | 开服期快照策略(例如积压时抽样 LOGIN 快照) | 不做 / 做 | **是** | 不做;容量不足先扩分区 + 批量 |
| Q-5 | 新分区数 S 与换代时机 | S=12(建议)/ 其它;开服前 / 开服后维护窗口 | 否(按 runbook 不丢) | 开服前、S=12,**前提是同批扩 Kafka PVC**(§10 第 0 步:S=12、Z=2 时整机最坏 19GiB 已顶到 20Gi) |
| Q-6 | 是否需要在线换代(双代并行消费,免停服) | 做 / 不做 | 否 | 开服前换代则不需要 |
| Q-7 | 批量开关何时打开(yaml 1→16 等) | Codex §9 全绿 + 压测对比后 | 否 | 按证据打开;前置:G2a-2 的 stress_summarize 审计段(§7)与 Q-9 |
| Q-8 | 一个分区停机是否只停该分区 | 维持全停 / 分区隔离 | 否 | 维持 |
| Q-9 | 【评审补】data-service 内存护栏:Deployment 加 `GOMEMLIMIT` / 提 `limits.memory`(现 512Mi,`data-service.yaml:105-111`) | 加 GOMEMLIMIT(如 limits 的 80%)/ 提 limits / 都不做(批量保持关) | 否(但 OOMKill 会中断 Load/Save) | 按 §9 第 8 步实测 RSS 峰值定;不拍板就不打开批量 |

---

## 16. 「万一全服库成了瓶颈」—— 本项范围内的回答

- 全局库(SnapshotMySQL)同时承载建角同步路径(名字登记、号段)与审计批量写(快照、流水),分在不同连接池但同一台实例的 CPU / IO 上
  (`data_service.yaml:92-110`)。审计是唯一能「慢下来而不伤玩家」的一方:积压只要在保留期内就不丢。
- 本项给出的节流旋钮:`SnapshotMaxInflightFlushes`(每实例同时写几个分区,最小 1)、批量(减少 fsync 次数,
  `mysql.yaml:66-67` 下每条自动提交两次 fsync)。全局库吃紧时把并发压到 1、批调大,lag 上升但同步路径受保护;
  代价是积压逼近 P×512MiB 的删除线(Q-1 决定能退多远)。观测:`flush_seconds` p99 告警是「全局库开始排队」的早期信号。
  【评审 M-3 补】这个旋钮在 Q-1 拍板之前**基本不能用**:按 §12,示例速率下删除线只有几十秒到几分钟,「让审计慢下来」
  几分钟内就会变成「审计被 broker 删掉」。所以开服首日万一全局库吃紧,顺序是:先确认 Q-1 已把 T_safe 拉到小时级 → 再压并发、加批;
  T_safe 不够时,能保同步路径的只剩「开服前就把审计表拆到独立实例」(下一条)或负责人拍板的 Q-4(开服期快照策略)。
- 结构性解法(不在本项):把 `player_snapshot` / `transaction_log` 拆到独立的审计库实例(新 DSN 段),让审计写不与建角共享 IO;
  这是 data_service 配置 + 迁移 + 回档读路径的改动,建议作为 G2b 单独立项,待负责人拍板。
- TiDB 上「库」不是单机瓶颈,但 `player_snapshot` 的 `(zone_id, created_at)` 等单调索引尾部会热(launch_verdict;`tidb-phase1-and-launch-readiness.md` §5),
  批量会把同一时刻的写更集中,需在 TiDB 压测里看 Key Visualizer。

---

## 17. Java 版(AGENTS §12)状态判断

- **性质**:本项全部是服务端内部实现(消费者、指标、部署),不涉及客户端契约(帧、客户端可见 proto、tip、配表),两版不需同批改。
- **Java 现状**:game-data `ConsumerLoop` 已按 poll 批量、一个事务落库、`commitSync`,已有 lag 量表(`ConsumerLoop.java:24-37`、`:303-319`);
  topic `game-player-snapshot-g<N>` 同样 3 分区;生产端 >1000000 字节丢弃并计数(`PARITY.md:70`)。
- **Java 待做(评估)**:① 分区数是否同样换代扩到 S(Java 自己的世代配置与预建);② 单实例单线程处理全部分区的落库上限是否够用
  (与本仓并行度分析同理:min(P, 实例数));③ 告警规则按 Java 指标名另写;④ Q-1 保留字节、Q-2 消息上限的决定两版共用同一结论。
  毒丸语义两版本就不同(Java 跳过、本仓停住,Q-3),PARITY 行需注明。
  ⑤【评审补】Java 的 lag 量表取的是 `consumer.currentLag(tp)`(`ConsumerLoop.java:311-319`),只覆盖本实例当前分到的分区,
  与 kafka-go `Stats().Lag` 同一类盲区(§4.5);也没有「保留期删掉未消费消息」的检测。Java 若共用同一 broker 的 512MiB/分区上限,
  §12 的 T_safe 问题对它同样成立。
- 交付时在 Java 仓 `PARITY.md` 登记一行「G2a 审计管道扩容:mmorpg <commit> 已做 / Java 待评估 + 原因」;本仓 `docs/PROGRESS.md` 条目注明对应 Java 版本号(Docs 阶段统一写)。

---

## 评审处理记录(对抗式评审,2026-10-12;只读代码核对,未编译、未运行)

核对范围:规格引用的全部 `文件:行号` 逐条回到代码(snapshot_consumer.go / consumer.go / start.go / txlog_consumer.go / fakes_test.go /
snapshot_consumer_test.go / snapshot_store.go / mysql.go / config.go / data_service.go / svc/servicecontext.go / metrics.go /
kafkautil/topic_init.go / safego / k8s_deploy.ps1 / k8s_deploy_contract.tests.ps1 / gen_docs_index.py / deploy 清单 / C++ 生产端 / Java ConsumerLoop)。
行号引用除个别 ±2 行外均准确;go-sql-driver v1.9.3(默认 maxAllowedPacket 64MiB、未开 interpolateParams → 大 blob 走 COM_STMT_SEND_LONG_DATA)、
kafka-go v0.4.47、sarama v1.43.1、data_service `go 1.26.5`(双 `%w` 可用)均已确认。

### 必须改(已改进正文)

| # | 问题 | 依据 | 处理 |
|---|---|---|---|
| M-1 | **编译失败**:`SnapshotSink` 加批量方法后,`data_service.go:362` 把静态类型为接口 `svc.SnapshotStore` 的 `svcCtx.SnapshotStore` 传给 `SnapshotSink` 参数,接口方法集不够 → 编译不过;`logic/rollback_recall_test.go:31` 的 `fakeSnapshotStore` 也要跟着补方法 | `svc/servicecontext.go:23-37`;`data_service.go:353-362` | §4.3 新增一段;§7 修改清单加两文件;§9 第 3 步加 logic 包 vet/test |
| M-2 | **U-0 必红**:flush / isolate 第一行就失败时会 `commit(ctx, b.msgs[:0])`;fakeReader 对空调用也记一次 commit(`fakes_test.go:64-72`),`TestSnapshotConsumer_StopsWithoutCommitWhenDBKeepsFailing` 的 `assert.Empty(commitCalls)`(`:129`)失败;真 kafka-go 也会白发空提交 | `TxLogConsumer.commit` 已有 `len(top)==0` 守卫(`txlog_consumer.go:226-229`) | §4.3 `commit` 明确空切片直接返回;§3 注明是 U-0 前提;新增 U-14 |
| M-3 | **告警与「保留期内不丢」的论证在开服速率下失效**:broker 每分区 512MiB 换算成时间,示例速率(128/s、300KB)P=3 只有约 41s、P=12 约 164s;原稿积压告警「>2000 条、for 15m」要在删了十几分钟后才响;§16「放慢审计保护同步路径」在这个窗口下几分钟就变成丢数据;20Gi 盘离真 30 天差几个数量级 | §12 原有公式;`k8s_deploy.ps1:1168-1205`(512MiB / 段 32MiB / 检查间隔 2 分钟);`kafka.yaml:363` | §0 第 4 条、§6 第 5 条、§12 加时间换算;§4.7 LagHigh 改为按删除线时间定阈值、`for: 2m`、critical;Q-1 升为开服前阻塞项并改写选项;§16 加前置条件;R-12 |
| M-4 | **runbook 第 0 步两处算错**:①并行度 min(S,2Z,2Z×M) 多了 2Z(G2a 后一个实例可同时写 M 个分区);②Kafka 盘只算审计 topic,漏了每 zone 10 分区的 db_task —— S=12、T=6、Z=2 时整机最坏 19GiB,已顶到 20Gi PVC,broker 盘满会拖停所有 zone 的存盘 | `db-task-kafka-partition-contract.md`;`kafka.yaml:350-363`(副本 3,每台 broker 持全部分区) | §10 第 0 步、§12 改公式;Q-5 加「同批扩 PVC」前提 |

### 重要(已改进正文)

| # | 问题 | 处理 |
|---|---|---|
| I-1 | 内存低估:批里 msgs 保留 Value 时每条快照两份(Value + decode 拷贝的 Data);`b.msgs[:0]` 截断仍钉住旧载荷;未算 GC 余量;data-service 同进程跑 Load/Save,OOMKill 是玩家面事故 | `add` 剥掉 Value/Key/Headers(保留 Topic);reset 前 `clear`;R-3 升「高」并重算;新增 Q-9(GOMEMLIMIT / limits),Q-7 以它为前置;U-14 ② |
| I-2 | lag 探针:①先读 end 再读 committed 会在繁忙 topic 上周期性误报 committed>end;②`retention_gap` 在消费者跳过并提交后自行归零,丢失没有持久记录;③sarama 调用不收 ctx、默认 30s 超时,进 `consumers.wg` 会拖过 8s 关停宽限;④`groupOffsetReader` 的两个方法分属 Client / ClusterAdmin,原稿没说谁实现;⑤OffsetFetch v2+ 有顶层 Err | §4.5 规定先读 committed、收紧 sarama 超时、Prober 自己实现接口、检查顶层 Err;§4.4 探针不进 wg、`newReader` 配 kafka-go `ErrorLogger`(「skipping from offset」是唯一精确丢失记录,措辞待按源码核对);RetentionGap 告警 `for: 2m`;S-1 加顺序断言与顶层 Err;R-13 |
| I-3 | `flush_seconds` 含信号量排队与 1s 退避,`FlushSlow`「全局库排队」会被本实例并发上限或一次重试触发 | 改为单次 DB 尝试耗时;`ObserveKafkaConsumerFlush` 拆成 `FlushAttempt` + `BatchRows`;§4.6、§4.7 同步 |
| I-4 | 「单条比上限大时一批只有它自己」不成立(加入后才检查),U-4 按原文写会红 | §4.1 表与 U-4 改为「批为空时到达的超大条才独占」 |
| I-5 | isolate 每行只试一次:批级失败若是批本身问题,逐条阶段一次瞬时 1205 / 断连就停机 60s,比现状逐条更容易停,与「不比逐条更容易卡住」的承诺相反 | isolate 改用 `insertWithRetry`;删 `insertOnce`;§5 表、D-8 |
| I-6 | 单测会死等:U-10 在 MaxInflight=1 时 p1 占名额被挡、p0 永远拿不到名额;fake gate 不收 ctx 时 U-10 / U-11 卡住;U-5 第一次调用其实走单行接口 | U-5 / U-10 / U-11 / U-12 写明前提;fakes_test 要求 gate 挡两个方法且感知 ctx |
| I-7 | 直接构造 `SnapshotConsumerConfig{BatchMaxRows:300}` 时 store 每批拒绝 → 每批逐条兜底、静默变慢 | `withDefaults` 钳到 `store.MaxSnapshotBatchRows` 并打 Error |
| I-8 | 没有 stress_summarize 审计段就无法按 AGENTS §6.1 给对比表,却允许打开批量 | G2a-2 定为 Q-7 前置(合入代码不需要) |
| I-10 | txlog 全坏消息批(rows=0)也走 flush,会往 `batch_rows` 塞 0 | 只在 rows>0 时 observe |
| I-15 | 确定性「批太大」错误(1153 / ErrPktTooLarge / TiDB 8004)按 5×1s 退避后才兜底,每批白等约 4s | §4.3 建议直接进 isolate(只认这几个错误号) |

### 小问题(已改)

- §0「7 条告警」与 §4.7 / §7 / R-1 的 8 条不一致 → 改为 8。
- §7 把 I-6 放在 consumer 集成测试、§8 放在 store 集成测试 → 统一为 store I-1…I-6、consumer I-7。
- §3「与现状逐字等价」过强 → 改「语义等价」并列出差异(多分区重叠、错误文本、双 `%w`)。
- `MaxSnapshotBatchMaxBytes` 注释漏了 TiKV `raft-entry-max-size`(默认 8MB);新增 R-14(将来 TiDB 去重形态使批量在 TiDB 生效时须重验)。
- lag 探针是本批唯一默认打开的新行为 → §4.1 说明并登记 D-7。
- `GroupLag.Uncommitted` 无消费者 → 不出指标,只进日志。
- `Snapshot captured ... size=` 是 INFO 且在入队时打 → §4.7 注明压测环境日志级别要求与「入队≠投递」。
- C-2 补「池未配(0)按 5」的边界;runbook 第 6 步补 K8s 上 ConfigMap 重生成与 `AuditTopicGeneration` 核对。
- R-11:单次 DB 尝试无超时(既有,AGENTS §11.3)→ 列为建议,不阻塞本批。
- §17 Java:补 lag 量表的同类盲区与缺少保留期丢失检测。

### 核对后不成立 / 不改

- `insertSnapshotsOnDuplicateKeepSQL(n)` 的 `Grow` 在 n=0 时为负 → 不成立:常量部分远大于减项,总量恒正;且调用方 n≥1。
- 多行 ODKU 的 affected-rows / 批内重复 / id 随行序递增 → 与 MySQL 手册及 `innodb_autoinc_lock_mode=2` 行为一致,原稿结论成立(I-1 / I-3 真库验证保留)。
- rebalance 后旧 worker 迟到提交 → 原稿 §6.1 / R-2 的「只会重放、不会越过未落库前缀」论证成立(同一 worker FIFO,提交只取已落库前缀最高位)。
- `runStop.err` 的并发读写 → worker 在 `wg.Done` 之前 `fail`(defer 次序正确),`firstErr` 在 `wg.Wait` 之后读,有 happens-before,成立。
- T-3 放 store 包测试会不会 import 成环 → 不会:`store` 非测试代码不 import `config`,`config` 也不 import `store`。
- `docs/README.md` 登记位置与格式 → 与 `gen_docs_index.py` 的 `*runbook*` 规则、`str.lower` 排序一致,无需改生成器。
