// mail 服务 M1:系统邮件 + 定向邮件的列表 / 已读 / 删除 / 过期清理,领取短路(设计 docs/design/mail-system.md)。
//
// 全局一份、多副本、进程无状态:数据在独占库 mmorpg_mail(port-decisions D-14),推送前经共享 Redis 读跨运行时契约 key
// player:session:{id}。注册按 C++ NodeInfo 约定写进 etcd(shared/noderegistry),路由服按 MailNodeService.rpc 前缀发现本服务
// (契约 microservice-zone-contract §2/§3)。同一个 zrpc server 上挂两个 service:客户端协议 ClientPlayerMail 与
// 内部发件入口 MailAdmin(不标客户端协议、令牌鉴权,设计 §2.2)。
//
// 业务代码不读 cfg.ZoneId 做分支(全局服务):ZoneId 只影响 etcd 注册路径。
//
// 两种运行形态:
//   - 常驻服务(默认):建表 / 查表 → metrics → sweep → killswitch → gRPC → 注册 etcd;
//   - `-migrate`:只连 MySQL 跑一次 schemamigrate.Up,按 D-14 退出码退出(K8s mail-migrate Job 用),不起 gRPC、不连 etcd。
package main

import (
	"context"
	"database/sql"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"mail/internal/config"
	"mail/internal/constants"
	"mail/internal/data"
	"mail/internal/lifecycle"
	"mail/internal/logic"
	"mail/internal/metrics"
	"mail/internal/server"
	"mail/internal/session"
	"mail/internal/svc"

	base "proto/common/base"
	mailpb "proto/mail"

	"schemamigrate"

	"shared/buildinfo"
	"shared/grpcstats"
	"shared/killswitch"
	"shared/noderegistry"
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
	configFile = flag.String("f", "etc/mail.yaml", "the config file")

	// migrateOnly 是生产用的显式建表入口(D-14 第 4 条):`mail -f <yaml> -migrate` 只对 mmorpg_mail 跑一次
	// schemamigrate.Up(与 Schema.AutoMigrate=true 的启动路径同一份表清单与选项),跑完即退出。
	// 退出码按 schemamigrate.ExitCode:0 成功 / 1 错误 / 3 锁被占(K8s Job 按 backoff 重试)/ 4 需人工。
	migrateOnly = flag.Bool("migrate", false, "run schemamigrate.Up on mmorpg_mail and exit (0 ok / 1 error / 3 lock busy / 4 manual)")
	// 改列授权只属于显式迁移,不得被常驻服务启动期的 AutoMigrate 继承。
	allowModify = flag.Bool("allow-modify", false, "仅与 -migrate 一起使用:人工确认影响后允许 MODIFY COLUMN;常驻启动禁止使用")

	showVersion = flag.Bool("version", false, "打印版本信息并退出")
)

// nodeType 本服务的节点类型。MailNodeService = 8 早已在 node.proto 里,proto 不改。
const nodeType = base.ENodeType_MailNodeService

const (
	// listenProbeTimeout:等本进程 gRPC 端口可连的上限。先探端口再注册,不让路由服在监听建立前就选中本节点。
	listenProbeTimeout = 30 * time.Second
	// registerTimeout:注册整体(探端口 + etcd CAS)的上限,须大于 listenProbeTimeout。
	registerTimeout = 45 * time.Second
)

// migrateRemedyCommand 是 Schema.AutoMigrate=false 且库与 proto 不一致时打印给运维的补救命令。
const migrateRemedyCommand = "mail -f etc/mail.yaml -migrate"

func main() {
	flag.Parse()
	// 版本行直写 stdout,先于读配置与 logx 初始化:进程在配置 / 依赖阶段就崩溃时也已留下"跑的是哪一版"。
	fmt.Println(buildinfo.StartupLine("mail"))
	if *showVersion {
		return
	}
	if err := validateMigrationFlags(*migrateOnly, *allowModify); err != nil {
		fmt.Fprintf(os.Stderr, "[mail] %v\n", err)
		os.Exit(schemamigrate.ExitFailed)
	}

	var c config.Config
	// conf.MustLoad 会自动调用 (*Config).Validate(go-zero v1.10.0),不合法即 Fatalf 退出。
	conf.MustLoad(*configFile, &c)

	if *migrateOnly {
		os.Exit(runMigration(c, *allowModify, svc.OpenMySQL, schemamigrate.Up))
	}
	if err := runMail(c); err != nil {
		fmt.Fprintf(os.Stderr, "[mail] %v\n", err)
		os.Exit(1)
	}
}

// validateMigrationFlags 在读配置、连接数据库之前拒绝意外的常驻改列授权。
func validateMigrationFlags(migrateOnly, allowModify bool) error {
	if allowModify && !migrateOnly {
		return errors.New("-allow-modify 必须与 -migrate 一起使用;常驻启动不允许 MODIFY COLUMN")
	}
	return nil
}

// runMail 是常驻服务形态。启动失败与正常退出走同一条收尾(lifecycle.Shutdown),返回的 error 决定退出码。
//
// 停机(契约 §2、§9.1、§16;照 friend / chat / trade 已活体验证的做法):lifecycle.Configure 把 go-zero proc 的
// 自行停机推迟到 24s 硬截止,正常预算内由本函数串行收尾:① nr.Close() 注销 → ② 真实 grpc.Server.GracefulStop 排空(≤5s)
// → ③ 停 killswitch watch、关号段 / Kafka / DB / etcd → ④ proc.Shutdown 与等 Start 返回(≤2s)→ 刷日志。
// 到 24s 仍未收尾由 watchHardDeadline 强制退出。
func runMail(c config.Config) (runErr error) {
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
	// 令牌只从环境变量读(设计 §2.3);解析一次,横幅与 MailAdmin 用同一个结果。值绝不打印。
	adminToken := config.ResolveAdminToken(os.Getenv)

	svcCtx, err := svc.NewServiceContext(c)
	if err != nil {
		return err
	}

	// 收尾所有权从这里开始:之后任何 return 都会 注销(若已注册)→ 排空(若已起服)→ 关资源 → 框架收尾。
	stopKillSwitch := func() {}
	stopMetrics := func() {}
	var serverSlot lifecycle.ServerSlot
	shutdown := &lifecycle.Shutdown{
		DrainTimeout:  lifecycle.DrainTimeout,
		FinishTimeout: lifecycle.FinishTimeout,
		// killswitch watch 与注销共用 svcCtx 的 etcd 客户端,必须在 svcCtx.Stop 关连接之前停。
		CloseResources:  func() { stopKillSwitch(); stopMetrics(); svcCtx.Stop() },
		FinishFramework: proc.Shutdown,
	}
	defer func() {
		stopSignals() // 启动失败时也从这里触发同一个 24s 硬截止
		shutdown.Server = serverSlot.TakeForShutdown()
		if err := shutdown.Stop(); err != nil {
			logx.Errorf("[mail] Shutdown cleanup exceeded its budget: %v", err)
			runErr = errors.Join(runErr, err)
		}
		_ = logx.Close()
	}()

	// 建表 / 查表(D-14 第 4 条)。放在起 gRPC 之前:表不对的 mail 一个请求都不该接。
	// 不设整体超时、也不接退出信号:schemamigrate 自带锁等待与每条语句硬超时;中途取消 DDL 可能留下 dirty 台账。
	if err := ensureSchema(context.Background(), svcCtx.DB, c, schemamigrate.Up, schemamigrate.Plan); err != nil {
		return err
	}
	if exitRequested(ctx) {
		return nil
	}
	// 领第一段号:领不到只告警(发件 fail-closed 回 kServiceUnavailable,之后每次 Next 自己再试)。
	svcCtx.WarmMailIDSegment()

	deps := logic.NewDeps(svcCtx, adminToken)

	// Prometheus /metrics(MetricsListenAddr 为空则不开)。指标懒注册,不调 Start 也能安全 Inc。
	stopMetrics = metrics.Start(c.MetricsListenAddr)

	// 后台清理循环。放在 metrics.Start 之后,第一轮的 gauge 一开始就能被抓到;ctx 收到 SIGTERM 即退出,停机不等它。
	// ⚠ 默认 Sweep.Mode = report_only:不删任何数据,只统计 + WARN + 刷 gauge;横幅的 "sweep:" 行一眼可见。
	logic.StartSweep(ctx, deps)

	// RPC 级热关停(shared/killswitch):复用 svcCtx 的 etcd 客户端;Start 非阻塞,etcd 不可达一律放行。
	ksCtx, ksCancel := context.WithCancel(context.Background())
	stopKillSwitch = ksCancel
	ks := killswitch.New(killswitch.Config{Prefix: c.KillSwitchPrefix})
	ks.Start(ksCtx, svcCtx.Etcd)

	serverReady := make(chan struct{})
	s, err := zrpc.NewServer(c.RpcServerConf, func(grpcServer *grpc.Server) {
		// 两个 service 挂在同一个 server 上(go/trade 的 RegisterTradeAdminServer 先例,不新增端口)。
		// 客户端来源的方法级准入由 internal/session 的白名单负责:NotifyMailEvent 与 MailAdmin 的两个方法都不在里面。
		mailpb.RegisterClientPlayerMailServer(grpcServer, server.NewMailServer(deps))
		mailpb.RegisterMailAdminServer(grpcServer, server.NewAdminServer(deps))
		if config.IsRelaxedMode(c.Mode) {
			reflection.Register(grpcServer)
		}
		// 把真实 grpc.Server 交给退出线程:go-zero 的 RpcServer.Stop() 只 logx.Close(),主动排空只能调它。
		serverSlot.Publish(grpcServer)
		close(serverReady)
	})
	if err != nil {
		return fmt.Errorf("mail RPC 构造失败: %w", err)
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
		return fmt.Errorf("mail gRPC 监听未就绪: %v", err)
	}

	advertiseHost := advertisedHost(listenHost)
	regCtx, regCancel := context.WithTimeout(ctx, registerTimeout)
	nr, err := noderegistry.RegisterAfterListening(regCtx, svcCtx.Etcd, dialAddress(listenHost, port), listenProbeTimeout,
		noderegistry.Spec{
			// Prefix 由枚举名派生,不手写(契约 §2):"MailNodeService.rpc"。
			Prefix:     base.ENodeType_name[int32(nodeType)] + ".rpc",
			NodeType:   uint32(nodeType),
			ZoneId:     c.ZoneId,
			LeaseTTL:   c.LeaseTTL,
			BuildValue: nodeInfoValueBuilder(c.ZoneId, advertiseHost, port, uint64(time.Now().Unix())),
			// mail 不以 node_id 派生任何持久身份(id 全来自号段),失租后换 id 继续服务即可(D-11)。
			OnReclaimFailed: noderegistry.ReallocateNewID,
			OnNodeIDChanged: func(oldID, newID uint32) {
				logx.Errorf("[mail] Could not reclaim original node_id=%d after etcd lease loss; continuing with new node_id=%d", oldID, newID)
			},
			LogPrefix: "[mail]",
		})
	regCancel()
	if err != nil {
		if ctx.Err() != nil {
			return nil // 注册期间收到退出信号:按正常退出收尾
		}
		return fmt.Errorf("mail 节点注册 etcd 失败: %w", err)
	}
	shutdown.Unregister = nr.Close
	nr.KeepAlive()

	printBanner(os.Stdout, c, bannerInfo{
		advertise:   net.JoinHostPort(advertiseHost, strconv.FormatUint(uint64(port), 10)),
		nodeID:      nr.NodeID(),
		nodeUUID:    nr.NodeUUID,
		adminActive: adminToken != "",
		pushActive:  deps.Push != nil,
	})

	select {
	case <-ctx.Done():
		logx.Info("[mail] Received shutdown signal; deregistering before draining RPCs")
	case err := <-serveErr:
		return fmt.Errorf("mail gRPC 服务提前退出: %v", err)
	}
	return nil // 收尾在 defer 里按 注销 → 排空 → 关资源 → 框架收尾 执行
}

// bannerInfo 是横幅里运行期才知道的部分。
type bannerInfo struct {
	advertise   string
	nodeID      uint32
	nodeUUID    string
	adminActive bool
	pushActive  bool
}

// readyMarker 是 tools/scripts/go_services.ps1 的就绪判据(它在启动日志里找 "STARTED SUCCESSFULLY"):
// 改字就等于让启动器永远等不到 mail 就绪。mail_test.go 钉住它。
const readyMarker = "  MAIL SERVICE STARTED SUCCESSFULLY"

// printBanner 打启动横幅。mysql 目标不含密码;令牌只报 enabled / disabled,绝不打印值。
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
	fmt.Fprintf(w, "  mysql:         %s\n", svc.MySQLTarget(c.MySQL))
	fmt.Fprintf(w, "  schema:        %s\n", schemaModeLabel(c))
	for _, line := range adminBannerLines(info.adminActive) {
		fmt.Fprintf(w, "  %s\n", line)
	}
	fmt.Fprintf(w, "  attachments:   %s\n", attachmentsModeLabel(c))
	fmt.Fprintf(w, "  quota:         personal_per_player=%d active_system=%d ttl_days personal=%d system=%d\n",
		c.Mail.MaxPersonalMailsPerPlayer, c.Mail.MaxActiveSystemMails, c.Mail.MaxPersonalMailTtlDays, c.Mail.MaxSystemMailTtlDays)
	fmt.Fprintf(w, "  push:          %s\n", pushModeLabel(info.pushActive))
	fmt.Fprintf(w, "  sweep:         mode=%s interval=%v retention=%dd batch=%d\n",
		c.Sweep.Mode, c.Sweep.Interval, c.Sweep.RetentionDays, c.Sweep.BatchLimit)
	if c.MetricsListenAddr != "" {
		fmt.Fprintf(w, "  metrics:       %s\n", c.MetricsListenAddr)
	}
	fmt.Fprintln(w, "=============================================================")
}

// adminBannerLines 是横幅里 MailAdmin 的状态:第一行**恰为** mail_admin=enabled / mail_admin=disabled
// (设计 §2.3 / §10 ⑤ ⑨ 按整行判),原因另起一行,免得验收脚本要去切字符串。
func adminBannerLines(active bool) []string {
	if active {
		return []string{"mail_admin=enabled", "mail_admin_token: from " + config.AdminTokenEnv}
	}
	return []string{"mail_admin=disabled",
		"mail_admin_token: " + config.AdminTokenEnv + " not set or shorter than " + strconv.Itoa(config.MinAdminTokenBytes) + " bytes after trimming"}
}

func attachmentsModeLabel(c config.Config) string {
	if config.IsRelaxedMode(c.Mode) {
		return fmt.Sprintf("allowed (Mode=%s; currency types %v)", c.Mode, c.Mail.AllowedCurrencyTypes)
	}
	return "rejected (Mode is not dev/test; M1 claiming is not open)"
}

func pushModeLabel(active bool) string {
	if active {
		return "kafka (NotifyMailEvent to online recipients of new personal mail)"
	}
	return "disabled (Kafka.Brokers empty: new mail is persisted but not pushed)"
}

// watchHardDeadline 从进入退出开始计 lifecycle.HardTimeout,到点仍未收尾就强制退出(理由同 friend.go)。
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
		fmt.Fprintf(os.Stderr, "[mail] Shutdown hard deadline of %v reached; forcing exit, completion of in-flight requests cannot be confirmed\n", lifecycle.HardTimeout)
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

// buildUnaryInterceptors 组装一元拦截器链,返回顺序**就是执行顺序**(第一个是最外层)。抽成函数是为了让链本身可测
// (mail_test.go):哪次重构顺手删了 killswitch / session 那行,一切照常工作,只有出事那天才发现止血阀 / 准入是假的。
func buildUnaryInterceptors(ks *killswitch.Switch) []grpc.UnaryServerInterceptor {
	return []grpc.UnaryServerInterceptor{
		// ① 流量统计最外层:被热关停短路、被会话准入拒绝的请求也要被统计到。
		grpcstats.New(grpcstats.Options{}).UnaryServerInterceptor(),
		// ② 热关停尽早短路:命中后会话解码、MySQL 访问统统不做。
		ks.UnaryServerInterceptor(),
		// ③ 会话与方法准入(D-9):无会话 = 内部调用放行(MailAdmin 再由令牌鉴权);坏会话 → Unauthenticated;
		//    带会话调白名单外方法(NotifyMailEvent、MailAdmin 两个方法)→ PermissionDenied。
		session.UnaryServerInterceptor(session.ClientMethods),
		// ④ in-band 故障定性:读 TipInfoMessage.id 按 Tip.xlsx 的 fault 列定性(kServiceUnavailable = 故障,其余 = 业务拒绝)。
		serverbase.UnaryInterceptor(serverbase.Options{
			TipClassifier: constants.TipClassifier(),
		}),
	}
}

// schemaRunner 是 schemamigrate.Up / Plan 的函数形状;抽出来让启动期建表策略可单测。
type schemaRunner func(ctx context.Context, db *sql.DB, opts schemamigrate.Options) (schemamigrate.Report, error)

// schemaOptions 是启动路径与 -migrate 共用的迁移选项:库名断言用代码常量(Validate 已保证配置与之相等),
// 表清单只来自 data.Tables()。
func schemaOptions() schemamigrate.Options {
	return schemamigrate.Options{
		Database: data.DatabaseName,
		Tables:   data.Tables(),
		Logf:     logx.Infof,
	}
}

// missingIndexWarningPrefix 是 schemamigrate"缺索引"告警的前缀,逐字取自 go/schemamigrate/plan.go。
const missingIndexWarningPrefix = "缺索引"

// missingIndexWarnings 挑出"缺索引"告警。schemamigrate 对已存在的表不补建 proto 新声明的普通索引,只记 Warning
// (不进 ExitCode)。放过去等于带着缺索引的表服务:mail 最直接的受害者是列表 / 邮箱现数(idx_mail_personal_0)与 sweep
// (idx_mail_personal_1 / idx_mail_system_0 / idx_mail_system_state_0),缺了全是全表扫、零报错(M-16,口径同 guild / friend)。
func missingIndexWarnings(r schemamigrate.Report) []string {
	var missing []string
	for _, w := range r.Warnings {
		if strings.HasPrefix(w, missingIndexWarningPrefix) {
			missing = append(missing, w)
		}
	}
	return missing
}

// missingIndexError 把"缺索引"从告警升级成阻断错误;没有缺索引时返回 nil。
func missingIndexError(r schemamigrate.Report) error {
	missing := missingIndexWarnings(r)
	if len(missing) == 0 {
		return nil
	}
	return fmt.Errorf("[mail] 库 %s 缺 %d 个 proto 声明的普通索引,拒绝启动"+
		"(schemamigrate 对已存在的表不自动补建;按告警里的表名与索引列手工 "+
		"`ALTER TABLE <表> ADD INDEX <索引名> (<列...>);`,或在开发期直接删库重建): %s",
		data.DatabaseName, len(missing), strings.Join(missing, "; "))
}

// ensureSchema 是启动期建表策略(D-14 第 4 条):
//   - Schema.AutoMigrate 没写或 true:跑 Up;err 非 nil 或出现需人工项 → 拒绝启动。多副本同时起由 Up 内部 GET_LOCK 串行化。
//   - false:只跑只读 Plan;有待执行语句或需人工项 → 拒绝启动,并带上补救命令。
//   - 两种模式都把"缺普通索引"从 Warning 升级成阻断。
func ensureSchema(ctx context.Context, db *sql.DB, c config.Config, up, plan schemaRunner) error {
	opts := schemaOptions()
	if c.ShouldAutoMigrate() {
		report, err := up(ctx, db, opts)
		logReport("schemamigrate.Up", report)
		if err != nil {
			return fmt.Errorf("[mail] 启动期建表失败(库 %s),拒绝启动: %w", data.DatabaseName, err)
		}
		if len(report.Manual) > 0 {
			return fmt.Errorf("[mail] 启动期建表发现 %d 项需人工处理(库 %s),拒绝启动: %s",
				len(report.Manual), data.DatabaseName, strings.Join(report.Manual, "; "))
		}
		return missingIndexError(report)
	}

	report, err := plan(ctx, db, opts)
	logReport("schemamigrate.Plan", report)
	if err != nil {
		return fmt.Errorf("[mail] Schema.AutoMigrate=false,启动期检查表结构失败(库 %s),拒绝启动;确认库可达后执行 %s: %w",
			data.DatabaseName, migrateRemedyCommand, err)
	}
	if !report.Clean() {
		return fmt.Errorf("[mail] Schema.AutoMigrate=false 且库 %s 与 proto/mail/mail_table.proto 不一致"+
			"(待执行 %d 条、需人工 %d 项),拒绝启动;先执行 %s",
			data.DatabaseName, len(report.Statements), len(report.Manual), migrateRemedyCommand)
	}
	// Report.Clean() 不看 Warnings,缺索引落在 Warnings 里,必须单独判。
	return missingIndexError(report)
}

// runMigration 是 -migrate 的实现:只连 MySQL,不起 gRPC、不连 etcd。退出码按 schemamigrate.ExitCode(D-14)。
func runMigration(c config.Config, allowModify bool, openMySQL func(config.MySQLConf) (*sql.DB, error), up schemaRunner) int {
	fmt.Printf("mail schema migration: %s\n", svc.MySQLTarget(c.MySQL))
	db, err := openMySQL(c.MySQL)
	if err != nil {
		code := schemamigrate.ExitCode(schemamigrate.Report{}, err)
		fmt.Fprintf(os.Stderr, "schema migration FAILED (exit %d): %v\n", code, err)
		return code
	}
	defer func() { _ = db.Close() }()

	opts := schemaOptions()
	opts.AllowModifyColumn = allowModify
	report, err := up(context.Background(), db, opts)
	printReport(os.Stdout, report)
	code := schemamigrate.ExitCode(report, err)
	// 缺索引在 schemamigrate 里只是 Warning,不进 ExitCode。K8s 的 mail-migrate Job 只看退出码:不升级成 ExitManual 的话,
	// Job 绿着结束,随后的 mail Pod 又被 ensureSchema 拦下 —— "迁移成功但服务起不来"。两条路径用同一个判据。
	if code == schemamigrate.ExitOK {
		if missErr := missingIndexError(report); missErr != nil {
			fmt.Fprintf(os.Stderr, "schema migration needs manual action (exit %d): %v\n", schemamigrate.ExitManual, missErr)
			return schemamigrate.ExitManual
		}
	}
	switch {
	case err != nil:
		fmt.Fprintf(os.Stderr, "schema migration FAILED (exit %d): %v\n", code, err)
	case code != 0:
		fmt.Fprintf(os.Stderr, "schema migration needs manual action (exit %d): %d item(s) listed above\n", code, len(report.Manual))
	default:
		fmt.Printf("schema migration OK: %s synced from proto/mail/mail_table.proto (%d statement(s) executed)\n",
			data.DatabaseName, len(report.Statements))
	}
	return code
}

// reportLines 把迁移报告展开成可读行(Statements / Warnings / Manual 各一段)。
func reportLines(r schemamigrate.Report) []string {
	var lines []string
	for _, s := range r.Statements {
		lines = append(lines, "  statement: "+s)
	}
	for _, w := range r.Warnings {
		lines = append(lines, "  warning:   "+w)
	}
	for _, m := range r.Manual {
		lines = append(lines, "  MANUAL:    "+m)
	}
	return lines
}

func printReport(w io.Writer, r schemamigrate.Report) {
	for _, line := range reportLines(r) {
		fmt.Fprintln(w, line)
	}
}

// logReport 把启动期迁移报告写进日志:需人工项与告警用 Error,待执行 / 已执行语句用 Info。
func logReport(stage string, r schemamigrate.Report) {
	for _, s := range r.Statements {
		logx.Infof("[mail] %s statement: %s", stage, s)
	}
	for _, w := range r.Warnings {
		logx.Errorf("[mail] %s warning: %s", stage, w)
	}
	for _, m := range r.Manual {
		logx.Errorf("[mail] %s MANUAL: %s", stage, m)
	}
}

// schemaModeLabel 是横幅里的建表策略描述。
func schemaModeLabel(c config.Config) string {
	if c.ShouldAutoMigrate() {
		return "auto-migrate (schemamigrate.Up at startup)"
	}
	return "plan-only (tables owned by " + migrateRemedyCommand + ")"
}

// nodeInfoValueBuilder 返回 noderegistry 的 BuildValue 回调:按 C++ NodeInfo 约定生成 protojson。
// Endpoint 与 GrpcEndpoint **必须都填**:只填 Endpoint 时路由服拨不通(guild 踩过),mail_test.go 钉住。
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

// advertisedHost 决定写进 NodeInfo 的 IP(与 friend / chat / trade 同一口径):
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
