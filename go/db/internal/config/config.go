package config

import (
	"errors"
	"fmt"
	"strings"

	"github.com/zeromicro/go-zero/zrpc"
)

// Config is the global configuration.
type Config struct {
	zrpc.RpcServerConf
	ZoneId       uint32       `json:"ZoneId"`
	ServerConfig ServerConfig `json:"ServerConfig"`

	// MetricsListenAddr is the bind address for the Prometheus /metrics
	// HTTP endpoint. Leave empty to disable (production default; opt-in
	// via deploy config). Dev default is set in etc/db.yaml so stress
	// runs can scrape per-task stage histograms alongside login :9101
	// and scene_manager :9150.
	MetricsListenAddr string `json:"MetricsListenAddr,optional"`

	// Placement 是按玩家存储落点选库的参数(docs/design/player-storage-placement.md §6)。
	// 整块可缺省,缺省语义见 PlacementConfig。
	Placement PlacementConfig `json:"Placement,optional"`
}

// PlacementConfig 控制 go/db 如何按 player:placement:{id} / player:zone:{id} 选库。
//
// **整块缺省必须等价于 AllowStoreFamilies=true、Required=false**。go-zero 对整段 optional 且
// 未出现的嵌套结构不回填内层 default 标签,所以这里不写 default:
//   - AllowStoreFamilies 用 *bool,nil(没写)按 true 读,经 StoreFamiliesAllowed 取值;
//   - Required 的零值 false 本身就是安全方向(缺席即旧语义,P-5);
//   - 连接池大小由 Normalize 回填。
type PlacementConfig struct {
	// AllowStoreFamilies 为 nil 或 true 时,名字属于 zone_{1..999999}_db / player_store_{>=1000000}_db
	// 两个家族之一的落点库即使不在库名白名单里也允许按需打开(§6.1)。按需打开只 Ping + 只读 schema 闸,
	// 从不建库,白名单「防 ZoneId 填错静默建新库」的语义不受影响。显式 false 时只认白名单。
	AllowStoreFamilies *bool `json:"AllowStoreFamilies,optional"`

	// Required=true:没有落点记录的玩家一律 fail-closed —— 写进死信、读回失败(§6.2)。
	// 打开前提(§13):全部 login 已 PinOnCreate=true,且各 zone 已 -mode pin-placement 钉完存量。
	Required bool `json:"Required,optional"`

	// 额外落点库按家族分两个池旋钮(G2b 批 C;本 zone 库仍用 ServerConfig.Database.MaxOpenConn / MaxIdleConn):
	//   ZoneStore*    —— **每个**额外 zone 家族库 zone_{1..999999}_db(本 zone 库除外):冷缓存访客读、合服 pin 后的源区库;
	//   NonZoneStore* —— **每个**非 zone 落点库 player_store_{>=1000000}_db(Phase 2)。
	// 分开的原因:zone 家族池的预算随 zone 数平方增长,非 zone 池在 Phase 2 是瓶颈要单独调大,一个旋钮两头顾不上。
	// 0 = 回落旧键 ExtraStore*,再回落默认 8 / 2(Normalize;优先级 新键 > 旧键 > 默认)。
	ZoneStoreMaxOpenConn    int `json:"ZoneStoreMaxOpenConn,optional"`
	ZoneStoreMaxIdleConn    int `json:"ZoneStoreMaxIdleConn,optional"`
	NonZoneStoreMaxOpenConn int `json:"NonZoneStoreMaxOpenConn,optional"`
	NonZoneStoreMaxIdleConn int `json:"NonZoneStoreMaxIdleConn,optional"`

	// ExtraStoreConnMaxIdleSeconds 是额外落点库池的 SetConnMaxIdleTime(秒);≤0 视为没写,取默认 60。
	ExtraStoreConnMaxIdleSeconds int `json:"ExtraStoreConnMaxIdleSeconds,optional"`

	// ExtraStoreMaxOpenConn / ExtraStoreMaxIdleConn 是**旧键**:只作上面新键缺省时的回落(两类库都回落到它),
	// 新配置不要再写。Normalize 仍回填它的默认值 8 / 2,让「只写旧键」的旧部署两类库维持原来的大小。
	ExtraStoreMaxOpenConn int `json:"ExtraStoreMaxOpenConn,optional"`
	ExtraStoreMaxIdleConn int `json:"ExtraStoreMaxIdleConn,optional"`

	// Redis 指向落点记录所在的 Redis(与 player:zone 同库:data_service 的 MappingRedis,恒 DB 0)。
	// nil = 复用 ServerConfig.RedisClient。两者不是同一实例 / 同一 DB 时**必须**显式填:
	// 读错库 = 读不到任何记录与 home_zone,全员按本进程 zone 选库,被钉到别处的玩家会写错库。
	// 必须是主节点而不是只读副本:落库后复核(§6.3)要读到搬库工具最新写下的冻结 / 切换。
	Redis *PlacementRedisConfig `json:"Redis,optional"`
}

// PlacementRedisConfig 是落点记录所在 Redis 的连接参数。段出现时 Hosts 必填。
type PlacementRedisConfig struct {
	Hosts    string `json:"Hosts"`
	Password string `json:"Password,optional"`
	DB       int    `json:"DB,optional"`
	// PoolSize 同 RedisConfig.PoolSize(0 / 不写 = go-redis 缺省 10 × GOMAXPROCS,负数拒启)。
	// 只在本段写出、另开一个落点客户端时生效;K8s 的 db ConfigMap 不写本段。
	PoolSize int `json:"PoolSize,optional"`
}

// StoreFamiliesAllowed 返回是否按家族放行落点库;没写(nil)按 true。
func (p PlacementConfig) StoreFamiliesAllowed() bool {
	return p.AllowStoreFamilies == nil || *p.AllowStoreFamilies
}

// ServerConfig holds core service settings.
type ServerConfig struct {
	Database    DatabaseConfig  `json:"Database"`
	RedisClient RedisConfig     `json:"RedisClient"`
	JsonPath    string          `json:"JsonPath"`
	Kafka       KafkaConfig     `json:"Kafka"`
	BlobGuard   BlobGuardConfig `json:"BlobGuard,optional"`
	Fencing     FencingConfig   `json:"Fencing,optional"`
	// OwnerEpochRedis 只读核心实例的 owner_epoch；缺省复用 RedisClient。
	OwnerEpochRedis *PlacementRedisConfig `json:"OwnerEpochRedis,optional"`
}

// 玩家两张表的 MySQL 行内围栏模式(docs/design/db-write-fencing-mysql.md §4.7、§8)。
const (
	// FencingModeLegacy 不拒绝旧写，但更新保留已有行内围栏，也不发布资产 SQL 提交证据。缺省值。
	FencingModeLegacy = "legacy"
	// FencingModeDual 玩家两张表按 DBTask 的 (owner_epoch, save_seq) 做行内围栏条件写,并发布资产 SQL 提交证据。
	FencingModeDual = "dual"
)

// FencingConfig 是行内围栏的开关。整段可缺省:go-zero 对整段 optional 且未出现的嵌套结构不回填 default,
// 所以零值(空串)必须直接就是安全语义 legacy。逐 zone 切 dual,门禁见设计 §8.3;dual 切回 legacy 随时可做,
// legacy 期间更新保留已有围栏，新行盖零；切换仍须满足 §8.3 的滚动门禁。
type FencingConfig struct {
	Mode          string `json:"Mode,optional"`
	EpochOverride bool   `json:"EpochOverride,optional"`
}

// EffectiveMode 返回生效的模式:空串按 legacy。
func (f FencingConfig) EffectiveMode() string {
	if f.Mode == "" {
		return FencingModeLegacy
	}
	return f.Mode
}

// DatabaseConfig holds database connection settings.
type DatabaseConfig struct {
	Hosts       string `json:"Hosts"`
	User        string `json:"User"`
	Passwd      string `json:"Passwd"`
	DBName      string `json:"DBName,optional"` // Derived from ZoneId at startup
	MaxOpenConn int    `json:"MaxOpenConn"`
	MaxIdleConn int    `json:"MaxIdleConn"`
	Net         string `json:"Net"`
	// RequireTiDB 强制每条玩家库物理连接使用 TiDB leader、当前严格读；false 仅兼容本地 MySQL。
	RequireTiDB bool `json:"RequireTiDB,optional"`

	// AllowedDatabases 是**外部注入**的库名白名单。
	//
	// 为什么必须外部注入:DBName 由 ZoneId 现拼(ZoneDBName),而 ZoneId 本身
	// 就是最容易填错的那个字段 —— 填错一位(1 → 11)时旧实现会在生产实例上
	// 静默 CREATE DATABASE + 建全套表,把玩家数据写进一个无人知晓的新库,
	// 全程没有任何报错。用「ZoneId 推导出来的名字」自己校验自己是无效的,
	// 所以白名单必须来自部署侧(env / ConfigMap 文件 / yaml),与 ZoneId 无关。
	//
	// 解析优先级:环境变量 DB_ALLOWED_DATABASES > AllowedDatabasesFile > 本字段。
	AllowedDatabases []string `json:"AllowedDatabases,optional"`

	// AllowedDatabasesFile 指向一份「每行一个库名」的白名单文件(K8s ConfigMap
	// 挂载场景)。`#` 开头与空行忽略。
	AllowedDatabasesFile string `json:"AllowedDatabasesFile,optional"`

	// AllowlistEnforcement 控制「白名单三个来源全为空」时的行为:
	//   ""/"strict" —— 拒启(默认,生产语义)
	//   "warn"      —— 打一条显眼的 WARN 后放行,**仅限本地/dev**
	//
	// 刻意不复用 go-zero 的 Mode 做判据:db.yaml 里没写 Mode 时 go-zero 默认
	// 是 "pro",本地脚本起的服务会被当成生产,踩坑时完全看不出因果。
	AllowlistEnforcement string `json:"AllowlistEnforcement,optional,options=|strict|warn"`

	// AutoCreateDatabase 允许启动路径在库不存在(MySQL 1049)时自动建库。
	// 默认 false。生产由部署阶段的迁移 Job 预建;只有 dev/test 模式下本开关
	// 才会被采纳(见 proto_sql.ddlPolicy)。
	AutoCreateDatabase bool `json:"AutoCreateDatabase,optional"`

	// AutoMigrateSchema 允许启动路径逐张跑 CreateOrUpdateTable(建表 / 补列)。
	// 默认 false。历史行为等价于 true —— 多 zone × 多副本同时启动会对同一张
	// 大表并发 ALTER,MDL 阻塞会让整个 zone 的 db_task 消费停摆。生产改为由
	// `go run ./cmd/migrate -command up` 在部署阶段单独执行。
	// 同样只有 dev/test 模式下本开关才会被采纳。
	AutoMigrateSchema bool `json:"AutoMigrateSchema,optional"`

	// Migration 是迁移入口(cmd/migrate)的超时与锁参数,业务服务不读。
	Migration MigrationConfig `json:"Migration,optional"`

	// 以下五个是业务连接池(本 zone 库与按需打开的落点库)的驱动级超时与会话级锁等待上限
	// (docs/design/db-task-store-unavailable-deferral.md §8)。迁移入口与建库不用这组值:DDL 可能合法地跑几分钟。
	//
	// 写 0 或不写取默认值;写负数表示不设(驱动不加超时 / 不发对应的 SET)。
	//
	// 为什么要求会话级锁等待短于读超时:客户端读超时只关掉自己这一侧的连接,已经发给 MySQL 的语句不会被撤回,
	// 可能在之后才生效,而此时排序锁已经释放、任务已经进了重试队列,先后顺序就乱了。让服务端先以 1205
	// 自行中止排队中的语句,超时之后就不会留下在途语句(条件写落地之前,这是唯一能关掉这个窗口的办法)。
	DialTimeoutSeconds  int `json:"DialTimeoutSeconds,optional"`
	ReadTimeoutSeconds  int `json:"ReadTimeoutSeconds,optional"`
	WriteTimeoutSeconds int `json:"WriteTimeoutSeconds,optional"`
	// SessionLockWaitSeconds 对应 MySQL 的 lock_wait_timeout(元数据锁等待,默认一年)。
	// TiDB 是否接受这个会话变量没有核实;切 TiDB 时若连接因它失败,写 -1 关掉。
	SessionLockWaitSeconds int `json:"SessionLockWaitSeconds,optional"`
	// SessionInnodbLockWaitSeconds 对应 innodb_lock_wait_timeout(行锁等待,MySQL 默认 50 秒)。
	SessionInnodbLockWaitSeconds int `json:"SessionInnodbLockWaitSeconds,optional"`
}

// 业务连接池的超时与锁等待默认值(秒)。会话级锁等待必须短于读超时,理由见 DatabaseConfig 的注释。
const (
	DefaultDatabaseDialTimeoutSeconds           = 5
	DefaultDatabaseReadTimeoutSeconds           = 30
	DefaultDatabaseWriteTimeoutSeconds          = 30
	DefaultDatabaseSessionLockWaitSeconds       = 10
	DefaultDatabaseSessionInnodbLockWaitSeconds = 10
)

// MigrationConfig 是 DDL 迁移的安全阀参数。
//
// 三层保护缺一不可:
//  1. AdvisoryLockSeconds —— GET_LOCK 跨实例互斥,杜绝多 zone/多副本并发 ALTER;
//  2. LockWaitSeconds / InnodbLockWaitSeconds —— 会话级 MDL / 行锁等待上限,
//     让 DDL 撞上长事务时**快速失败**而不是把整表锁死;
//  3. StatementTimeoutSeconds —— 每条语句的硬超时,超时后从旁路连接 KILL QUERY,
//     防止服务端还在跑而工具已经退出。
type MigrationConfig struct {
	LockWaitSeconds         int `json:"LockWaitSeconds,optional"`
	InnodbLockWaitSeconds   int `json:"InnodbLockWaitSeconds,optional"`
	StatementTimeoutSeconds int `json:"StatementTimeoutSeconds,optional"`
	AdvisoryLockSeconds     int `json:"AdvisoryLockSeconds,optional"`
}

// BlobGuardConfig 是 db_task 落库路径上 pb blob 列的保守计费上限(大字段三档闸)。
//
// 档位语义:
//   - 正常          —— 只记 histogram,零日志;
//   - >= WarnRatio  —— WARN 日志 + db_blob_guard_total{level="warn"} 计数;
//   - >  上限       —— db_blob_guard_total{level="reject"} 计数,Enforce 时拒写。
//
// 上限**按设计期望定,不是按 MEDIUMBLOB 的 16MB 定**:玩家单个组件 blob 的
// 设计量级是个位数 KB,16MB 的闸等于没设 —— 等它触发时行早就大到把 Save
// 的网络往返和 Redis 写回一起拖垮了。
type BlobGuardConfig struct {
	// MaxColumnBytes 单个 blob 列的保守计费上限(0 = 用默认值)。
	// 兼容既有 base64.EncodedLen(序列化字节) 口径与指标；当前 proto2mysql v0.2.0
	// 向 SQL 传裸 protobuf/bytes，本预算不是实际入库量，不随本轮列覆盖放宽计费算法。
	MaxColumnBytes int64 `json:"MaxColumnBytes,optional"`

	// MaxRowBytes 单行所有 blob 列保守计费之和的上限(0 = 用默认值)。
	MaxRowBytes int64 `json:"MaxRowBytes,optional"`

	// WarnRatio 预警水位(0 = 用默认值 0.8)。
	WarnRatio float64 `json:"WarnRatio,optional"`

	// ReportOnly 把超限从「拒写」降级为「只报警」。默认 false = fail-closed 拒写。
	// 之所以用反向开关而不是 Enforce:go-zero 对「整段 optional 且未出现」的
	// 嵌套结构不会回填 default 标签,零值必须直接就是我们要的安全语义。
	// 灰度期可显式设 true 先跑 report-only,盯 histogram 的 p99 定完上限再关掉。
	ReportOnly bool `json:"ReportOnly,optional"`

	// PerTable 按表覆盖上限,键是表名(= proto message 名)。
	// 用于 player_snapshot 这类天然就该大的表。
	PerTable map[string]BlobTableLimit `json:"PerTable,optional"`
}

// BlobTableLimit 是单表的上限覆盖,0 表示沿用全局值。
type BlobTableLimit struct {
	MaxColumnBytes int64 `json:"MaxColumnBytes,optional"`
	MaxRowBytes    int64 `json:"MaxRowBytes,optional"`
	// ColumnBytes 只覆盖指定 blob 列，不能顺带放宽同表其他组件。
	ColumnBytes map[string]int64 `json:"ColumnBytes,optional"`
}

// WithDefaults 返回独立配置，统一普通启动与未调用 InitBlobGuard 的保护闸默认值。
// 资产账本保留五流身份和至多 256 KiB 原件；预算沿用 protobuf 的 base64 长度保守计费。
// 显式正值（含更小的限制）原样保留；克隆两层 map，调用方后续修改互不影响。
func (b BlobGuardConfig) WithDefaults() BlobGuardConfig {
	if b.MaxColumnBytes <= 0 {
		b.MaxColumnBytes = DefaultBlobMaxColumnBytes
	}
	if b.MaxRowBytes <= 0 {
		b.MaxRowBytes = DefaultBlobMaxRowBytes
	}
	if b.WarnRatio <= 0 || b.WarnRatio > 1 {
		b.WarnRatio = DefaultBlobWarnRatio
	}
	perTable := make(map[string]BlobTableLimit, len(b.PerTable)+1)
	for table, limit := range b.PerTable {
		columns := make(map[string]int64, len(limit.ColumnBytes))
		for column, bytes := range limit.ColumnBytes {
			columns[column] = bytes
		}
		limit.ColumnBytes = columns
		perTable[table] = limit
	}
	player := perTable["player_database"]
	if player.ColumnBytes == nil {
		player.ColumnBytes = make(map[string]int64, 1)
	}
	if player.ColumnBytes["asset_op_ledger"] <= 0 {
		player.ColumnBytes["asset_op_ledger"] = DefaultAssetLedgerMaxColumnBytes
	}
	perTable["player_database"] = player
	b.PerTable = perTable
	return b
}

// RedisConfig holds Redis connection settings.
type RedisConfig struct {
	Hosts             string `json:"Hosts"`
	DefaultTTLSeconds int    `json:"DefaultTTLSeconds"`
	Password          string `json:"Password"`
	DB                int    `json:"DB"`
	// PoolSize 是 go-redis 连接池上限(共享 Redis 连接总账,P0-E 第 3 批)。0 / 不写 = go-redis 缺省 10 × GOMAXPROCS,
	// 随 CPU limit 漂移;负数起服即失败(svc.NewServiceContext)。K8s 上 tools/scripts/k8s_deploy.ps1 从 etc/db.yaml
	// 镜像 ServerConfig.RedisClient.PoolSize 并要求正整数,Redis 连接总账读同一个键。
	// Placement.Redis 缺省时落点句柄复用本客户端,不另开池。
	PoolSize int `json:"PoolSize,optional"`

	// 以下三个是重试 / 死信 / 毒消息入队的落盘确认(WAITAOF,docs/design/db-retry-owner-routing.md §7)。
	// 只作用于「Kafka 位点提交之前、任务唯一副本首次写进 Redis」的入队点(go/db/internal/kafka/durable_enqueue.go):
	// 确认不足不提交位点(fail-closed,消息重投,由 seq 游标幂等吸收);开启时启动先探一次 WAITAOF,失败拒启
	// (需要 Redis >= 7.2 且 appendonly yes)。确认走专用的小连接池,不占上面的主池。
	//
	// 两个份数用 *int(理由同 go/data_service RouteStoreConfig):默认值 1 不是零值,而「显式写 0」又有合法语义 ——
	// 两项都写 0 = 不发 WAITAOF、退回只 LPUSH(启动打一行 ERROR),只给确实没开 AOF 的环境用。nil(没写)取默认,
	// 由 Normalize 回填;访问一律经 NumLocal / NumReplicas。redis-ha(有副本)接入时 NumReplicas 写 1。
	WaitAOFNumLocal    *int `json:"WaitAOFNumLocal,optional"`    // 0 | 1;nil = 1
	WaitAOFNumReplicas *int `json:"WaitAOFNumReplicas,optional"` // [0, 16];nil = 0
	// WaitAOFTimeoutMs 是单次 WAITAOF 的等待上限(毫秒),[1, 2000];0 / 不写 = 1500,负数拒启。
	// 专用客户端的读超时 = 本值 + 1s。默认不取 1000:appendfsync everysec 下本地确认最长要等到下一次 fsync(约 1s)
	// 再加事件循环延迟,1000 正压在边界上。实际取 min(本值, 本批截止前剩余 − 100ms)。
	WaitAOFTimeoutMs int `json:"WaitAOFTimeoutMs,optional"`
}

// RedisClient 入队落盘确认(WAITAOF)的默认值与上界。
//
// MaxRedisWaitAOFTimeoutMs 取 2000:专用客户端读超时 = 本值 + 1s,再大就拖长单批预算(5s)内的重试轮数,
// 并让停机排空等得更久;everysec 下本地确认最长约 1s,2000 已留出一倍余量。
const (
	DefaultRedisWaitAOFNumLocal    = 1
	DefaultRedisWaitAOFNumReplicas = 0
	DefaultRedisWaitAOFTimeoutMs   = 1500
	MaxRedisWaitAOFNumReplicas     = 16
	MaxRedisWaitAOFTimeoutMs       = 2000
)

// NumLocal 是 WAITAOF 的 numlocal;没写(nil)= 默认 1。
func (r RedisConfig) NumLocal() int {
	if r.WaitAOFNumLocal == nil {
		return DefaultRedisWaitAOFNumLocal
	}
	return *r.WaitAOFNumLocal
}

// NumReplicas 是 WAITAOF 的 numreplicas;没写(nil)= 默认 0。
func (r RedisConfig) NumReplicas() int {
	if r.WaitAOFNumReplicas == nil {
		return DefaultRedisWaitAOFNumReplicas
	}
	return *r.WaitAOFNumReplicas
}

// WaitsForAOF 报告入队之后要不要发 WAITAOF:两项都是 0(显式写的)才不发。
func (r RedisConfig) WaitsForAOF() bool {
	return r.NumLocal() > 0 || r.NumReplicas() > 0
}

// KafkaConfig holds Kafka consumer settings.
type KafkaConfig struct {
	Brokers []string `json:"Brokers"` // Broker addresses
	// GroupID 是消费组名的**最终形状**键(docs/design/db-consumer-group-per-zone.md §2):本地 db.yaml,以及 K8s 上
	// 不在交接中的 ConfigMap(final(L) = db_rpc_consumer_group、final(Z) = db_rpc_consumer_group_z<ZoneId>)都只写它。
	// 交接闸之前的二进制把它当必填(`json:"GroupID"`),所以最终形状任何版本都能加载。
	// 交接形状不写它,只写 ConsumerGroup + GroupHandoffFrom:旧二进制在 conf.MustLoad 就因缺本键拒启,
	// 这是有意的能力证明 —— 没有交接闸的二进制不会以新组名从 Offsets.Initial(oldest)起步全量重放。
	// 与 ConsumerGroup 二选一,见 ValidateConsumerGroup。
	GroupID string `json:"GroupID,optional"`
	// ConsumerGroup 是交接形状的组名键:handoff(X←Y) 写 ConsumerGroup=X、GroupHandoffFrom=Y,且不写 GroupID。
	ConsumerGroup string `json:"ConsumerGroup,optional"`
	// GroupHandoffFrom 非空时启用交接闸(go/db/internal/kafka/group_handoff.go):本组第一次入组前等对方组释放本 topic,
	// 入组后用对方组的已提交位点给本组没有位点的分区播种。只能与 ConsumerGroup 一起用,且不能等于本组名。
	GroupHandoffFrom string `json:"GroupHandoffFrom,optional"`
	// 消费组会话 / 心跳 / 再平衡超时(毫秒)。0 或不写取默认值(= sarama v1.43.1 的默认值,见 DefaultKafka*);
	// 约束见 ValidateConsumerGroup:HeartbeatIntervalMs×3 ≤ SessionTimeoutMs,RebalanceTimeoutMs ≥ MinKafkaRebalanceTimeoutMs。
	SessionTimeoutMs    int    `json:"SessionTimeoutMs,optional"`
	HeartbeatIntervalMs int    `json:"HeartbeatIntervalMs,optional"`
	RebalanceTimeoutMs  int    `json:"RebalanceTimeoutMs,optional"`
	Topic               string `json:"Topic,optional"`               // Derived from ZoneId at startup
	TopicGeneration     uint32 `json:"TopicGeneration,default=1"`    // Immutable routing generation; partition changes require a new topic
	PartitionCnt        int32  `json:"PartitionCnt"`                 // Partition count
	RetentionMs         int64  `json:"RetentionMs,default=86400000"` // Topic retention in ms (default 24h; matches login)
	// P1 数据安全加固 2026-06-03: 旧值 300000 (5min)
	// db service 卡 5min+ 会丢数据,24h 给运维事故充足窗口
	// RetentionBytes 是 db_task topic 每分区的 retention.bytes。K8s 由 k8s_deploy.ps1 按 Kafka 盘预算渲染,
	// 与 login 同值;本机不设 = 沿用 broker 缺省(0 = 不管理,<0 启动拒绝)。
	// 见 docs/design/db-task-delivery-and-retention.md §B。
	RetentionBytes  int64 `json:"RetentionBytes,optional"`
	IsOfflineExpand bool  `json:"IsOfflineExpand"` // Offline expansion: true = maintenance mode

	// SubShardCount controls intra-partition parallelism. 0 or 1 = legacy
	// behaviour (single goroutine per partition, max parallelism =
	// PartitionCnt). N>1 = each partition's worker becomes a router that
	// fans tasks across N sub-goroutines by hash(task.Key), so per-key
	// ordering is preserved while overall throughput multiplies by N.
	//
	// Why this exists: under MaxOpenConn=30 the MySQL pool was massively
	// underused because partition=10 = strictly 10 in-flight queries.
	// With SubShardCount=4, effective parallelism becomes 10×4=40 and the
	// pool actually fills. See 2026-05-28 stress maxopenconn doc §3.
	SubShardCount int `json:"SubShardCount,optional"`

	// RetryInstanceId 是本实例在重试收据登记表里的名字(go/db/internal/kafka/retry_ownership.go)。
	// 留空 = <主机名>:<ListenOn 端口>:K8s 上主机名就是 Pod 名,本机多开时端口不同,通常不需要配置。
	// 两个同时活着的实例**不能**同名:同名被当成"上一次的自己",启动时会收走对方的在途重试收据(重复执行)。
	RetryInstanceId string `json:"RetryInstanceId,optional"`
	// RetryLeaseSeconds 是上面那份登记的租约时长。0 = 默认 30;小于 10 的值按 10 算。
	// 续租与孤儿回收的周期是它的三分之一;崩溃实例的在途重试收据最迟约 4/3 个租约之后回到 ready。
	RetryLeaseSeconds int `json:"RetryLeaseSeconds,optional"`
	// RetryRouting 决定谁执行重试(docs/design/db-retry-owner-routing.md):
	//   shared(缺省)—— 任一实例都可以认领执行,另外每拍兜底排空按分区拆开的 kafka:retry:queue:{topic}:p{N}(回滚路径);
	//   owner —— 重试只在持有原始分区 claim 的实例执行,与该分区的新消息同子 shard 串行;拿不到分区的实例只分拣与回收。
	// 取值大小写敏感,拼错拒启(不静默回落)。环境变量 DB_RETRY_ROUTING(RetryRoutingEnvVar)优先于本键,只用于单 zone
	// 金丝雀;生效值与来源以 ResolveRetryRouting 为准,启动日志 "retry routing: mode=… source=…" 是核对依据。
	// 空串不在 Normalize 里回填,好让启动日志区分 default 与 config。
	RetryRouting string `json:"RetryRouting,optional"`
}

// RetryRouting 的取值与覆盖用的环境变量。
const (
	RetryRoutingShared = "shared"
	RetryRoutingOwner  = "owner"
	RetryRoutingEnvVar = "DB_RETRY_ROUTING"
)

// ResolveRetryRouting 决定生效的重试路由模式与来源(source 只用于启动日志):
//   - env 去掉首尾空白后非空:必须恰为 shared | owner(大小写敏感),source = "env DB_RETRY_ROUTING";
//   - 否则 configured 为空:shared,source = "default";
//   - 否则 configured 必须恰为 shared | owner,source = "config"。
//
// 任何非法值都返回 error,调用方拒启:拼错(Owner、owner 两侧带空格、ownr)被悄悄当成 shared,
// 运维以为切了 owner 而实际没切,这种偏差不报错也没有日志。错误文本英文(启动日志与 K8s 事件里直接看到)。
func ResolveRetryRouting(configured, env string) (mode, source string, err error) {
	if v := strings.TrimSpace(env); v != "" {
		if !validRetryRouting(v) {
			return "", "", fmt.Errorf("environment variable %s=%q must be exactly %q or %q (case-sensitive)",
				RetryRoutingEnvVar, env, RetryRoutingShared, RetryRoutingOwner)
		}
		return v, "env " + RetryRoutingEnvVar, nil
	}
	if configured == "" {
		return RetryRoutingShared, "default", nil
	}
	if !validRetryRouting(configured) {
		return "", "", fmt.Errorf("ServerConfig.Kafka.RetryRouting=%q must be exactly %q or %q (case-sensitive; a typo must not silently fall back to shared)",
			configured, RetryRoutingShared, RetryRoutingOwner)
	}
	return configured, "config", nil
}

func validRetryRouting(mode string) bool {
	return mode == RetryRoutingShared || mode == RetryRoutingOwner
}

var AppConfig Config

// 迁移安全阀默认值。
//
// LockWait / InnodbLockWait 取 5s:DDL 撞上长事务时必须**秒级失败**并退出,
// 而不是把 MDL 排到整表后面 —— 后者会让所有 db_task 的 Save 一起挂住。
// StatementTimeout 取 300s 是给真正在跑的 ALTER 留出余量(表大时正常耗时),
// 超时后由 KILL QUERY 兜底。AdvisoryLock 取 60s:多副本同时部署时后到的那个
// 应该排队等一会儿,而不是立刻失败重启。
const (
	DefaultMigrationLockWaitSeconds       = 5
	DefaultMigrationInnodbLockWaitSeconds = 5
	DefaultMigrationStatementTimeoutSec   = 300
	DefaultMigrationAdvisoryLockSeconds   = 60
)

// 大字段闸默认值。
//
// 256 KiB / 列:玩家单个组件 blob 的设计量级是个位数 KB,256 KiB 已经留了
// 一到两个数量级的余量,同时比 MEDIUMBLOB 的 16 MiB 低 64 倍 —— 只有这样
// 闸门才会在「异常增长」而不是「已经炸了」的时候响。
// 1 MiB / 行:沿用所有 blob 列的 base64 长度保守计费上限，与历史指标可比。
// 当前 SQL 参数是裸字节，不能把此值解释为真实传输量或磁盘占用；本轮不放宽整行预算。
const (
	DefaultBlobMaxColumnBytes int64   = 256 * 1024
	DefaultBlobMaxRowBytes    int64   = 1024 * 1024
	DefaultBlobWarnRatio      float64 = 0.8
	// 只对资产账本列放宽，玩家整行仍沿用原有 1 MiB 总预算。
	DefaultAssetLedgerMaxColumnBytes int64 = 1024 * 1024
)

// 额外落点库连接池默认值(player-storage-placement.md §6.1)。
//
// 每个被按需打开的库各自一个池,库数不设上限,所以单库默认给小。预算按「每个 go/db 实例的额外库数 × 池大小 × 副本数 × zone 数」
// 计入同一台 MySQL 的总账(tools/scripts/k8s_deploy.ps1 Get-MysqlConnectionLedger;mysql.yaml 的 max_connections 按其规划峰值取值)。
// 以前这里写的「MySQL 默认 max_connections=151 下仍留得出余量」不成立(2026-10-12 更正)。
// 两类库各有旋钮(ZoneStore* / NonZoneStore*,缺省都回落到这两个默认值):Phase 2 大部分玩家搬进非 zone 落点库后,
// 那一类池会成为瓶颈,届时按压测结论只调大 NonZoneStore*(先重做总账规划档),不连带放大 zone 家族的访客池。
//
// ExtraStoreConnMaxIdleSeconds 默认 60:冷库(偶发访客读)的池因此回到 0 条,不再按 MaxIdleConn 常驻。
// ConnMaxLifetime 仍是 5 分钟,本值大于 300 等于不生效(空闲连接本来就会在 5 分钟寿命到期时被回收,本值只是让冷池更早归零)。
const (
	DefaultPlacementExtraStoreMaxOpenConn        = 8
	DefaultPlacementExtraStoreMaxIdleConn        = 2
	DefaultPlacementExtraStoreConnMaxIdleSeconds = 60
)

// 消费组超时默认值(毫秒),等于 sarama v1.43.1 的 NewConfig 默认值(Session 10s / Heartbeat 3s / Rebalance 60s),
// 所以不写这三个键的部署与引入它们之前行为相同。
//
// MinKafkaRebalanceTimeoutMs = go/db/internal/kafka.claimDrainTimeout(10s)+ 5s 提交余量:再平衡时 ConsumeClaim
// 最多等在途任务 10s 才返回,之后还要提交位点;再平衡超时短于它,协调者会在本成员还在收尾时把它踢出组,
// 分区被别的成员拿走,本成员收尾的提交作废、任务重放。两处数值要同步改。
const (
	DefaultKafkaSessionTimeoutMs    = 10000
	DefaultKafkaHeartbeatIntervalMs = 3000
	DefaultKafkaRebalanceTimeoutMs  = 60000
	MinKafkaRebalanceTimeoutMs      = 15000
)

// EffectiveGroupID 返回本进程实际加入的消费组:ConsumerGroup 非空取它(交接形状),否则取 GroupID(最终形状)。
// go/db 运行时从不自己拼组名(没有 _z 推导函数):组名一律由配置给出字面量,K8s 由 k8s_deploy.ps1 渲染。
func (k KafkaConfig) EffectiveGroupID() string {
	if k.ConsumerGroup != "" {
		return k.ConsumerGroup
	}
	return k.GroupID
}

// ValidateConsumerGroup 校验消费组相关键的组合与超时约束。调用方须先 Normalize(否则超时键为 0,必然报错)。
//
// 刻意不命名为 Validate:go-zero 的 conf.Load 会在 Normalize 之前自动调用根结构实现的 Validate(),
// 那时超时键还没回填默认值;而且本方法挂在 KafkaConfig 上,也不该让它变成整份配置加载的隐式步骤。
// 错误文本用英文(启动日志与 K8s 事件里直接看到)。
func (k KafkaConfig) ValidateConsumerGroup() error {
	switch {
	case k.GroupID == "" && k.ConsumerGroup == "":
		return errors.New("Kafka.GroupID or Kafka.ConsumerGroup must be set")
	case k.GroupID != "" && k.ConsumerGroup != "":
		return errors.New("set only one of Kafka.GroupID and Kafka.ConsumerGroup")
	case k.GroupHandoffFrom != "" && k.ConsumerGroup == "":
		return errors.New("Kafka.GroupHandoffFrom requires Kafka.ConsumerGroup (without Kafka.GroupID) so that binaries without the handoff gate refuse to start")
	case k.GroupHandoffFrom != "" && k.GroupHandoffFrom == k.EffectiveGroupID():
		return errors.New("Kafka.GroupHandoffFrom must differ from the consumer group")
	}
	if k.HeartbeatIntervalMs*3 > k.SessionTimeoutMs {
		return fmt.Errorf("Kafka.HeartbeatIntervalMs (%d) x 3 must not exceed Kafka.SessionTimeoutMs (%d)",
			k.HeartbeatIntervalMs, k.SessionTimeoutMs)
	}
	if k.RebalanceTimeoutMs < MinKafkaRebalanceTimeoutMs {
		return fmt.Errorf("Kafka.RebalanceTimeoutMs (%d) must be at least %d (claim drain timeout 10s + 5s commit margin)",
			k.RebalanceTimeoutMs, MinKafkaRebalanceTimeoutMs)
	}
	return nil
}

// Normalize 回填零值默认项。
//
// go-zero 对「标了 optional 且 yaml 里整段缺失」的嵌套结构不会走 default 标签,
// 所以默认值统一在这里补,而不是散在 tag 里 —— 否则「配了半段」和「完全没配」
// 会走出两套不同的默认值。可重复调用。
func (c *Config) Normalize() {
	m := &c.ServerConfig.Database.Migration
	if m.LockWaitSeconds <= 0 {
		m.LockWaitSeconds = DefaultMigrationLockWaitSeconds
	}
	if m.InnodbLockWaitSeconds <= 0 {
		m.InnodbLockWaitSeconds = DefaultMigrationInnodbLockWaitSeconds
	}
	if m.StatementTimeoutSeconds <= 0 {
		m.StatementTimeoutSeconds = DefaultMigrationStatementTimeoutSec
	}
	if m.AdvisoryLockSeconds <= 0 {
		m.AdvisoryLockSeconds = DefaultMigrationAdvisoryLockSeconds
	}

	// 业务连接池:0 取默认,负数保留(表示不设),所以这里只补 0。
	d := &c.ServerConfig.Database
	if d.DialTimeoutSeconds == 0 {
		d.DialTimeoutSeconds = DefaultDatabaseDialTimeoutSeconds
	}
	if d.ReadTimeoutSeconds == 0 {
		d.ReadTimeoutSeconds = DefaultDatabaseReadTimeoutSeconds
	}
	if d.WriteTimeoutSeconds == 0 {
		d.WriteTimeoutSeconds = DefaultDatabaseWriteTimeoutSeconds
	}
	if d.SessionLockWaitSeconds == 0 {
		d.SessionLockWaitSeconds = DefaultDatabaseSessionLockWaitSeconds
	}
	if d.SessionInnodbLockWaitSeconds == 0 {
		d.SessionInnodbLockWaitSeconds = DefaultDatabaseSessionInnodbLockWaitSeconds
	}

	c.ServerConfig.BlobGuard = c.ServerConfig.BlobGuard.WithDefaults()

	p := &c.Placement
	// 旧键先补默认:它是新键缺省时的回落值,也让「只写旧键」的旧部署两类库维持原来的大小。
	if p.ExtraStoreMaxOpenConn <= 0 {
		p.ExtraStoreMaxOpenConn = DefaultPlacementExtraStoreMaxOpenConn
	}
	if p.ExtraStoreMaxIdleConn <= 0 {
		p.ExtraStoreMaxIdleConn = DefaultPlacementExtraStoreMaxIdleConn
	}
	// 新键缺省回落旧键(优先级 新键 > 旧键 > 默认)。不新增拒启校验:idle > open 由 database/sql 静默压到 open,
	// 与旧键的行为相同,不为此引入新的启动失败。
	if p.ZoneStoreMaxOpenConn <= 0 {
		p.ZoneStoreMaxOpenConn = p.ExtraStoreMaxOpenConn
	}
	if p.ZoneStoreMaxIdleConn <= 0 {
		p.ZoneStoreMaxIdleConn = p.ExtraStoreMaxIdleConn
	}
	if p.NonZoneStoreMaxOpenConn <= 0 {
		p.NonZoneStoreMaxOpenConn = p.ExtraStoreMaxOpenConn
	}
	if p.NonZoneStoreMaxIdleConn <= 0 {
		p.NonZoneStoreMaxIdleConn = p.ExtraStoreMaxIdleConn
	}
	if p.ExtraStoreConnMaxIdleSeconds <= 0 {
		p.ExtraStoreConnMaxIdleSeconds = DefaultPlacementExtraStoreConnMaxIdleSeconds
	}

	// 消费组超时:≤0 视为没写,取默认值(约束由 ValidateConsumerGroup 检查,这里不改写显式值)。
	k := &c.ServerConfig.Kafka
	if k.SessionTimeoutMs <= 0 {
		k.SessionTimeoutMs = DefaultKafkaSessionTimeoutMs
	}
	if k.HeartbeatIntervalMs <= 0 {
		k.HeartbeatIntervalMs = DefaultKafkaHeartbeatIntervalMs
	}
	if k.RebalanceTimeoutMs <= 0 {
		k.RebalanceTimeoutMs = DefaultKafkaRebalanceTimeoutMs
	}

	// 入队落盘确认(WAITAOF):两个份数 nil(没写)时回填默认;TimeoutMs 只把 0 换成默认,负数留给 Validate 拒绝。
	// 只在 nil 时换成新指针,不改调用方指向的值。RetryRouting 不在这里回填(见字段注释)。
	r := &c.ServerConfig.RedisClient
	if r.WaitAOFNumLocal == nil {
		v := DefaultRedisWaitAOFNumLocal
		r.WaitAOFNumLocal = &v
	}
	if r.WaitAOFNumReplicas == nil {
		v := DefaultRedisWaitAOFNumReplicas
		r.WaitAOFNumReplicas = &v
	}
	if r.WaitAOFTimeoutMs == 0 {
		r.WaitAOFTimeoutMs = DefaultRedisWaitAOFTimeoutMs
	}
}

// Validate 校验重试路由与入队落盘确认的取值,任何一项非法都返回 error(调用方拒启),错误文本英文并点名键。
//
// 两个调用点,本方法对两者都安全:
//   - go-zero 的 conf.Load 在 Normalize **之前**自动调用根结构的 Validate()(见 ValidateConsumerGroup 的注释)。
//     所以这里把「没写」当成合法:RetryRouting 空串 = shared,两个份数 nil = 默认,WaitAOFTimeoutMs 0 = 默认 1500;
//     只拒绝写出来的非法值。否则还没镜像新键的 ConfigMap 与旧版 yaml 会在加载时就被拒。
//   - 启动路径在 Normalize 与 ResolveRetryRouting 之后显式再调一次(db.go)。
//
// Config 内嵌的 zrpc.RpcServerConf 自带 Validate(Auth 打开时校验 Redis 段);本方法会遮蔽它,所以先原样调一次,
// 保持 conf.Load 原有的校验不丢。不在这里调 ValidateConsumerGroup:它要求先 Normalize。
func (c *Config) Validate() error {
	if err := c.RpcServerConf.Validate(); err != nil {
		return err
	}
	if pr := c.ServerConfig.OwnerEpochRedis; pr != nil {
		if strings.TrimSpace(pr.Hosts) == "" || pr.PoolSize < 0 || pr.DB < 0 {
			return errors.New("ServerConfig.OwnerEpochRedis连接配置无效")
		}
	}
	if mode := c.ServerConfig.Fencing.Mode; mode != "" && mode != FencingModeLegacy && mode != FencingModeDual {
		return fmt.Errorf("ServerConfig.Fencing.Mode=%q must be exactly %q or %q (case-sensitive; empty means %q)",
			mode, FencingModeLegacy, FencingModeDual, FencingModeLegacy)
	}
	if routing := c.ServerConfig.Kafka.RetryRouting; routing != "" && !validRetryRouting(routing) {
		return fmt.Errorf("ServerConfig.Kafka.RetryRouting=%q must be exactly %q or %q (case-sensitive; empty means %q)",
			routing, RetryRoutingShared, RetryRoutingOwner, RetryRoutingShared)
	}
	r := c.ServerConfig.RedisClient
	if local := r.NumLocal(); local != 0 && local != 1 {
		return fmt.Errorf("ServerConfig.RedisClient.WaitAOFNumLocal must be 0 or 1, got %d", local)
	}
	if replicas := r.NumReplicas(); replicas < 0 || replicas > MaxRedisWaitAOFNumReplicas {
		return fmt.Errorf("ServerConfig.RedisClient.WaitAOFNumReplicas must be in [0, %d], got %d",
			MaxRedisWaitAOFNumReplicas, replicas)
	}
	// 0 = 没写(Normalize 之后是默认 1500);WAITAOF 的 timeout 0 在 Redis 里是无限阻塞,所以 Normalize 必须先换掉它。
	if t := r.WaitAOFTimeoutMs; t != 0 && (t < 1 || t > MaxRedisWaitAOFTimeoutMs) {
		return fmt.Errorf("ServerConfig.RedisClient.WaitAOFTimeoutMs must be in [1, %d] (0 or unset = %d), got %d",
			MaxRedisWaitAOFTimeoutMs, DefaultRedisWaitAOFTimeoutMs, t)
	}
	return nil
}

// DbTaskTopic returns the zone-specific Kafka topic for DB tasks.
func DbTaskTopic(zoneId uint32) string {
	return fmt.Sprintf("db_task_zone_%d", zoneId)
}

func DbTaskTopicForGeneration(zoneId, generation uint32) string {
	base := DbTaskTopic(zoneId)
	if generation <= 1 {
		return base
	}
	return fmt.Sprintf("%s_g%d", base, generation)
}

// ZoneDBName returns the zone-specific MySQL database name.
// 与 shared/placement.StoreDBName 的 zone 家族同规则:落点库注册表把本 zone 库登记为落点 ZoneId,
// 两边拼出的名字不一致时 proto_sql.NewStoreRegistry 拒启。
func ZoneDBName(zoneId uint32) string {
	return fmt.Sprintf("zone_%d_db", zoneId)
}
