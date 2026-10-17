// Package svc 是 player_inbox 的依赖装配:etcd 客户端、mmorpg_player_inbox 连接池、核心 Redis、
// Kafka 生产者、scene 节点镜像、data_service 客户端,以及 inbox 的 Store / 在线提醒 / 后台循环。
package svc

import (
	"context"
	"database/sql"
	"fmt"
	"os"
	"sync"
	"time"

	"player_inbox/generated/pb/game"
	"player_inbox/internal/config"
	"player_inbox/internal/constants"
	"player_inbox/internal/inbox"

	dspb "proto/data_service"

	"shared/kafkacmd"
	"shared/leader"
	"shared/safego"
	"shared/scenenode"

	_ "github.com/go-sql-driver/mysql"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/redis/go-redis/v9"
	"github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/zrpc"
	clientv3 "go.etcd.io/etcd/client/v3"
	"google.golang.org/grpc"
)

const (
	etcdDialTimeout      = 5 * time.Second
	mysqlPingTimeout     = 5 * time.Second
	coreRedisPingTimeout = 3 * time.Second
	// loopsStopTimeout:关停时等后台循环退出的上限(设计 §10:有界等待,再关连接池)。
	// 单轮各自有超时、所有 SQL 都带 ctx,取消即返回;卸任释放选主锁自带 2s 预算。取 2s 是为了让
	// 注销 15s + 排空 5s + 本项 2s + 框架收尾 2s 不超过 lifecycle.HardTimeout(24s);超时只告警,
	// 最坏后果是选主锁没来得及释放,接任者多等一个 TTL。
	loopsStopTimeout = 2 * time.Second

	// MaintenanceLeaderKey 是后台循环选主锁(核心 Redis)。
	MaintenanceLeaderKey = "player_inbox:maintenance:leader"
	// sceneWatcherName 进 scenenode_nodes{kind} 这一个 label。
	sceneWatcherName = "scene"
)

// ServiceContext 持有全部依赖。构造后只读;Start / Stop 各只生效一次。
type ServiceContext struct {
	Config config.Config

	// Etcd 供 noderegistry 注册、scene 节点镜像与 killswitch 共用(Etcd.Key 为空,go-zero 不写自己的 key)。
	Etcd *clientv3.Client
	// DB 是 mmorpg_player_inbox 的连接池(设计 §10 的独立连接池)。表由 schemamigrate 建,本包不碰 DDL。
	DB *sql.DB
	// CoreRedis 是核心 Redis(owner_epoch / location / 选主锁)。
	CoreRedis *redis.Client
	// Kafka 写在线提醒(scene-cmd_g<N>,显式分区)。
	Kafka *kafka.Writer
	// SceneNodes 是 SceneNodeService.rpc/ 前缀的节点镜像。
	SceneNodes *scenenode.Watcher
	// DataService 玩家存在校验与对账读账本。
	DataService dspb.DataServiceClient

	Registry *inbox.Registry
	Store    *inbox.Store
	// Ledgers 是对账读已落盘账本的适配器(运维 ReconcilePlayer 与定时对账共用)。
	Ledgers inbox.LedgerReader
	Loops   *inbox.Loops

	// dataServiceConn 是 DataService 底下的 gRPC 连接,Stop 时关闭。
	dataServiceConn *grpc.ClientConn
	// notifier 是装进 Store 的在线提醒器(装配单测核对它拿到的生成事件号)。
	notifier *inbox.KafkaNotifier
	// background 是 Start 起的后台任务(scene 节点镜像、选主后台循环);装配时填好,单测可以换成假任务。
	background []backgroundTask
	// stopTimeout 是 Stop 等后台任务退出的上限(生产为 loopsStopTimeout)。
	stopTimeout time.Duration

	startOnce sync.Once
	stopOnce  sync.Once
	mu        sync.Mutex
	stopped   bool
	cancel    context.CancelFunc
	wg        sync.WaitGroup
}

// backgroundTask 是 Start 起、Stop 取消并有界等待的一项后台任务。run 必须在 ctx 结束后返回。
type backgroundTask struct {
	name string // safego 的标识(panic 日志里看得到)
	run  func(ctx context.Context)
}

// externalDeps 是 NewServiceContext 建外部连接的入口(AGENTS §11.2 显式依赖)。生产为 productionDeps();
// 单测注入假实现,只验证装配顺序、失败时的回收与接线,不连真实的 etcd / MySQL / 核心 Redis / data_service。
type externalDeps struct {
	openEtcd  func(hosts []string) (*clientv3.Client, error)
	openMySQL func(config.MySQLConf) (*sql.DB, error)
	// openCoreRedis 建核心 Redis 客户端并探活;返回错误时自己负责关掉已建的客户端。
	openCoreRedis   func(config.RedisConf) (*redis.Client, error)
	openDataService func(zrpc.RpcClientConf) (*grpc.ClientConn, error)
	// registerer 是 scene 节点镜像指标(scenenode_nodes)的注册表。
	registerer prometheus.Registerer
}

// productionDeps 是生产用的外部连接入口。
func productionDeps() externalDeps {
	return externalDeps{
		openEtcd: func(hosts []string) (*clientv3.Client, error) {
			return clientv3.New(clientv3.Config{Endpoints: hosts, DialTimeout: etcdDialTimeout})
		},
		openMySQL:     OpenMySQL,
		openCoreRedis: openCoreRedis,
		openDataService: func(c zrpc.RpcClientConf) (*grpc.ClientConn, error) {
			// 不用 MustNewClient:它在配置错误时直接退出进程,跳过本函数「返回错误、关闭已建资源」的契约。
			cli, err := zrpc.NewClient(c)
			if err != nil {
				return nil, err
			}
			return cli.Conn(), nil
		},
		registerer: prometheus.DefaultRegisterer,
	}
}

// openCoreRedis 建核心 Redis 客户端并 Ping。探不通就不起服:R 读不到时所有需要上界的 Fetch / Ack 都回
// StoreUnavailable,提醒也发不出。错误里的地址只含 host:port,不含密码。
func openCoreRedis(c config.RedisConf) (*redis.Client, error) {
	client := redis.NewClient(&redis.Options{
		Addr:     c.Host,
		Password: c.Password,
		DB:       c.DB,
		// 共享 Redis 连接总账按它记账(0 = go-redis 缺省 10 × GOMAXPROCS;负数已被 config.Validate 拒绝)。
		PoolSize: c.PoolSize,
	})
	pingCtx, cancel := context.WithTimeout(context.Background(), coreRedisPingTimeout)
	err := client.Ping(pingCtx).Err()
	cancel()
	if err != nil {
		_ = client.Close()
		return nil, fmt.Errorf("ping core Redis %s (DB %d) failed (owner_epoch / location live there and "+
			"must be the same instance as C++ scene / scene_manager): %w", c.Host, c.DB, err)
	}
	// 生效值(缺省时是 go-redis 算出来的 10 × GOMAXPROCS):核对 ConfigMap 镜像的 PoolSize 是否真的被加载。
	logx.Infof("Core Redis client %s DB=%d pool_size=%d", c.Host, c.DB, client.Options().PoolSize)
	return client, nil
}

// NewServiceContext 建全部依赖。任何一项建不出来都返回错误(启动致命);已建的资源在返回前关闭。
func NewServiceContext(c config.Config) (*ServiceContext, error) {
	return newServiceContext(c, productionDeps())
}

func newServiceContext(c config.Config, deps externalDeps) (*ServiceContext, error) {
	sc := &ServiceContext{Config: c, stopTimeout: loopsStopTimeout}
	ok := false
	defer func() {
		if !ok {
			sc.closeResources()
		}
	}()

	// 先做不连任何东西的自检:注册表与白名单不对时,一条外部连接都不该建。
	registry, err := inbox.NewRegistry()
	if err != nil {
		return nil, fmt.Errorf("player_inbox: type registry self-check failed: %w", err)
	}
	sc.Registry = registry
	allowed, err := config.ParseEnqueueTypes(c.PlayerInbox.EnqueueTypes)
	if err != nil {
		return nil, err
	}

	sc.Etcd, err = deps.openEtcd(c.Etcd.Hosts)
	if err != nil {
		return nil, fmt.Errorf("player_inbox: create etcd client: %w", err)
	}

	sc.DB, err = deps.openMySQL(c.MySQL)
	if err != nil {
		return nil, fmt.Errorf("player_inbox: %w", err)
	}

	sc.CoreRedis, err = deps.openCoreRedis(c.CoreRedis)
	if err != nil {
		return nil, fmt.Errorf("player_inbox: %w", err)
	}

	sc.Kafka = &kafka.Writer{
		Addr: kafka.TCP(c.Kafka.Brokers...),
		// scene-cmd_gN 必须按 node_id % P 落到**指定分区**:消费端 assign 的就是那一个分区,落错分区 = 目标 scene
		// 永远收不到且 Kafka 一个错都不报;kafka-go 的 Writer 只问 Balancer,所以必须用 CommandPartitionBalancer
		// (照 go/match;docs/design/control-plane-topic-partitioning-20260908.md)。
		Balancer:               &kafkacmd.CommandPartitionBalancer{Fallback: &kafka.Hash{}},
		AllowAutoTopicCreation: false,
		BatchTimeout:           10 * time.Millisecond,
		MaxAttempts:            1,
		WriteTimeout:           time.Duration(c.Kafka.WriteTimeoutMs) * time.Millisecond,
		RequiredAcks:           kafka.RequireOne,
		Async:                  false,
	}

	nodeMetrics := scenenode.NewMetrics(deps.registerer, "player_inbox")
	sc.SceneNodes = scenenode.NewWatcher(sceneWatcherName, scenenode.SceneNodeRpcPrefix, nodeMetrics.SetNodes, nil)

	sc.dataServiceConn, err = deps.openDataService(c.DataServiceRpc)
	if err != nil {
		return nil, fmt.Errorf("player_inbox: create data_service client: %w", err)
	}
	sc.DataService = dspb.NewDataServiceClient(sc.dataServiceConn)

	// 事件号来自 player_inbox/generated/pb/game(proto-gen 产物,随仓库跟踪):干净检出不跑生成也能编译。
	sc.notifier = &inbox.KafkaNotifier{
		Locations: scenenode.GoRedisReader{Client: sc.CoreRedis},
		Nodes:     sc.SceneNodes,
		Writer:    sc.Kafka,
		EventID:   uint32(game.PlayerInboxNotifyEventEventId),
	}
	sc.Store, err = inbox.NewStore(inbox.StoreDeps{
		DB:       sc.DB,
		Registry: registry,
		Allowed:  allowed,
		Players:  &DataServicePlayers{Client: sc.DataService, Timeout: constants.HomeZoneLookupTimeout},
		Epochs:   &RedisEpochs{Client: sc.CoreRedis},
		Notifier: sc.notifier,
		Now:      time.Now,
	})
	if err != nil {
		return nil, fmt.Errorf("player_inbox: build store: %w", err)
	}
	sc.Ledgers = &DataServiceLedgers{Client: sc.DataService, Timeout: constants.LedgerReadTimeout}

	elector := leader.New(leader.NewGoRedisStore(sc.CoreRedis), MaintenanceLeaderKey, leader.Options{ID: leaderID()})
	sc.Loops, err = inbox.NewLoops(sc.Store, sc.Ledgers, loopConfigFrom(c.PlayerInbox), elector.Run, time.Now)
	if err != nil {
		return nil, fmt.Errorf("player_inbox: build maintenance loops: %w", err)
	}
	sc.background = []backgroundTask{
		{name: "player_inbox.scenenode.watcher", run: func(ctx context.Context) { sc.SceneNodes.Run(ctx, sc.Etcd) }},
		{name: "player_inbox.maintenance", run: sc.Loops.Run},
	}

	logx.Infof("[player_inbox] Enqueue allowlist: %d type(s) enabled %v (empty = all disabled, design §3.3)",
		len(allowed), c.PlayerInbox.EnqueueTypes)
	ok = true
	return sc, nil
}

// loopConfigFrom 把配置换算成 inbox.LoopConfig。合法区间由 config.Validate 先拦,inbox.NewLoops 再兜底。
func loopConfigFrom(p config.PlayerInboxConf) inbox.LoopConfig {
	return inbox.LoopConfig{
		TerminalRetention: time.Duration(p.TerminalRetentionDays) * 24 * time.Hour,
		SweepInterval:     time.Duration(p.SweepIntervalSeconds) * time.Second,
		NotifyInterval:    time.Duration(p.NotifyResendIntervalSeconds) * time.Second,
		GaugeInterval:     time.Duration(p.GaugeIntervalSeconds) * time.Second,
		ReconcileInterval: time.Duration(p.ReconcileIntervalMinutes) * time.Minute,
		ReconcileWindow:   time.Duration(p.ReconcileWindowHours) * time.Hour,
	}
}

// leaderID 是选主竞选者标识,只进锁值前缀与日志。注册 etcd 之前就要竞选,所以用主机名(K8s 下即 Pod 名)。
func leaderID() string {
	if host, err := os.Hostname(); err == nil && host != "" {
		return host
	}
	return "player_inbox"
}

// Start 起后台任务:scene 节点镜像与后台循环(选主 → 任期内跑循环)。ctx 取消或 Stop 即退出。
// 重复调用只生效一次;Stop 之后再 Start 不起任何任务。
func (sc *ServiceContext) Start(ctx context.Context) {
	sc.startOnce.Do(func() {
		sc.mu.Lock()
		defer sc.mu.Unlock()
		if sc.stopped {
			return
		}
		loopCtx, cancel := context.WithCancel(ctx)
		sc.cancel = cancel
		for _, task := range sc.background {
			sc.wg.Add(1)
			safego.Go(task.name, func() {
				defer sc.wg.Done()
				task.run(loopCtx)
			})
		}
	})
}

// Stop 先取消后台任务与选举并有界等待它们退出(选主卸任时释放锁要用核心 Redis),再关 Kafka / Redis /
// data_service / DB / etcd。等不到也照常关连接:最坏后果是选主锁没来得及释放,接任者多等一个 TTL。
// 幂等。调用方须保证 noderegistry.Close() 与 killswitch 的 ctx 取消都**先于**它(共用 etcd 连接)。
func (sc *ServiceContext) Stop() {
	sc.stopOnce.Do(func() {
		sc.mu.Lock()
		sc.stopped = true
		cancel := sc.cancel
		sc.mu.Unlock()
		if cancel != nil {
			cancel()
			timeout := sc.stopTimeout
			if timeout <= 0 {
				timeout = loopsStopTimeout
			}
			done := make(chan struct{})
			go func() { sc.wg.Wait(); close(done) }()
			select {
			case <-done:
			case <-time.After(timeout):
				logx.Errorf("[player_inbox] Background loops did not stop within %v; closing connections anyway", timeout)
			}
		}
		sc.closeResources()
	})
}

func (sc *ServiceContext) closeResources() {
	if sc.Kafka != nil {
		if err := sc.Kafka.Close(); err != nil {
			logx.Errorf("[player_inbox] Kafka writer close/flush failed: %v", err)
		}
	}
	if sc.CoreRedis != nil {
		if err := sc.CoreRedis.Close(); err != nil {
			logx.Errorf("[player_inbox] core Redis close failed: %v", err)
		}
	}
	if sc.dataServiceConn != nil {
		if err := sc.dataServiceConn.Close(); err != nil {
			logx.Errorf("[player_inbox] data_service connection close failed: %v", err)
		}
	}
	if sc.DB != nil {
		if err := sc.DB.Close(); err != nil {
			logx.Errorf("[player_inbox] MySQL close failed: %v", err)
		}
	}
	if sc.Etcd != nil {
		if err := sc.Etcd.Close(); err != nil {
			logx.Errorf("[player_inbox] etcd client close failed: %v", err)
		}
	}
}

// BuildDSN 拼 mmorpg_player_inbox 的 DSN(与 go/trade BuildDSN 同口径,另加锁等待上限):
//   - sql_mode=STRICT_TRANS_TABLES:自由文本超长时报错而不是静默截断(入箱校验之外的第二道防线);
//   - transaction_isolation=READ-COMMITTED:事务之外的 autocommit 语句(补建流行、清理、提醒 CAS)也走 RC,
//     不在 RR 下拿间隙锁;显式事务本来就经 WithTxRetryConfig 固定 RC(设计 §10);
//   - innodb_lock_wait_timeout=1:锁等待上限 1 秒(设计 §10,照 data_service 的 route store),
//     等不到即 1205,由注入的可重试判定有界重试,不让请求在锁上耗光预算。
//
// 返回值含密码,只能交给 sql.Open,绝不打日志。
func BuildDSN(c config.MySQLConf) string {
	return fmt.Sprintf("%s:%s@tcp(%s)/%s?parseTime=true&charset=utf8mb4&sql_mode=%%27STRICT_TRANS_TABLES%%27"+
		"&transaction_isolation=%%27READ-COMMITTED%%27&innodb_lock_wait_timeout=1",
		c.User, c.Password, c.Host, c.DBName)
}

// MySQLTarget 是连接目标的可读描述(不含密码),供横幅与错误信息使用。
func MySQLTarget(c config.MySQLConf) string {
	return fmt.Sprintf("%s/%s (user=%s)", c.Host, c.DBName, c.User)
}

// OpenMySQL 建池、设上限、ping。返回错误时池已关闭;错误信息点名库名(D-14 第 5 条)。
func OpenMySQL(c config.MySQLConf) (*sql.DB, error) {
	db, err := sql.Open("mysql", BuildDSN(c))
	if err != nil {
		return nil, fmt.Errorf("open MySQL %s: %w", MySQLTarget(c), err)
	}
	db.SetMaxOpenConns(c.MaxOpenConn)
	db.SetMaxIdleConns(c.MaxIdleConn)

	ctx, cancel := context.WithTimeout(context.Background(), mysqlPingTimeout)
	defer cancel()
	if err := db.PingContext(ctx); err != nil {
		_ = db.Close()
		return nil, fmt.Errorf("ping MySQL %s failed (create database %s and grant access first, see deploy/mysql-init): %w",
			MySQLTarget(c), c.DBName, err)
	}
	return db, nil
}
