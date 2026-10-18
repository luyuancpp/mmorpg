// Package routetoolcli 是 route_tool 的命令行入口:解析参数、装配(配置 → mapping Redis → 全局库 RouteStore)、把结果映射成退出码
// (docs/design/player-route-mapping-durability-proposal.md §7、§11.2;运维步骤见 docs/ops/merge-zone-runbook.md §9.3 / §9.4)。
// 逻辑全在 internal/routetool。
//
// 2026-10-18(第 1b-1 批)从 cmd/route_tool/main.go 原样迁来,为的是并进 data_service 主程序:K8s 镜像只构建单一入口
// /app/service(deploy/k8s/Dockerfile.go-svc),生产上的运行方式是
//
//	data_service -f <yaml> -route-tool <子命令> [子命令参数]
//
// (data_service.go 的 -route-tool;一次性 Job / Pod 跑,见 runbook §9.4)。cmd/route_tool 只留一个薄包装,给 dev 的 `go run`
// 与既有 runbook 命令用,命令行逐字兼容。
//
// 子命令与退出码(0 干净 / 1 需要人工处理 / 2 失败 / 3 有生效的合服围栏):
//   - import-from-redis、audit、tidb-prepare:第 1a 批,语义不变;audit 新增 -frozen-recovery(冻结期恢复口径);
//   - freeze / reconcile-from-cache / unfreeze:第 1b-1 批,全局库回档后的恢复流程(freeze → reconcile-from-cache →
//     audit -frozen-recovery → unfreeze),只碰映射写者的冻结开关与反向修库,不改服务的运行行为。
//
// 与 data_service 读同一份 yaml:mapping Redis 永远是 DB 0(go-zero RedisConf 没有 DB 字段;连接带 RedisConf 的 User / Pass / Tls,
// 由 go-zero redis.NewRedis 透传),全局库取 SnapshotMySQL 段,连接池大小取 RouteStore 段;整段仍按服务的口径校验(非法即退出码 2)。
//
// RouteStore.Mode 的门禁(在连任何东西之前判):
//   - import-from-redis:yaml 是 db_first 时真导入默认拒绝,要显式 -allow-db-first(只给提案 §7 P3 那一次);
//   - reconcile-from-cache / unfreeze / audit -frozen-recovery:只在 db_first 下有意义,redis 模式退出码 2 —— 那时 Redis 才是
//     提交点、库只是 P1 时刻的快照,「从缓存修库」就是 import-from-redis 的事;
//   - freeze:redis 模式也允许,但 W1 在 redis 模式下不读 writable,只打 WARN。
package routetoolcli

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"strconv"
	"strings"
	"time"

	"data_service/internal/config"
	"data_service/internal/routetool"
	"data_service/internal/store"
	"data_service/internal/svc"

	goredis "github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

const (
	subImport      = "import-from-redis"
	subAudit       = "audit"
	subTiDBPrepare = "tidb-prepare"
	subFreeze      = "freeze"
	subReconcile   = "reconcile-from-cache"
	subUnfreeze    = "unfreeze"

	// redisPingTimeout 是装配时 PING mapping Redis 的上限:地址不可达要快速失败成退出码 2,
	// 不能等到第一条 SCAN 才发现(那时已经连上了全局库)。
	redisPingTimeout = 5 * time.Second

	// defaultFreezeGrace 是 reconcile-from-cache 开始扫描前的等待:冻结之前已经过了开关检查的 W1 仍会提交并发布,
	// 服务端 W1 的预算是 2s(其中加锁等待最多 1s),10s 留足余量。
	defaultFreezeGrace = 10 * time.Second
)

const usage = `用法:
  route_tool -f <data_service.yaml> import-from-redis [-dry-run=true] [-allow-db-first] [-batch=500] [-timeout=30m] [-max-report=200]
  route_tool -f <data_service.yaml> audit [-frozen-recovery] [-batch=500] [-timeout=30m] [-max-report=200] [-recheck-delay=2s] [-max-recheck=10000]
  route_tool -f <data_service.yaml> tidb-prepare -zones=1,2[,…] [-storages=1000000[,…]] [-dry-run=true] [-timeout=30m]
  route_tool -f <data_service.yaml> freeze [-reason=<文本>] [-freeze-timeout=5s] [-timeout=30m]
  route_tool -f <data_service.yaml> reconcile-from-cache [-dry-run=true] [-resolve-ties=report|cache] [-freeze-grace=10s] [-batch=500] [-timeout=30m] [-max-report=200]
  route_tool -f <data_service.yaml> unfreeze -confirm-audited [-cache-was-not-verified-acknowledged] [-freeze-timeout=5s] [-timeout=30m]
  (K8s 上同一套子命令走主程序:data_service -f <yaml> -route-tool <子命令> [参数])

退出码:0 干净;1 完成但需要人工处理(unfreeze:就绪标记在途中被改过,重跑);2 失败(参数 / 配置 / 连接 / 查询 / 写入 / 超时,
db_first 下真导入没加 -allow-db-first,redis 模式下跑 reconcile-from-cache / unfreeze / audit -frozen-recovery,
没冻结就修库,unfreeze 没加 -confirm-audited 或确认不了缓存);
3 有生效的合服围栏、或导入期间 merge:merged_into 变过(import-from-redis / reconcile-from-cache)。
tidb-prepare:0 完成或 MySQL 上跳过(MySQL 没有 Region);2 失败。
恢复流程(全局库回档之后,runbook §9.4):freeze → reconcile-from-cache(先 dry-run)→ audit -frozen-recovery(退出码 0)→ unfreeze -confirm-audited。`

// cliArgs 是解析后的命令行。
type cliArgs struct {
	configPath   string
	sub          string
	dryRun       bool
	allowDBFirst bool
	batch        int
	timeout      time.Duration
	maxReport    int
	recheckDelay time.Duration
	maxRecheck   int
	// zones / storages 只给 tidb-prepare。
	zones    []uint32
	storages []uint32
	// 第 1b-1 批:audit 的冻结期恢复口径;freeze / unfreeze / reconcile-from-cache 的参数。
	frozenRecovery     bool
	resolveTies        bool
	freezeGrace        time.Duration
	freezeTimeout      time.Duration
	reason             string
	confirmAudited     bool
	cacheUnverifiedAck bool
}

// routeStore 是本入口对全局库 RouteStore 的全部依赖(*store.RouteStore 天然满足;单测注入假实现)。
type routeStore interface {
	ImportRoutes(ctx context.Context, rows []store.RouteImportRow, dryRun bool) (store.RouteImportStats, error)
	ImportMergedInto(ctx context.Context, rows []store.MergedIntoRow, dryRun bool) (store.MergedIntoImportStats, error)
	ScanRoutes(ctx context.Context, after store.RouteScanCursor, limit int) ([]store.RouteRow, error)
	GetRoutes(ctx context.Context, ids []uint64) (map[uint64]store.RouteRow, error)
	ListMergedInto(ctx context.Context) ([]store.MergedIntoRow, error)
	PrepareTiDB(ctx context.Context, req store.RouteTiDBPrepareRequest) (store.RouteTiDBPrepareReport, error)
	ReadRouteMeta(ctx context.Context) (store.RouteMeta, error)
	RepairRoutesFromCache(ctx context.Context, rows []store.RouteImportRow, opts store.RouteRepairOptions) (store.RouteRepairStats, error)
	RepairMergedIntoFromCache(ctx context.Context, rows []store.MergedIntoRow, dryRun bool) (store.MergedIntoImportStats, error)
	FreezeRouteWriters(ctx context.Context) (bool, error)
	UnfreezeRouteWriters(ctx context.Context, minWatermarkMs uint64) (bool, error)
	Close() error
}

var _ routeStore = (*store.RouteStore)(nil)

// deps 是装配用到的外部依赖(显式注入,单测替换;生产取 defaultDeps)。
type deps struct {
	connectRedis   func(ctx context.Context, rc redis.RedisConf) (*redis.Redis, error)
	openRouteStore func(c config.Config) (routeStore, error)
	redisIdentity  func(ctx context.Context, rds *redis.Redis) (routetool.RedisIdentity, error)
}

func defaultDeps() deps {
	return deps{connectRedis: connectMappingRedis, openRouteStore: openRouteStore, redisIdentity: readRedisIdentity}
}

// Run 是 route_tool 的入口:args 不含程序名(`-f <yaml> <子命令> [参数]`),返回退出码,不调 os.Exit。
// stdout 只写一行统计(给脚本解析),其余全部进 stderr(含 go-zero logx 的输出)。
func Run(ctx context.Context, args []string, stdout, stderr io.Writer) int {
	return run(ctx, args, stdout, stderr, defaultDeps())
}

func run(ctx context.Context, args []string, stdout, stderr io.Writer, d deps) int {
	a, err := parseArgs(args, stderr)
	if errors.Is(err, flag.ErrHelp) {
		fmt.Fprintln(stderr, usage)
		return routetool.ExitClean
	}
	if err != nil {
		fmt.Fprintf(stderr, "route_tool: %v\n\n%s\n", err, usage)
		return routetool.ExitFailed
	}

	// stdout 只许有一行统计(给脚本解析),而 go-zero logx 默认把 Info 写到 stdout:整体改到 stderr,
	// 并只留 Error 以上 —— import 的 stderr 第一行要留给 NOTE,连接目标由 Target 行给出(只有 host 与库名)。
	logx.SetWriter(logx.NewWriter(stderr))
	logx.SetLevel(logx.ErrorLevel)
	logx.DisableStat()

	// 用 conf.Load 而不是 conf.MustLoad:后者出错时 log.Fatalf 直接退出 1,退出码就不归本工具管了。
	var c config.Config
	if err := conf.Load(a.configPath, &c); err != nil {
		fmt.Fprintf(stderr, "route_tool: load %s: %v\n", a.configPath, err)
		return routetool.ExitFailed
	}
	c.RouteStore = c.RouteStore.Normalize()
	if err := c.RouteStore.Validate(); err != nil {
		fmt.Fprintf(stderr, "route_tool: invalid RouteStore config in %s: %v\n", a.configPath, err)
		return routetool.ExitFailed
	}
	if code, refused := refuseByMode(a, c, stderr); refused {
		return code
	}

	ctx, cancel := context.WithTimeout(ctx, a.timeout)
	defer cancel()

	// tidb-prepare 与 freeze 只碰全局库:在连 mapping Redis 之前分流(Redis 不可达不该挡住预切与冻结 —— 冻结是止血动作)。
	switch a.sub {
	case subTiDBPrepare:
		return runTiDBPrepare(ctx, a, c, d, stdout, stderr)
	case subFreeze:
		return runFreeze(ctx, a, c, d, stdout, stderr)
	}

	rds, err := d.connectRedis(ctx, c.MappingRedis)
	if err != nil {
		fmt.Fprintf(stderr, "route_tool: %v\n", err)
		return routetool.ExitFailed
	}
	st, err := d.openRouteStore(c)
	if err != nil {
		fmt.Fprintf(stderr, "route_tool: cannot open the route tables on the global DB (%s): %v\n", store.OpenRouteStoreHint(a.configPath, err), err)
		return routetool.ExitFailed
	}
	defer func() { _ = st.Close() }()

	// 只打 host 与库名,不打 DSN / 口令。
	target := fmt.Sprintf("route_tool %s: mapping_redis=%s global_db=%s/%s", a.sub, c.MappingRedis.Host, c.SnapshotMySQL.Host, c.SnapshotMySQL.DBName)

	switch a.sub {
	case subImport:
		rep, err := routetool.Import(ctx, rds, st, routetool.ImportOptions{
			DryRun:    a.dryRun,
			Batch:     a.batch,
			MaxReport: a.maxReport,
			Target:    target,
			Stdout:    stdout,
			Stderr:    stderr,
		})
		if err != nil {
			fmt.Fprintf(stderr, "route_tool %s: %v\n", a.sub, err)
		}
		return routetool.ImportExitCode(rep, err)
	case subReconcile:
		rep, err := routetool.ReconcileFromCache(ctx, rds, st, routetool.RepairOptions{
			DryRun:      a.dryRun,
			ResolveTies: a.resolveTies,
			Batch:       a.batch,
			MaxReport:   a.maxReport,
			FreezeGrace: a.freezeGrace,
			Target:      target,
			Stdout:      stdout,
			Stderr:      stderr,
		})
		if err != nil {
			fmt.Fprintf(stderr, "route_tool %s: %v\n", a.sub, err)
		}
		return routetool.RepairExitCode(rep, err)
	case subUnfreeze:
		rep, err := routetool.Unfreeze(ctx, rds, st, routetool.UnfreezeOptions{
			CacheUnverifiedAcknowledged: a.cacheUnverifiedAck,
			Timeout:                     a.freezeTimeout,
			Identity:                    func(ctx context.Context) (routetool.RedisIdentity, error) { return d.redisIdentity(ctx, rds) },
			Target:                      target,
			Stdout:                      stdout,
			Stderr:                      stderr,
		})
		if err != nil {
			fmt.Fprintf(stderr, "route_tool %s: %v\n", a.sub, err)
		}
		return routetool.UnfreezeExitCode(rep, err)
	}

	// audit。冻结期恢复口径只在映射写者真的冻结时成立:可写时库独有的行与「Redis 丢了已确认的发布」分不开,
	// 放过 db_only_unpublished 就是放过真丢失。
	if a.frozenRecovery {
		meta, err := st.ReadRouteMeta(ctx)
		if err != nil {
			fmt.Fprintf(stderr, "route_tool %s: read route_meta: %v\n", a.sub, err)
			return routetool.ExitFailed
		}
		if !meta.Frozen() {
			fmt.Fprintf(stderr, "route_tool %s: -frozen-recovery only applies while the route writers are frozen "+
				"(route_meta.writable=%d); run `freeze` first or drop the flag\n", a.sub, meta.Writable)
			return routetool.ExitFailed
		}
	}
	rep, err := routetool.Audit(ctx, rds, st, routetool.AuditOptions{
		Batch:          a.batch,
		MaxReport:      a.maxReport,
		RecheckDelay:   a.recheckDelay,
		MaxRecheck:     a.maxRecheck,
		FrozenRecovery: a.frozenRecovery,
		Target:         target,
		Stdout:         stdout,
		Stderr:         stderr,
	})
	if err != nil {
		fmt.Fprintf(stderr, "route_tool %s: %v\n", a.sub, err)
	}
	return routetool.AuditExitCode(rep, err)
}

// refuseByMode 是 RouteStore.Mode 的门禁(见包注释),在连任何东西之前判。refused=true 时按 code 退出。
func refuseByMode(a cliArgs, c config.Config, stderr io.Writer) (int, bool) {
	// db_first 下真导入是反方向的回填,只有提案 §7 P3 那一次是合法的。
	if a.sub == subImport && !a.dryRun && c.RouteStore.DBFirst() && !a.allowDBFirst {
		fmt.Fprintf(stderr, "route_tool %s: %s has RouteStore.Mode=%s — the global DB is the source of truth, "+
			"so filling it from the cache runs the wrong way; a real import is refused. Only the proposal §7 P3 re-import "+
			"(after every replica switched) may pass -allow-db-first (merge-zone-runbook §9.3). Dry-run and audit are unaffected.\n",
			a.sub, a.configPath, c.RouteStore.Mode)
		return routetool.ExitFailed, true
	}
	needsDBFirst := a.sub == subReconcile || a.sub == subUnfreeze || (a.sub == subAudit && a.frozenRecovery)
	if needsDBFirst && !c.RouteStore.DBFirst() {
		what := a.sub
		if a.sub == subAudit {
			what = "audit -frozen-recovery"
		}
		fmt.Fprintf(stderr, "route_tool %s: %s has RouteStore.Mode=%s — in redis mode the cache is the commit point and the DB is only "+
			"the P1 snapshot, so there is nothing to repair the DB from and no frozen recovery; use import-from-redis (merge-zone-runbook §9.3)\n",
			what, a.configPath, c.RouteStore.Mode)
		return routetool.ExitFailed, true
	}
	// 就绪标记与 INFO 身份都按单实例定义(多键脚本、run_id);cluster 下 INFO 落到哪个节点不确定,确认不了身份。
	if a.sub == subUnfreeze && c.MappingRedis.Type != "" && c.MappingRedis.Type != redis.NodeType {
		fmt.Fprintf(stderr, "route_tool %s: MappingRedis.Type=%s; the ready marker and the Redis identity check need a single instance (Type=node)\n",
			a.sub, c.MappingRedis.Type)
		return routetool.ExitFailed, true
	}
	return 0, false
}

// runTiDBPrepare 是 tidb-prepare 的装配:只开全局库的 RouteStore(探针照跑:TiDB 上同时证明悲观事务与
// chk_route_shard 已就绪、模数与本二进制一致),不连 mapping Redis。失败原因由 routetool.TiDBPrepare 打在 stderr(FAILED 行)。
func runTiDBPrepare(ctx context.Context, a cliArgs, c config.Config, d deps, stdout, stderr io.Writer) int {
	st, err := d.openRouteStore(c)
	if err != nil {
		fmt.Fprintf(stderr, "route_tool: cannot open the route tables on the global DB (%s): %v\n", store.OpenRouteStoreHint(a.configPath, err), err)
		return routetool.ExitFailed
	}
	defer func() { _ = st.Close() }()

	// 只打 host 与库名,不打 DSN / 口令。
	target := fmt.Sprintf("route_tool %s: global_db=%s/%s", a.sub, c.SnapshotMySQL.Host, c.SnapshotMySQL.DBName)
	rep, err := routetool.TiDBPrepare(ctx, st, routetool.TiDBPrepareOptions{
		Zones:    a.zones,
		Storages: a.storages,
		DryRun:   a.dryRun,
		Target:   target,
		Stdout:   stdout,
		Stderr:   stderr,
	})
	return routetool.TiDBPrepareExitCode(rep, err)
}

// runFreeze 是 freeze 的装配:只开全局库,不连 mapping Redis。
func runFreeze(ctx context.Context, a cliArgs, c config.Config, d deps, stdout, stderr io.Writer) int {
	st, err := d.openRouteStore(c)
	if err != nil {
		fmt.Fprintf(stderr, "route_tool: cannot open the route tables on the global DB (%s): %v\n", store.OpenRouteStoreHint(a.configPath, err), err)
		return routetool.ExitFailed
	}
	defer func() { _ = st.Close() }()

	target := fmt.Sprintf("route_tool %s: global_db=%s/%s", a.sub, c.SnapshotMySQL.Host, c.SnapshotMySQL.DBName)
	rep, err := routetool.Freeze(ctx, st, routetool.FreezeOptions{
		Reason:    a.reason,
		Timeout:   a.freezeTimeout,
		RedisMode: !c.RouteStore.DBFirst(),
		Target:    target,
		Stdout:    stdout,
		Stderr:    stderr,
	})
	if err != nil {
		fmt.Fprintf(stderr, "route_tool %s: %v\n", a.sub, err)
	}
	return routetool.FreezeExitCode(rep, err)
}

// parseArgs 解析 `-f <yaml> <子命令> [子命令参数]`。子命令只认六个,参数按子命令登记(别的子命令的参数一律
// 「flag provided but not defined」);参数越界、多余参数一律报错(退出码 2)。
func parseArgs(args []string, stderr io.Writer) (cliArgs, error) {
	a := cliArgs{
		dryRun:        true,
		batch:         500,
		timeout:       30 * time.Minute,
		maxReport:     200,
		recheckDelay:  2 * time.Second,
		maxRecheck:    10000,
		freezeGrace:   defaultFreezeGrace,
		freezeTimeout: store.RouteFreezeTimeoutMin,
	}

	global := flag.NewFlagSet("route_tool", flag.ContinueOnError)
	global.SetOutput(stderr)
	global.StringVar(&a.configPath, "f", "", "data_service 的 yaml(与服务读同一份)")
	if err := global.Parse(args); err != nil {
		return a, err
	}
	if a.configPath == "" {
		return a, errors.New("missing -f <data_service.yaml>")
	}
	rest := global.Args()
	allSubs := strings.Join([]string{subImport, subAudit, subTiDBPrepare, subFreeze, subReconcile, subUnfreeze}, " | ")
	if len(rest) == 0 {
		return a, fmt.Errorf("missing subcommand (%s)", allSubs)
	}
	a.sub = rest[0]

	sub := flag.NewFlagSet("route_tool "+a.sub, flag.ContinueOnError)
	sub.SetOutput(stderr)
	sub.DurationVar(&a.timeout, "timeout", a.timeout, "整次运行的时限;超时退出码 2(导入与修库幂等,重跑即可)")
	var zonesRaw, storagesRaw string
	resolveTies := "report"
	switch a.sub {
	case subImport, subAudit, subReconcile:
		sub.IntVar(&a.batch, "batch", a.batch, fmt.Sprintf("一批的玩家数(SCAN COUNT / MGET / 库批量),[1, %d]", store.RouteBatchLimit))
		sub.IntVar(&a.maxReport, "max-report", a.maxReport, "stderr 明细行上限;stdout 的计数始终精确")
	}
	switch a.sub {
	case subFreeze, subUnfreeze:
		sub.DurationVar(&a.freezeTimeout, "freeze-timeout", a.freezeTimeout,
			fmt.Sprintf("冻结 / 解冻写本身的时限,至少 %s;TiDB 把 route_meta 设成缓存表后要 >= tidb_table_cache_lease + 2s", store.RouteFreezeTimeoutMin))
	}
	switch a.sub {
	case subImport:
		sub.BoolVar(&a.dryRun, "dry-run", a.dryRun, "只判定不写库;真导入用 -dry-run=false")
		sub.BoolVar(&a.allowDBFirst, "allow-db-first", a.allowDBFirst,
			"yaml 是 RouteStore.Mode=db_first 时仍做真导入(只用于提案 §7 P3 那一次;默认拒绝)")
	case subAudit:
		sub.DurationVar(&a.recheckDelay, "recheck-delay", a.recheckDelay, "复核前的等待(给途中的 W1 走完)")
		sub.IntVar(&a.maxRecheck, "max-recheck", a.maxRecheck, "复核的玩家数上限;超过则不复核、全部计入")
		sub.BoolVar(&a.frozenRecovery, "frozen-recovery", a.frozenRecovery,
			"冻结期恢复口径:db_only_unpublished 不计入退出码,其余类别必须为 0(只在 db_first 且已冻结时可用;runbook §9.4)")
	case subTiDBPrepare:
		sub.StringVar(&zonesRaw, "zones", "", fmt.Sprintf("要预切的 zone,逗号分隔(必填),每个在 [1, %d]", store.RouteHomeZoneMax))
		sub.StringVar(&storagesRaw, "storages", "", "idx_route_storage 要预切的落点值,逗号分隔(可空;Phase 2 PinOnCreate 指向 1000000 时传 1000000)")
		sub.BoolVar(&a.dryRun, "dry-run", a.dryRun, "只判引擎、核对聚簇、列出语句;真执行用 -dry-run=false")
	case subFreeze:
		sub.StringVar(&a.reason, "reason", "", "冻结原因,只打到 stderr(库里没有记原因的列)")
	case subReconcile:
		sub.BoolVar(&a.dryRun, "dry-run", a.dryRun, "只判定不写库;真修库用 -dry-run=false")
		sub.StringVar(&resolveTies, "resolve-ties", resolveTies, "同 rev 而值不同时:report 只报告(默认),cache 用缓存覆盖")
		sub.DurationVar(&a.freezeGrace, "freeze-grace", a.freezeGrace, "开始扫描前的等待,覆盖冻结之前已过开关检查的在途 W1")
	case subUnfreeze:
		sub.BoolVar(&a.confirmAudited, "confirm-audited", a.confirmAudited,
			"确认已在冻结状态下跑过 `audit -frozen-recovery` 且退出码为 0(必填)")
		sub.BoolVar(&a.cacheUnverifiedAck, "cache-was-not-verified-acknowledged", a.cacheUnverifiedAck,
			"没有可信的就绪标记时仍解冻:先按 merge-zone-runbook §9.2 重放清单,再显式加它(runbook §9.4)")
	default:
		return a, fmt.Errorf("unknown subcommand %q (want %s)", a.sub, allSubs)
	}
	if err := sub.Parse(rest[1:]); err != nil {
		return a, err
	}
	if sub.NArg() > 0 {
		return a, fmt.Errorf("unexpected arguments after %s: %v", a.sub, sub.Args())
	}
	switch a.sub {
	case subTiDBPrepare:
		if zonesRaw == "" {
			return a, errors.New("tidb-prepare: missing -zones=<zone>[,<zone>…]")
		}
		var err error
		if a.zones, err = parseIDList("zones", zonesRaw, 1, uint64(store.RouteHomeZoneMax)); err != nil {
			return a, err
		}
		if a.storages, err = parseIDList("storages", storagesRaw, 1, math.MaxUint32); err != nil {
			return a, err
		}
	case subReconcile:
		switch resolveTies {
		case "report":
		case "cache":
			a.resolveTies = true
		default:
			return a, fmt.Errorf("-resolve-ties=%s: want report or cache", resolveTies)
		}
	case subUnfreeze:
		if !a.confirmAudited {
			return a, errors.New("unfreeze: missing -confirm-audited (run `audit -frozen-recovery` while frozen first; it must exit 0)")
		}
	}

	switch {
	case a.batch < 1 || a.batch > store.RouteBatchLimit:
		return a, fmt.Errorf("-batch=%d not in [1, %d]", a.batch, store.RouteBatchLimit)
	case a.timeout <= 0:
		return a, fmt.Errorf("-timeout=%s must be positive", a.timeout)
	case a.maxReport < 0:
		return a, fmt.Errorf("-max-report=%d must not be negative", a.maxReport)
	case a.recheckDelay < 0:
		return a, fmt.Errorf("-recheck-delay=%s must not be negative", a.recheckDelay)
	case a.maxRecheck < 0:
		return a, fmt.Errorf("-max-recheck=%d must not be negative", a.maxRecheck)
	case a.freezeGrace < 0:
		return a, fmt.Errorf("-freeze-grace=%s must not be negative", a.freezeGrace)
	case a.freezeTimeout < store.RouteFreezeTimeoutMin:
		return a, fmt.Errorf("-freeze-timeout=%s must be at least %s", a.freezeTimeout, store.RouteFreezeTimeoutMin)
	}
	return a, nil
}

// parseIDList 解析逗号分隔的十进制 id 列表(-zones / -storages):空串 = 空列表;拒空项("1,"、",1")、
// 非数字、越界(每项必须在 [lo, hi])。去重排序不在这里做 —— 那是 store.PrepareTiDB 的规范化(一处口径)。
func parseIDList(name, raw string, lo, hi uint64) ([]uint32, error) {
	if raw == "" {
		return nil, nil
	}
	parts := strings.Split(raw, ",")
	out := make([]uint32, 0, len(parts))
	for _, p := range parts {
		if p == "" {
			return nil, fmt.Errorf("-%s=%s has an empty item", name, raw)
		}
		v, err := strconv.ParseUint(p, 10, 32)
		if err != nil {
			return nil, fmt.Errorf("-%s=%s: %q is not a decimal number: %v", name, raw, p, err)
		}
		if v < lo || v > hi {
			return nil, fmt.Errorf("-%s=%s: %d not in [%d, %d]", name, raw, v, lo, hi)
		}
		out = append(out, uint32(v))
	}
	return out, nil
}

// connectMappingRedis 连 mapping Redis 并 PING 一次。用 redis.NewRedis 而不是 MustNewRedis:
// 后者出错时 logx.Must 直接退出进程,退出码就不归本工具管了。RedisConf 的 User(ACL 用户名)/ Pass / Tls 由 NewRedis 透传。
func connectMappingRedis(ctx context.Context, rc redis.RedisConf) (*redis.Redis, error) {
	rds, err := redis.NewRedis(rc)
	if err != nil {
		return nil, fmt.Errorf("mapping redis %s: %w", rc.Host, err)
	}
	pingCtx, cancel := context.WithTimeout(ctx, redisPingTimeout)
	defer cancel()
	if !rds.PingCtx(pingCtx) {
		return nil, fmt.Errorf("mapping redis %s unreachable (PING failed within %s)", rc.Host, redisPingTimeout)
	}
	return rds, nil
}

// openRouteStore 开全局库的 RouteStore:连接参数同 data_service(svc.MySQLConfigOf),池大小取 RouteStore 段,
// 探针失败就返回 error。探针五步:会话 RC + 锁等待 1s、TiDB 悲观事务、route_meta 行在、chk_route_shard 实测(P4)、
// 行片模数核对(P5)。后两步是同一个必然回滚的事务里的三次 INSERT,不留数据,但要 player_route 的 INSERT 权限 ——
// 只读的 audit 与 dry-run 也一样。
func openRouteStore(c config.Config) (routeStore, error) {
	cfg := svc.MySQLConfigOf(c)
	cfg.MaxOpenConn = c.RouteStore.MaxOpenConn
	cfg.MaxIdleConn = c.RouteStore.MaxIdleConn
	host, _ := os.Hostname() // 取不到时 NewRouteWriterID 用 "unknown"
	st, err := store.NewRouteStore(cfg, store.RouteStoreOptions{
		FenceShards: uint32(c.RouteStore.FenceShards),
		Writer:      store.NewRouteWriterID("route_tool", host),
	})
	if err != nil {
		// 不能把 nil 的 *store.RouteStore 包进接口返回:那是「非 nil 接口包着 nil 指针」,调用方的判空拦不住。
		return nil, err
	}
	return st, nil
}

// readRedisIdentity 用 INFO server / INFO replication 读 mapping Redis 的身份(go-zero 没有 Info 方法,只能借 pipeline)。
// pipeline 返回任何错误都算读不到:包括 go-zero 熔断打开时整批没执行(命令的 Err() 为 nil、值为空,只有返回值里有
// breaker.ErrServiceUnavailable)—— 身份读不准时宁可确认不了(fail-closed)。
func readRedisIdentity(ctx context.Context, rds *redis.Redis) (routetool.RedisIdentity, error) {
	var server, replication *goredis.StringCmd
	pipeErr := rds.PipelinedCtx(ctx, func(p redis.Pipeliner) error {
		server = p.Info(ctx, "server")
		replication = p.Info(ctx, "replication")
		return nil
	})
	if pipeErr != nil {
		return routetool.RedisIdentity{}, fmt.Errorf("INFO server / replication: %w", pipeErr)
	}
	if server == nil || replication == nil {
		return routetool.RedisIdentity{}, errors.New("INFO server / replication: pipeline callback was not invoked")
	}
	s, err := server.Result()
	if err != nil {
		return routetool.RedisIdentity{}, fmt.Errorf("INFO server: %w", err)
	}
	r, err := replication.Result()
	if err != nil {
		return routetool.RedisIdentity{}, fmt.Errorf("INFO replication: %w", err)
	}
	return routetool.ParseRedisIdentity(s, r)
}
