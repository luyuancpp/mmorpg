// route_tool 是映射持久化 P1 的运维工具:把 mapping Redis 里现存的归属 / 落点导进全局库的路由表,并审计两边是否一致
// (docs/design/player-route-mapping-durability-proposal.md §7 P1、§11.2;运维步骤见 docs/ops/merge-zone-runbook.md §9.3)。
//
//	route_tool -f <data_service.yaml> import-from-redis [-dry-run=true] [-allow-db-first] [-batch=500] [-timeout=30m] [-max-report=200]
//	route_tool -f <data_service.yaml> audit [-batch=500] [-timeout=30m] [-max-report=200] [-recheck-delay=2s] [-max-recheck=10000]
//	route_tool -f <data_service.yaml> tidb-prepare -zones=1,2[,…] [-storages=1000000[,…]] [-dry-run=true] [-timeout=30m]
//
// 本文件只做薄入口:解析参数、装配(配置 → mapping Redis → 全局库 RouteStore)、把结果映射成退出码;
// 逻辑全在 internal/routetool。退出码:0 干净 / 1 需要人工处理 / 2 失败 / 3 有生效的合服围栏或窗口内跑过合服(仅 import)。
//
// tidb-prepare(2026-10-12 G1a,TiDB-only):对路由表设 merge_option=deny 并按片 / 按 zone 预切 Region
// (player_route 改成聚簇 (route_shard, player_id) 之后,不预切的话 16 条写入尾部在首日仍压在同一个 Region 上)。
// 放在 route_tool 而不是迁移里:按 zone 预切要真实 zone 清单,data_service 的 yaml 里没有。只碰全局库,不连
// mapping Redis;MySQL 上自动跳过(退出码 0)。何时要跑:TiDB 上建表之后、开新 zone 时(该 zone 开放前、低峰)、
// BR / PITR 恢复之后、TRUNCATE 之后。它同样经过建池探针(含 chk_route_shard 实测),CHECK 不生效的库直接退出码 2。
// K8s 上要等第 1b 批把 route_tool 并进主程序入口(与 import / audit 同一限制,见下)。
//
// 与 data_service 读同一份 yaml:mapping Redis 永远是 DB 0(go-zero RedisConf 没有 DB 字段),全局库取 SnapshotMySQL 段,
// 连接池大小取 RouteStore 段;整段仍按服务的口径校验(非法即退出码 2)。
//
// RouteStore.Mode 只管一件事:yaml 是 db_first 时,真导入(-dry-run=false)默认拒绝(退出码 2),要显式加 -allow-db-first。
// db_first 下库才是真源,「从缓存回填库」方向是反的:绕过发布脚本的写者留下的值可能被补进库;导入事务还会对一批行
// 持 FOR UPDATE、逐行 CAS 更新,并发的 W1 在同一玩家行上最多等满 1s 后以 1205 失败。唯一合法的用法是提案 §7 P3
// (全部副本切完之后补一次滚动期间旧副本只写 Redis 的登记),所以要求显式确认。dry-run 与 audit 只读,不受影响。
// K8s 镜像只构建单一入口(deploy/k8s/Dockerfile.go-svc),生产上跑它要等第 1b 批把入口并进 data_service 主程序。
package main

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

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

const (
	subImport      = "import-from-redis"
	subAudit       = "audit"
	subTiDBPrepare = "tidb-prepare"

	// redisPingTimeout 是装配时 PING mapping Redis 的上限:地址不可达要快速失败成退出码 2,
	// 不能等到第一条 SCAN 才发现(那时已经连上了全局库)。
	redisPingTimeout = 5 * time.Second
)

const usage = `用法:
  route_tool -f <data_service.yaml> import-from-redis [-dry-run=true] [-allow-db-first] [-batch=500] [-timeout=30m] [-max-report=200]
  route_tool -f <data_service.yaml> audit [-batch=500] [-timeout=30m] [-max-report=200] [-recheck-delay=2s] [-max-recheck=10000]
  route_tool -f <data_service.yaml> tidb-prepare -zones=1,2[,…] [-storages=1000000[,…]] [-dry-run=true] [-timeout=30m]

退出码:0 干净;1 完成但需要人工处理;2 失败(参数 / 配置 / 连接 / 查询 / 写入 / 超时,或 db_first 下真导入没加 -allow-db-first);
3 有生效的合服围栏、或导入期间 merge:merged_into 变过(仅 import-from-redis)。
tidb-prepare:0 完成或 MySQL 上跳过(MySQL 没有 Region);2 失败。`

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
}

func main() {
	os.Exit(run(context.Background(), os.Args[1:], os.Stdout, os.Stderr))
}

// run 是可测的入口:返回退出码,不调 os.Exit。
func run(ctx context.Context, args []string, stdout, stderr io.Writer) int {
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
	// 在连任何东西之前拦:db_first 下真导入是反方向的回填,只有提案 §7 P3 那一次是合法的(见文件头)。
	if a.sub == subImport && !a.dryRun && c.RouteStore.DBFirst() && !a.allowDBFirst {
		fmt.Fprintf(stderr, "route_tool %s: %s has RouteStore.Mode=%s — the global DB is the source of truth, "+
			"so filling it from the cache runs the wrong way; a real import is refused. Only the proposal §7 P3 re-import "+
			"(after every replica switched) may pass -allow-db-first (merge-zone-runbook §9.3). Dry-run and audit are unaffected.\n",
			a.sub, a.configPath, c.RouteStore.Mode)
		return routetool.ExitFailed
	}

	ctx, cancel := context.WithTimeout(ctx, a.timeout)
	defer cancel()

	// tidb-prepare 只碰全局库:在连 mapping Redis 之前分流(Redis 不可达不该挡住预切)。
	if a.sub == subTiDBPrepare {
		return runTiDBPrepare(ctx, a, c, stdout, stderr)
	}

	rds, err := connectMappingRedis(ctx, c.MappingRedis)
	if err != nil {
		fmt.Fprintf(stderr, "route_tool: %v\n", err)
		return routetool.ExitFailed
	}
	st, err := openRouteStore(c)
	if err != nil {
		fmt.Fprintf(stderr, "route_tool: cannot open the route tables on the global DB "+
			"(run `data_service -f %s -migrate` first if route_meta is missing): %v\n", a.configPath, err)
		return routetool.ExitFailed
	}
	defer func() { _ = st.Close() }()

	// 只打 host 与库名,不打 DSN / 口令。
	target := fmt.Sprintf("route_tool %s: mapping_redis=%s global_db=%s/%s", a.sub, c.MappingRedis.Host, c.SnapshotMySQL.Host, c.SnapshotMySQL.DBName)

	if a.sub == subImport {
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
	}

	rep, err := routetool.Audit(ctx, rds, st, routetool.AuditOptions{
		Batch:        a.batch,
		MaxReport:    a.maxReport,
		RecheckDelay: a.recheckDelay,
		MaxRecheck:   a.maxRecheck,
		Target:       target,
		Stdout:       stdout,
		Stderr:       stderr,
	})
	if err != nil {
		fmt.Fprintf(stderr, "route_tool %s: %v\n", a.sub, err)
	}
	return routetool.AuditExitCode(rep, err)
}

// runTiDBPrepare 是 tidb-prepare 的装配:只开全局库的 RouteStore(探针照跑:TiDB 上同时证明悲观事务与
// chk_route_shard 已就绪),不连 mapping Redis。失败原因由 routetool.TiDBPrepare 打在 stderr(FAILED 行)。
func runTiDBPrepare(ctx context.Context, a cliArgs, c config.Config, stdout, stderr io.Writer) int {
	st, err := openRouteStore(c)
	if err != nil {
		fmt.Fprintf(stderr, "route_tool: cannot open the route tables on the global DB "+
			"(run `data_service -f %s -migrate` first if route_meta is missing): %v\n", a.configPath, err)
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

// parseArgs 解析 `-f <yaml> <子命令> [子命令参数]`。子命令只认三个,参数按子命令登记(别的子命令的参数一律
// 「flag provided but not defined」);参数越界、多余参数一律报错(退出码 2)。
func parseArgs(args []string, stderr io.Writer) (cliArgs, error) {
	a := cliArgs{
		dryRun:       true,
		batch:        500,
		timeout:      30 * time.Minute,
		maxReport:    200,
		recheckDelay: 2 * time.Second,
		maxRecheck:   10000,
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
	if len(rest) == 0 {
		return a, fmt.Errorf("missing subcommand (%s | %s | %s)", subImport, subAudit, subTiDBPrepare)
	}
	a.sub = rest[0]

	sub := flag.NewFlagSet("route_tool "+a.sub, flag.ContinueOnError)
	sub.SetOutput(stderr)
	sub.DurationVar(&a.timeout, "timeout", a.timeout, "整次运行的时限;超时退出码 2(导入幂等,重跑即可)")
	var zonesRaw, storagesRaw string
	switch a.sub {
	case subImport, subAudit:
		sub.IntVar(&a.batch, "batch", a.batch, fmt.Sprintf("一批的玩家数(SCAN COUNT / MGET / 库批量),[1, %d]", store.RouteBatchLimit))
		sub.IntVar(&a.maxReport, "max-report", a.maxReport, "stderr 明细行上限;stdout 的计数始终精确")
	}
	switch a.sub {
	case subImport:
		sub.BoolVar(&a.dryRun, "dry-run", a.dryRun, "只判定不写库;真导入用 -dry-run=false")
		sub.BoolVar(&a.allowDBFirst, "allow-db-first", a.allowDBFirst,
			"yaml 是 RouteStore.Mode=db_first 时仍做真导入(只用于提案 §7 P3 那一次;默认拒绝)")
	case subAudit:
		sub.DurationVar(&a.recheckDelay, "recheck-delay", a.recheckDelay, "复核前的等待(给途中的 W1 走完)")
		sub.IntVar(&a.maxRecheck, "max-recheck", a.maxRecheck, "复核的玩家数上限;超过则不复核、全部计入")
	case subTiDBPrepare:
		sub.StringVar(&zonesRaw, "zones", "", fmt.Sprintf("要预切的 zone,逗号分隔(必填),每个在 [1, %d]", store.RouteHomeZoneMax))
		sub.StringVar(&storagesRaw, "storages", "", "idx_route_storage 要预切的落点值,逗号分隔(可空;Phase 2 PinOnCreate 指向 1000000 时传 1000000)")
		sub.BoolVar(&a.dryRun, "dry-run", a.dryRun, "只判引擎、核对聚簇、列出语句;真执行用 -dry-run=false")
	default:
		return a, fmt.Errorf("unknown subcommand %q (want %s | %s | %s)", a.sub, subImport, subAudit, subTiDBPrepare)
	}
	if err := sub.Parse(rest[1:]); err != nil {
		return a, err
	}
	if sub.NArg() > 0 {
		return a, fmt.Errorf("unexpected arguments after %s: %v", a.sub, sub.Args())
	}
	if a.sub == subTiDBPrepare {
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
// 后者出错时 logx.Must 直接退出进程,退出码就不归本工具管了。
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
// 探针(会话 RC + 锁等待 1s、TiDB 悲观事务、route_meta 行在)失败就返回 error。
func openRouteStore(c config.Config) (*store.RouteStore, error) {
	cfg := svc.MySQLConfigOf(c)
	cfg.MaxOpenConn = c.RouteStore.MaxOpenConn
	cfg.MaxIdleConn = c.RouteStore.MaxIdleConn
	host, _ := os.Hostname() // 取不到时 NewRouteWriterID 用 "unknown"
	return store.NewRouteStore(cfg, store.RouteStoreOptions{
		FenceShards: uint32(c.RouteStore.FenceShards),
		Writer:      store.NewRouteWriterID("route_tool", host),
	})
}
