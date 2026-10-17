// rank 排行榜服务 v1:match 评分全服榜的读(GetRankPage)与写(消费 match 的评分快照)
// (设计 docs/design/leaderboard-system.md)。
//
// 全局一份、多副本、进程无状态:榜在 RankRedis(lb:{rating}:z / lb:{rating}:v,§3.2),写分只经 Kafka 消费
// match-rating-snapshot(消费组 rank-rating,§2.4),客户端只经 gate → 路由服到达唯一的读方法(D-12)。
// 注册按 C++ NodeInfo 约定写进 etcd(shared/noderegistry),路由服按 RankNodeService.rpc 前缀发现本服务(契约 §2/§3)。
//
// 业务代码不读 cfg.ZoneId 做分支(全局服务):ZoneId 只影响 etcd 注册路径。
// v1 零 MySQL(§5.3):没有 -migrate 形态,也没有建表步骤。
package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"os/signal"
	"strconv"
	"sync"
	"syscall"
	"time"

	"rank/internal/config"
	"rank/internal/constants"
	"rank/internal/data"
	rankkafka "rank/internal/kafka"
	"rank/internal/lifecycle"
	"rank/internal/logic"
	"rank/internal/metrics"
	"rank/internal/server"
	"rank/internal/session"
	"rank/internal/svc"

	base "proto/common/base"
	rankpb "proto/rank"

	"shared/buildinfo"
	"shared/grpcstats"
	"shared/killswitch"
	"shared/noderegistry"
	"shared/safego"
	"shared/serverbase"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/netx"
	"github.com/zeromicro/go-zero/core/proc"
	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
	"google.golang.org/protobuf/encoding/protojson"
)

var (
	configFile  = flag.String("f", "etc/rank.yaml", "the config file")
	showVersion = flag.Bool("version", false, "打印版本信息并退出")
)

// nodeType 本服务的节点类型。RankNodeService = 13 早已在 node.proto 里,proto 不改(rank.proto 的目录名就是为了回落到它)。
const nodeType = base.ENodeType_RankNodeService

const (
	// listenProbeTimeout:等本进程 gRPC 端口可连的上限。先探端口再注册,不让路由服在监听建立前就选中本节点。
	listenProbeTimeout = 30 * time.Second
	// registerTimeout:注册整体(探端口 + etcd CAS)的上限,须大于 listenProbeTimeout。
	registerTimeout = 45 * time.Second
	// consumerRetryInterval:快照消费者起不来(Kafka 暂不可达、分区契约不符)时的后台重试间隔(与 match 的结果消费者同口径)。
	consumerRetryInterval = 30 * time.Second
	// consumerStopTimeout:停机时等消费 goroutine 离开消费组(reader.Close)的上限。正在写 Redis 的那一条最多再等
	// go-redis ReadTimeout(3s),离组本身一个往返。等不到就照常退出,没有数据风险:未提交的那一条会被重放,
	// 写榜的版本判定让它落成 stale;代价只是其余副本要等会话超时才接管这些分区。
	consumerStopTimeout = 4 * time.Second
	// consumerStopBudget:从开始收尾算起,等消费者最晚等到的时刻。收尾是串行的:注销(≤15s)→ 排空(≤5s)→ 等消费者 →
	// 框架收尾(≤2s)。消费者从 ctx 取消起就与前两步并行退出,wait 通常立刻返回;前两步用满时剩下的时间只够框架收尾,
	// 所以 wait 的实际上限 = min(consumerStopTimeout, 截止点 − 当前)。截止点留出框架收尾与 2s 余量,
	// 串行最坏仍 < lifecycle.HardTimeout,不会被硬截止强退、丢掉最后一段日志(consumer_handle_test.go 钉住)。
	consumerStopBudget = lifecycle.HardTimeout - lifecycle.FinishTimeout - 2*time.Second
	// boardSizeSampleTimeout:一次 ZCARD 采样的上限。采样是旁路,不能被 Redis 抖动拖住。
	boardSizeSampleTimeout = 5 * time.Second
)

func main() {
	flag.Parse()
	// 版本行直写 stdout,先于读配置与 logx 初始化:进程在配置 / 依赖阶段就崩溃时也已留下"跑的是哪一版"。
	fmt.Println(buildinfo.StartupLine("rank"))
	if *showVersion {
		return
	}

	var c config.Config
	// conf.MustLoad 会自动调用 (*Config).Validate(go-zero v1.10.0),不合法即 Fatalf 退出。
	conf.MustLoad(*configFile, &c)

	if err := runRank(c); err != nil {
		fmt.Fprintf(os.Stderr, "[rank] %v\n", err)
		os.Exit(1)
	}
}

// runRank 是常驻服务形态。启动失败与正常退出走同一条收尾(lifecycle.Shutdown),返回的 error 决定退出码。
//
// 停机(契约 §2、§9.1、§16;照 mail / friend 已活体验证的做法):lifecycle.Configure 把 go-zero proc 的自行停机推迟到 24s 硬截止,
// 正常预算内由本函数串行收尾:① nr.Close() 注销 → ② 真实 grpc.Server.GracefulStop 排空(≤5s)→ ③ 有上限地等快照消费者离组
// (consumerStopTimeout / consumerStopBudget),再停 killswitch watch、关 metrics / data_service / etcd → ④ proc.Shutdown 与
// 等 Start 返回(≤2s)→ 刷日志。到 24s 仍未收尾由 watchHardDeadline 强制退出。
// 快照消费者与榜规模采样都跟随 ctx:收到退出信号即开始退出,与 ①② 并行(消费者逐条同步提交,最多重放一条,写榜的版本判定兜底)。
func runRank(c config.Config) (runErr error) {
	ctx, stopSignals := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stopSignals()
	lifecycle.Configure(&c.RpcServerConf) // 必须早于 zrpc.NewServer(其 SetUp 会再次 proc.Setup 同一份值)
	finished := make(chan struct{})
	defer close(finished)
	go watchHardDeadline(ctx, stopSignals, finished)
	if exitRequested(ctx) {
		return nil
	}

	listenHost, port, err := c.ListenHostPort()
	if err != nil {
		return err
	}

	svcCtx, err := svc.NewServiceContext(c)
	if err != nil {
		return err
	}

	// 收尾所有权从这里开始:之后任何 return 都会 注销(若已注册)→ 排空(若已起服)→ 关资源 → 框架收尾。
	stopKillSwitch := func() {}
	stopMetrics := func() {}
	consumer := &consumerHandle{}
	var stopStarted time.Time // 开始收尾的时刻(下面 defer 里取消 ctx 时记下),consumerStopBudget 从这里算
	var serverSlot lifecycle.ServerSlot
	shutdown := &lifecycle.Shutdown{
		DrainTimeout:  lifecycle.DrainTimeout,
		FinishTimeout: lifecycle.FinishTimeout,
		// 先有上限地等快照消费者离组:赶在进程退出前让 broker 收到离组,其余副本立即接管分区,不用等会话超时。
		// killswitch watch 与注销共用 svcCtx 的 etcd 客户端,必须在 svcCtx.Stop 关连接之前停。
		// RankRedis(go-zero redis.Redis)没有 Close,不由业务关;svcCtx.Stop 只关 data_service 连接与 etcd 客户端。
		CloseResources: func() {
			consumer.wait(min(consumerStopTimeout, time.Until(stopStarted.Add(consumerStopBudget))))
			stopKillSwitch()
			stopMetrics()
			svcCtx.Stop()
		},
		FinishFramework: proc.Shutdown,
	}
	defer func() {
		stopSignals() // 启动失败时也从这里触发同一个 24s 硬截止
		stopStarted = time.Now()
		shutdown.Server = serverSlot.TakeForShutdown()
		if err := shutdown.Stop(); err != nil {
			logx.Errorf("[rank] Shutdown cleanup exceeded its budget: %v", err)
			runErr = errors.Join(runErr, err)
		}
		_ = logx.Close()
	}()

	// Prometheus /metrics(MetricsListenAddr 为空则不开)。指标懒注册,不调 Start 也能安全 Inc。
	stopMetrics = metrics.Start(c.MetricsListenAddr)

	// 写路径:评分快照消费者。Kafka 暂不可达不拖死读路径:降级为告警 + 后台重试(§2.4)。
	consumerLabel := startSnapshotConsumer(ctx, c, svcCtx.Board, consumer)
	// 榜规模采样(rank_board_size,告警 RankBoardShrunk 的输入)。
	startBoardSizeSampler(ctx, svcCtx.Board, c.BoardSizeSampleInterval)

	// 读路径:一页 + self + 名字补全(fail-open,800ms)。整请求预算 = Timeout − 回包余量。
	rankLogic := logic.NewRankLogic(svcCtx.Board,
		logic.NewDataServiceNames(svcCtx.DataService, logic.DefaultNameLookupTimeout), c.RequestBudget())

	// RPC 级热关停(shared/killswitch):复用 svcCtx 的 etcd 客户端;Start 非阻塞,etcd 不可达一律放行。
	ksCtx, ksCancel := context.WithCancel(context.Background())
	stopKillSwitch = ksCancel
	ks := killswitch.New(killswitch.Config{Prefix: c.KillSwitchPrefix})
	ks.Start(ksCtx, svcCtx.Etcd)

	serverReady := make(chan struct{})
	s, err := zrpc.NewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		// 客户端来源的方法级准入由 internal/session 的白名单负责(纵深防御,L-12)。
		rankpb.RegisterClientPlayerRankServer(grpcServer, server.NewRankServer(rankLogic))
		if config.IsRelaxedMode(c.Mode) {
			reflection.Register(grpcServer)
		}
		// 把真实 grpc.Server 交给退出线程:go-zero 的 RpcServer.Stop() 只 logx.Close(),主动排空只能调它。
		serverSlot.Publish(grpcServer)
		close(serverReady)
	})
	if err != nil {
		return fmt.Errorf("rank RPC 构造失败: %w", err)
	}
	// go-zero 自带拦截器在 NewServer 里已先加入,位于下面这条链的外层。
	s.AddUnaryInterceptors(buildUnaryInterceptors(ks)...)

	// **先起 gRPC,再注册**(契约 §2)。Start 在监听失败时 panic:recover 成错误交回主线程,走同一条收尾后以非 0 退出。
	serveDone := make(chan struct{})
	serveErr := make(chan error, 1)
	shutdown.ServeDone = serveDone
	go func() {
		var startErr error
		defer func() {
			if recovered := recover(); recovered != nil {
				startErr = fmt.Errorf("gRPC Start 失败: %v", recovered)
			}
			serveErr <- startErr
			close(serveDone)
		}()
		s.Start()
	}()
	select {
	case <-serverReady:
	case <-ctx.Done():
		return nil
	case err := <-serveErr:
		return fmt.Errorf("rank gRPC 监听未就绪: %v", err)
	}

	advertiseHost := advertisedHost(listenHost)
	regCtx, regCancel := context.WithTimeout(ctx, registerTimeout)
	nr, err := noderegistry.RegisterAfterListening(regCtx, svcCtx.Etcd, dialAddress(listenHost, port), listenProbeTimeout,
		noderegistry.Spec{
			// Prefix 由枚举名派生,不手写(契约 §2):"RankNodeService.rpc"。
			Prefix:     base.ENodeType_name[int32(nodeType)] + ".rpc",
			NodeType:   uint32(nodeType),
			ZoneId:     c.ZoneId,
			LeaseTTL:   c.LeaseTTL,
			BuildValue: nodeInfoValueBuilder(c.ZoneId, advertiseHost, port, uint64(time.Now().Unix())),
			// rank 不以 node_id 派生任何持久身份,失租后换 id 继续服务即可(D-11)。
			OnReclaimFailed: noderegistry.ReallocateNewID,
			OnNodeIDChanged: func(oldID, newID uint32) {
				logx.Errorf("[rank] Could not reclaim original node_id=%d after etcd lease loss; continuing with new node_id=%d", oldID, newID)
			},
			LogPrefix: "[rank]",
		})
	regCancel()
	if err != nil {
		if ctx.Err() != nil {
			return nil // 注册期间收到退出信号:按正常退出收尾
		}
		return fmt.Errorf("rank 节点注册 etcd 失败: %w", err)
	}
	shutdown.Unregister = nr.Close
	nr.KeepAlive()

	printBanner(os.Stdout, c, bannerInfo{
		advertise: net.JoinHostPort(advertiseHost, strconv.FormatUint(uint64(port), 10)),
		nodeID:    nr.NodeID(),
		nodeUUID:  nr.NodeUUID,
		consumer:  consumerLabel,
	})

	select {
	case <-ctx.Done():
		logx.Info("[rank] Received shutdown signal; deregistering before draining RPCs")
	case err := <-serveErr:
		return fmt.Errorf("rank gRPC 服务提前退出: %v", err)
	}
	return nil // 收尾在 defer 里按 注销 → 排空 → 关资源 → 框架收尾 执行
}

// startSnapshotConsumer 起评分快照消费者,返回横幅用的状态描述。
//
//   - Kafka.Brokers 为空:不消费(只读服务),合法但榜不会更新 —— 横幅与 ERROR 日志都写明;
//   - 起不来(Kafka 暂不可达、分区契约与 broker 不一致):降级为告警 + 每 30s 后台重试,读路径照常服务;
//     重试成功后从消费组已提交的位置(新组从最早)补消费,不丢快照。
func startSnapshotConsumer(ctx context.Context, c config.Config, writer rankkafka.SnapshotWriter, handle *consumerHandle) string {
	if !c.ConsumerEnabled() {
		logx.Error("[rank] WARN Kafka.Brokers is empty; rating snapshot consumer disabled, the leaderboard is read-only and will not update")
		return "disabled (Kafka.Brokers empty: read-only, the board will not update)"
	}
	cfg := rankkafka.ConsumerConfig{
		Brokers:    c.Kafka.Brokers,
		Topic:      c.RatingSnapshotTopic,
		GroupID:    c.RatingConsumerGroup,
		Partitions: c.RatingSnapshotTopicPartitions,
	}
	start := func() error {
		done, err := rankkafka.StartSnapshotConsumer(ctx, cfg, writer)
		if err == nil {
			handle.set(done)
		}
		return err
	}
	label := fmt.Sprintf("topic=%s partitions=%d group=%s", cfg.Topic, cfg.Partitions, cfg.GroupID)
	if err := start(); err != nil {
		if ctx.Err() != nil {
			return label + " (not started: shutting down)"
		}
		logx.Errorf("[rank] Failed to start rating snapshot consumer; board updates paused, retrying in the background every %s: %v",
			consumerRetryInterval, err)
		safego.Go("rank.kafka.rating_snapshot.retry", func() {
			for {
				select {
				case <-ctx.Done():
					return
				case <-time.After(consumerRetryInterval):
				}
				if err := start(); err != nil {
					if ctx.Err() != nil {
						return // 声明 topic 期间开始停机:不再入组(StartSnapshotConsumer 在建 reader 前复核 ctx)
					}
					logx.Errorf("[rank] Failed to restart rating snapshot consumer; retrying after %s: %v", consumerRetryInterval, err)
					continue
				}
				logx.Info("[rank] Rating snapshot consumer started after retry")
				return
			}
		})
		return label + " (start failed; retrying in the background, see ERROR log)"
	}
	return label
}

// consumerHandle 持有消费 goroutine 的 done 通道。消费者可能在后台重试循环里才起来,所以通道晚于 shutdown 构造时才有,加锁读写。
type consumerHandle struct {
	mu   sync.Mutex
	done <-chan struct{}
}

func (h *consumerHandle) set(done <-chan struct{}) {
	h.mu.Lock()
	h.done = done
	h.mu.Unlock()
}

// wait 有上限地等消费 goroutine 退出;消费者从没起来过时立即返回,limit ≤ 0(收尾预算已用完)时只看一眼不等。
// 调用前 ctx 已取消(收到退出信号或启动失败)。
func (h *consumerHandle) wait(limit time.Duration) {
	h.mu.Lock()
	done := h.done
	h.mu.Unlock()
	if done == nil {
		return
	}
	select {
	case <-done:
		return
	default:
	}
	if limit <= 0 {
		logx.Error("[rank] Shutdown budget exhausted before the rating snapshot consumer left its group; other replicas take over after the session timeout")
		return
	}
	timer := time.NewTimer(limit)
	defer timer.Stop()
	select {
	case <-done:
	case <-timer.C:
		logx.Errorf("[rank] Rating snapshot consumer did not leave its group within %s; other replicas take over after the session timeout", limit)
	}
}

// startBoardSizeSampler 定时 ZCARD 采样 rank_board_size(每个副本各自采样,告警用 max() 聚合)。
// 第一次采样立即异步做:不等满一个周期,也不让 Redis 抖动拖住启动。采样失败保留上一次的值,只记日志。
func startBoardSizeSampler(ctx context.Context, board *data.Board, interval time.Duration) {
	sample := func(ctx context.Context) {
		sampleCtx, cancel := context.WithTimeout(ctx, boardSizeSampleTimeout)
		defer cancel()
		size, err := board.Size(sampleCtx)
		if err != nil {
			if ctx.Err() == nil {
				logx.Errorf("[rank] Leaderboard size sample failed: %v", err)
			}
			return
		}
		metrics.SetBoardSize(metrics.BoardMatchRating, size)
	}
	safego.Go("rank.board_size.first", func() { sample(ctx) })
	safego.Loop(ctx, "rank.board_size", interval, sample)
}

// bannerInfo 是横幅里运行期才知道的部分。
type bannerInfo struct {
	advertise string
	nodeID    uint32
	nodeUUID  string
	consumer  string
}

// readyMarker 是 tools/scripts/go_services.ps1 的就绪判据(它在启动日志里找 "STARTED SUCCESSFULLY"):
// 改字就等于让启动器永远等不到 rank 就绪。rank_test.go 钉住它。
const readyMarker = "  RANK SERVICE STARTED SUCCESSFULLY"

// printBanner 打启动横幅。Redis 只打地址与是否回落,绝不打印口令。
func printBanner(w io.Writer, c config.Config, info bannerInfo) {
	fmt.Fprintln(w, "\n=============================================================")
	fmt.Fprintln(w, readyMarker)
	fmt.Fprintln(w, "=============================================================")
	fmt.Fprintf(w, "  Listen:        %s\n", c.ListenOn)
	fmt.Fprintf(w, "  Advertise:     %s\n", info.advertise)
	fmt.Fprintf(w, "  Mode:          %s\n", c.Mode)
	fmt.Fprintf(w, "  etcd:          %v\n", c.Etcd.Hosts)
	fmt.Fprintf(w, "  node_id:       %d (etcd CAS)\n", info.nodeID)
	fmt.Fprintf(w, "  node_uuid:     %s\n", info.nodeUUID)
	fmt.Fprintf(w, "  zone:          %d (affects registration path only)\n", c.ZoneId)
	fmt.Fprintf(w, "  rank_redis:    %s\n", rankRedisLabel(c))
	fmt.Fprintf(w, "  consumer:      %s\n", info.consumer)
	fmt.Fprintf(w, "  data_service:  %s (player names, fail-open %v)\n", c.DataServiceRpc.Etcd.Key, logic.DefaultNameLookupTimeout)
	if c.MetricsListenAddr != "" {
		fmt.Fprintf(w, "  metrics:       %s\n", c.MetricsListenAddr)
	}
	fmt.Fprintln(w, "=============================================================")
}

// rankRedisLabel 是横幅里榜键所在 Redis 的描述:回落共享库时写明"只适合本地"(§3.5)。
func rankRedisLabel(c config.Config) string {
	rc := c.EffectiveRankRedis()
	if c.RankRedisFallsBack() {
		return fmt.Sprintf("%s type=%s (fallback to shared Redis; local development only)", rc.Host, rc.Type)
	}
	return fmt.Sprintf("%s type=%s (dedicated)", rc.Host, rc.Type)
}

// watchHardDeadline 从进入退出开始计 lifecycle.HardTimeout,到点仍未收尾就强制退出(理由同 mail.go)。
func watchHardDeadline(ctx context.Context, stopSignals context.CancelFunc, finished <-chan struct{}) {
	select {
	case <-finished:
		return
	case <-ctx.Done():
	case <-proc.Done():
		// proc 在 init 已接管信号;补收配置加载期发生、NotifyContext 未看到的退出。Windows 上是 nil 通道。
		stopSignals()
	}
	timer := time.NewTimer(lifecycle.HardTimeout)
	defer timer.Stop()
	select {
	case <-finished:
	case <-timer.C:
		fmt.Fprintf(os.Stderr, "[rank] Shutdown hard deadline of %v reached; forcing exit, completion of in-flight requests cannot be confirmed\n", lifecycle.HardTimeout)
		os.Exit(1)
	}
}

// exitRequested 报告是否已经收到退出(本进程 ctx 或 go-zero proc),用于在建依赖 / 起服 / 注册之前短路。
func exitRequested(ctx context.Context) bool {
	select {
	case <-ctx.Done():
		return true
	case <-proc.Done():
		return true
	default:
		return false
	}
}

// buildUnaryInterceptors 组装一元拦截器链,返回顺序**就是执行顺序**(第一个是最外层,设计 §4.4)。抽成函数是为了让链本身可测
// (rank_test.go):哪次重构顺手删了 killswitch / session 那行,一切照常工作,只有出事那天才发现止血阀 / 准入是假的。
func buildUnaryInterceptors(ks *killswitch.Switch) []grpc.UnaryServerInterceptor {
	return []grpc.UnaryServerInterceptor{
		// ① 流量统计最外层:被热关停短路、被会话准入拒绝的请求也要被统计到。
		grpcstats.New(grpcstats.Options{}).UnaryServerInterceptor(),
		// ② 热关停尽早短路:命中后会话解码、读榜统统不做。
		ks.UnaryServerInterceptor(),
		// ③ 会话与方法准入(D-9):无会话 = 内部调用放行(逻辑层再回 kInvalidParameter、不查 Redis);坏会话 → Unauthenticated;
		//    带会话调白名单外方法 → PermissionDenied。
		session.UnaryServerInterceptor(session.ClientMethods),
		// ④ in-band 故障定性:读 TipInfoMessage.id 按 Tip.xlsx 的 fault 列定性(kServiceUnavailable = 故障,其余 = 业务拒绝)。
		serverbase.UnaryInterceptor(serverbase.Options{
			TipClassifier: constants.TipClassifier(),
		}),
	}
}

// nodeInfoValueBuilder 返回 noderegistry 的 BuildValue 回调:按 C++ NodeInfo 约定生成 protojson。
// Endpoint 与 GrpcEndpoint **必须都填**:只填 Endpoint 时路由服拨不通(guild 踩过),rank_test.go 钉住。
func nodeInfoValueBuilder(zoneId uint32, host string, port uint32, launchTime uint64) func(nodeID uint32, nodeUUID string) ([]byte, error) {
	return func(nodeID uint32, nodeUUID string) ([]byte, error) {
		info := &base.NodeInfo{
			NodeId:       nodeID,
			NodeType:     uint32(nodeType),
			ZoneId:       zoneId,
			NodeUuid:     nodeUUID,
			LaunchTime:   launchTime,
			ProtocolType: uint32(base.ENodeProtocolType_PROTOCOL_GRPC),
			Endpoint:     &base.EndpointComp{Ip: host, Port: port},
			GrpcEndpoint: &base.EndpointComp{Ip: host, Port: port},
		}
		return protojson.Marshal(info)
	}
}

// dialAddress 是注册前"探端口"用的拨号地址:监听 0.0.0.0 / :: 时本机只能拨 loopback。
func dialAddress(listenHost string, port uint32) string {
	host := listenHost
	if isUnspecifiedHost(host) {
		host = "127.0.0.1"
	}
	return net.JoinHostPort(host, strconv.FormatUint(uint64(port), 10))
}

// isUnspecifiedHost:""、0.0.0.0、:: 这类"监听所有网卡"的 host,不能原样写进 NodeInfo。
func isUnspecifiedHost(host string) bool {
	if host == "" {
		return true
	}
	ip := net.ParseIP(host)
	return ip != nil && ip.IsUnspecified()
}

// advertisedHost 决定写进 NodeInfo 的 IP(与 mail / friend 同一口径):
// POD_IP 优先 → ListenOn 的具体 host → 本机第一个非 loopback IP → 127.0.0.1。
func advertisedHost(listenHost string) string {
	if podIP := os.Getenv("POD_IP"); podIP != "" {
		return podIP
	}
	if !isUnspecifiedHost(listenHost) {
		return listenHost
	}
	if ip := netx.InternalIp(); ip != "" {
		return ip
	}
	return "127.0.0.1"
}
