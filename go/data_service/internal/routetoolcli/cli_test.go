package routetoolcli

// route_tool 入口的参数解析、模式门禁、装配与退出码。
//
// 前五个用例(TestRouteTool*)2026-10-18 从 cmd/route_tool/main_test.go 原样迁来,**名字不变**:前几批的验证命令按
// `-run "Route|…"` 选用例,改名会让它们在旧口径下静默消失(提案 §11.2「第 1b-1 批」的更正注列了新旧位置的对应)。
// 只钉「失败一律退出码 2、不 panic、不往 stdout 写东西」;各子命令本身的逻辑在 internal/routetool 的用例里。
// TestRun* 是第 1b-1 批新增的:新子命令的参数作用域、模式门禁、装配顺序与 stdout 纪律,装配用注入的 deps(不连真库)。

import (
	"bytes"
	"context"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"testing"
	"time"

	"data_service/internal/config"
	"data_service/internal/routetool"
	"data_service/internal/store"

	"shared/placement"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// lockedBuffer 是并发安全的 bytes.Buffer:run 会把 go-zero logx 改写到 stderr,
// go-zero 的后台 goroutine 可能在 run 返回之后仍往里写。
type lockedBuffer struct {
	mu  sync.Mutex
	buf bytes.Buffer
}

func (b *lockedBuffer) Write(p []byte) (int, error) {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.Write(p)
}

func (b *lockedBuffer) String() string {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.buf.String()
}

// runCLI 走生产装配(Run → defaultDeps)。
func runCLI(args ...string) (int, string, string) {
	var stdout, stderr lockedBuffer
	code := Run(context.Background(), args, &stdout, &stderr)
	return code, stdout.String(), stderr.String()
}

// runCLIWith 走注入的装配(不连真库)。
func runCLIWith(d deps, args ...string) (int, string, string) {
	var stdout, stderr lockedBuffer
	code := run(context.Background(), args, &stdout, &stderr, d)
	return code, stdout.String(), stderr.String()
}

// TestRouteToolBadArgsExitTwo:缺 -f、缺 / 未知子命令、-batch 越界、子命令不认的参数、多余参数,一律退出码 2 并打印用法。
func TestRouteToolBadArgsExitTwo(t *testing.T) {
	cases := []struct {
		name string
		args []string
		want string
	}{
		{"missing -f", []string{"audit"}, "missing -f"},
		{"missing subcommand", []string{"-f", "x.yaml"}, "missing subcommand"},
		{"unknown subcommand", []string{"-f", "x.yaml", "export"}, "unknown subcommand"},
		{"batch zero", []string{"-f", "x.yaml", "audit", "-batch=0"}, "-batch=0"},
		{"batch over the store limit", []string{"-f", "x.yaml", "import-from-redis", "-batch=1001"}, "-batch=1001"},
		{"negative max-report", []string{"-f", "x.yaml", "import-from-redis", "-max-report=-1"}, "-max-report=-1"},
		{"zero timeout", []string{"-f", "x.yaml", "audit", "-timeout=0s"}, "-timeout=0s"},
		{"dry-run is import only", []string{"-f", "x.yaml", "audit", "-dry-run=false"}, "dry-run"},
		{"recheck flags are audit only", []string{"-f", "x.yaml", "import-from-redis", "-max-recheck=1"}, "max-recheck"},
		{"allow-db-first is import only", []string{"-f", "x.yaml", "audit", "-allow-db-first"}, "allow-db-first"},
		{"extra positional argument", []string{"-f", "x.yaml", "audit", "extra"}, "unexpected arguments"},
		// tidb-prepare(G1a):-zones 必填、逐项校验;它不认 import / audit 的参数,别的子命令也不认它的参数。
		{"tidb-prepare missing zones", []string{"-f", "x.yaml", "tidb-prepare"}, "-zones"},
		{"tidb-prepare zone zero", []string{"-f", "x.yaml", "tidb-prepare", "-zones=0"}, "-zones"},
		{"tidb-prepare zone over the max", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1000000"}, "-zones"},
		{"tidb-prepare zone not a number", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1,a"}, "-zones"},
		{"tidb-prepare empty zone item", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1,"}, "-zones"},
		{"tidb-prepare storage zero", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1", "-storages=0"}, "-storages"},
		{"batch is not a tidb-prepare flag", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1", "-batch=10"}, "batch"},
		{"zones is tidb-prepare only", []string{"-f", "x.yaml", "audit", "-zones=1"}, "zones"},
	}
	for _, c := range cases {
		t.Run(c.name, func(t *testing.T) {
			code, stdout, stderr := runCLI(c.args...)
			assert.Equal(t, routetool.ExitFailed, code)
			assert.Empty(t, stdout)
			assert.Contains(t, stderr, c.want)
			assert.Contains(t, stderr, "用法:")
		})
	}
}

// TestRouteToolMissingConfigExitsTwo:配置读不到 → 退出码 2(不是 conf.MustLoad 的 log.Fatalf 退出 1)。
func TestRouteToolMissingConfigExitsTwo(t *testing.T) {
	code, stdout, stderr := runCLI("-f", filepath.Join(t.TempDir(), "absent.yaml"), "audit")
	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, "absent.yaml")
}

// TestRouteToolUnreachableRedisExitsTwoWithoutPanic:mapping Redis 不可达 → 退出码 2,不 panic、不退出进程
// (MustNewRedis 会经 logx.Must 直接退出),也不去碰全局库。
func TestRouteToolUnreachableRedisExitsTwoWithoutPanic(t *testing.T) {
	addr := closedLocalAddr(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, addr))

	code, stdout, stderr := runCLI("-f", path, "audit")

	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, addr)
	assert.Contains(t, stderr, "unreachable")
}

// TestRouteToolRealImportUnderDBFirstNeedsExplicitFlag:yaml 是 RouteStore.Mode=db_first 时,真导入默认拒绝
// (退出码 2,在连 Redis / 库之前就拦下)—— db_first 下库才是真源,从缓存回填库方向是反的,只有提案 §7 P3 那一次合法。
// -allow-db-first 放行、dry-run 不受影响:这两种都越过这道闸,走到连 Redis(这里地址不可达 → unreachable)。
func TestRouteToolRealImportUnderDBFirstNeedsExplicitFlag(t *testing.T) {
	addr := closedLocalAddr(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, addr)+dbFirstYaml)

	t.Run("refused without the flag", func(t *testing.T) {
		code, stdout, stderr := runCLI("-f", path, "import-from-redis", "-dry-run=false")
		assert.Equal(t, routetool.ExitFailed, code)
		assert.Empty(t, stdout)
		assert.Contains(t, stderr, "-allow-db-first")
		assert.NotContains(t, stderr, "unreachable", "必须在连 Redis 之前就拒绝")
	})
	for _, c := range []struct {
		name string
		args []string
	}{
		{"real import with the flag passes the gate", []string{"-f", path, "import-from-redis", "-dry-run=false", "-allow-db-first"}},
		{"dry-run passes the gate", []string{"-f", path, "import-from-redis"}},
		{"audit passes the gate", []string{"-f", path, "audit"}},
	} {
		t.Run(c.name, func(t *testing.T) {
			code, stdout, stderr := runCLI(c.args...)
			assert.Equal(t, routetool.ExitFailed, code)
			assert.Empty(t, stdout)
			assert.Contains(t, stderr, "unreachable", "越过 db_first 闸之后才会去连 Redis")
			assert.NotContains(t, stderr, "a real import is refused")
		})
	}
}

// TestRouteToolTiDBPrepareDoesNotTouchRedis:tidb-prepare 只碰全局库,不连 mapping Redis。两个地址都指向刚关掉的
// 不同端口:失败必须来自全局库(stderr 带 MySQL 的地址),而不是 Redis(不带 Redis 的地址、没有 Redis 的 unreachable 文案)。
// SnapshotMySQL.Host 必须显式写:不写会回落默认 127.0.0.1:3306,本机可能真有 MySQL。
func TestRouteToolTiDBPrepareDoesNotTouchRedis(t *testing.T) {
	redisAddr, mysqlAddr := twoClosedLocalAddrs(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, redisAddr)+fmt.Sprintf("SnapshotMySQL:\n  Host: %s\n", mysqlAddr))

	code, stdout, stderr := runCLI("-f", path, "tidb-prepare", "-zones=1,2")

	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, mysqlAddr, "失败应来自全局库")
	assert.NotContains(t, stderr, redisAddr, "tidb-prepare 不该去连 mapping Redis")
	assert.NotContains(t, stderr, "unreachable", "那是 mapping Redis 的文案")
}

// 建池失败时 stderr 括号里的提示按原因分流(store.OpenRouteStoreHint,三个建 route store 的入口共用),
// 纯函数用例在 store 包的 TestOpenRouteStoreHintByCause。

// ── 第 1b-1 批:freeze / reconcile-from-cache / unfreeze 与 audit -frozen-recovery ──────────────

// TestRunSubcommandFlagsAreScoped:新子命令的参数互不串用(别的子命令的参数一律 flag provided but not defined),
// 取值越界同样退出码 2;-freeze-timeout 不得低于 store.RouteFreezeTimeoutMin。
func TestRunSubcommandFlagsAreScoped(t *testing.T) {
	for _, c := range []struct {
		name string
		args []string
		want string
	}{
		{"dry-run is not a freeze flag", []string{"-f", "x.yaml", "freeze", "-dry-run=false"}, "dry-run"},
		{"batch is not a freeze flag", []string{"-f", "x.yaml", "freeze", "-batch=10"}, "batch"},
		{"resolve-ties is reconcile only", []string{"-f", "x.yaml", "audit", "-resolve-ties=cache"}, "resolve-ties"},
		{"freeze-timeout is freeze / unfreeze only", []string{"-f", "x.yaml", "audit", "-freeze-timeout=5s"}, "freeze-timeout"},
		{"frozen-recovery is audit only", []string{"-f", "x.yaml", "import-from-redis", "-frozen-recovery"}, "frozen-recovery"},
		{"reason is freeze only", []string{"-f", "x.yaml", "unfreeze", "-confirm-audited", "-reason=x"}, "reason"},
		{"freeze-grace is reconcile only", []string{"-f", "x.yaml", "unfreeze", "-confirm-audited", "-freeze-grace=1s"}, "freeze-grace"},
		{"confirm-audited is unfreeze only", []string{"-f", "x.yaml", "reconcile-from-cache", "-confirm-audited"}, "confirm-audited"},
		{"acknowledged is unfreeze only", []string{"-f", "x.yaml", "freeze", "-cache-was-not-verified-acknowledged"}, "cache-was-not-verified-acknowledged"},
		{"resolve-ties value", []string{"-f", "x.yaml", "reconcile-from-cache", "-resolve-ties=db"}, "-resolve-ties=db"},
		{"negative freeze-grace", []string{"-f", "x.yaml", "reconcile-from-cache", "-freeze-grace=-1s"}, "-freeze-grace=-1s"},
		{"freeze-timeout below the floor", []string{"-f", "x.yaml", "freeze", "-freeze-timeout=4s"}, "-freeze-timeout=4s"},
		{"unfreeze freeze-timeout below the floor", []string{"-f", "x.yaml", "unfreeze", "-confirm-audited", "-freeze-timeout=1s"}, "-freeze-timeout=1s"},
		{"reason is not a tidb-prepare flag", []string{"-f", "x.yaml", "tidb-prepare", "-zones=1", "-reason=x"}, "reason"},
		{"extra positional after freeze", []string{"-f", "x.yaml", "freeze", "now"}, "unexpected arguments"},
		// 2026-10-19:rewarm 在第 1b-2 批收尾时落地,原先这里钉的「rewarm is not a subcommand yet」改为钉它的参数作用域
		// (rewarm 自身的参数校验见 TestRunRewarmFlags)。
		{"wait is rewarm only", []string{"-f", "x.yaml", "audit", "-wait=1m"}, "wait"},
	} {
		t.Run(c.name, func(t *testing.T) {
			code, stdout, stderr := runCLI(c.args...)
			assert.Equal(t, routetool.ExitFailed, code)
			assert.Empty(t, stdout)
			assert.Contains(t, stderr, c.want)
			assert.Contains(t, stderr, "用法:")
		})
	}
}

// TestRunReconcileAndUnfreezeRefusedInRedisMode:yaml 是 redis 模式时 reconcile-from-cache / audit -frozen-recovery
// 在连任何东西之前就退出码 2(库不是真源,没有可修的);freeze 允许,但打 WARN(redis 模式的 W1 不读 writable)。
// 2026-10-18 修复评审起 unfreeze 在 redis 模式下改为「没有 -cache-was-not-verified-acknowledged 才拒绝」(同样在连任何东西之前),
// 放行的那一半见 TestRunFreezeThenUnfreezeInRedisMode。
func TestRunReconcileAndUnfreezeRefusedInRedisMode(t *testing.T) {
	addr := closedLocalAddr(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, addr)) // 不写 RouteStore 段 = redis 模式
	for _, args := range [][]string{
		{"-f", path, "reconcile-from-cache"},
		{"-f", path, "reconcile-from-cache", "-dry-run=false"},
		{"-f", path, "unfreeze", "-confirm-audited"},
		{"-f", path, "audit", "-frozen-recovery"},
	} {
		t.Run(strings.Join(args[2:], " "), func(t *testing.T) {
			code, stdout, stderr := runCLI(args...)
			assert.Equal(t, routetool.ExitFailed, code)
			assert.Empty(t, stdout)
			assert.Contains(t, stderr, "RouteStore.Mode=redis")
			assert.NotContains(t, stderr, "unreachable", "必须在连 Redis 之前就拒绝")
		})
	}

	t.Run("freeze 允许_打WARN", func(t *testing.T) {
		st := &fakeStore{meta: store.RouteMeta{Generation: testGen, Writable: 1}}
		code, stdout, stderr := runCLIWith(fakeDeps(st), "-f", path, "freeze", "-reason=drill")
		assert.Equal(t, routetool.ExitClean, code, stderr)
		assert.Equal(t, "route_freeze changed=true writable=0\n", stdout)
		assert.Contains(t, stderr, "WARN: RouteStore.Mode=redis")
		assert.Equal(t, 1, st.freezeCalls)
	})
}

// TestRunFreezeThenUnfreezeInRedisMode(2026-10-18 修复评审):redis 模式下误跑 / 演练过 freeze 之后,工具自己能撤销 ——
// import-from-redis 的 requireRouteWritable 与模式无关,没有这条路径就只能手写 UPDATE route_meta。
// 没有 -cache-was-not-verified-acknowledged 退出码 2、不解冻;加上之后解冻,最低水位 0;两次都不连 mapping Redis
// (地址不可达,连了就是 unreachable)、不读身份(fakeDeps 的身份读取会报错)。MappingRedis.Type 不是 node 也不挡(不读标记)。
func TestRunFreezeThenUnfreezeInRedisMode(t *testing.T) {
	addr := closedLocalAddr(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, addr)) // 不写 RouteStore 段 = redis 模式
	st := &fakeStore{meta: store.RouteMeta{Generation: testGen, Writable: 1}}

	code, _, stderr := runCLIWith(fakeDeps(st), "-f", path, "freeze", "-reason=drill")
	require.Equal(t, routetool.ExitClean, code, stderr)
	require.Equal(t, uint64(0), st.meta.Writable)

	code, stdout, stderr := runCLIWith(fakeDeps(st), "-f", path, "unfreeze", "-confirm-audited")
	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Empty(t, st.unfreezeCalls, "没有显式确认不许解冻")
	assert.Contains(t, stderr, "RouteStore.Mode=redis")
	assert.Contains(t, stderr, "-cache-was-not-verified-acknowledged")
	assert.NotContains(t, stderr, "unreachable")

	code, stdout, stderr = runCLIWith(fakeDeps(st), "-f", path, "unfreeze", "-confirm-audited", "-cache-was-not-verified-acknowledged")
	assert.Equal(t, routetool.ExitClean, code, stderr)
	assert.Equal(t, []uint64{0}, st.unfreezeCalls)
	assert.Equal(t, uint64(1), st.meta.Writable)
	assert.Equal(t, "route_unfreeze changed=true already_writable=false marker=not_read_redis_mode generation_realigned=false min_watermark_ms=0\n", stdout)
	assert.Contains(t, stderr, "WARN: RouteStore.Mode=redis")
	assert.NotContains(t, stderr, "unreachable", "redis 模式的 unfreeze 不连 mapping Redis")
	assert.NotContains(t, stderr, "identity not scripted")

	clusterPath := writeYaml(t, strings.Replace(fmt.Sprintf(minimalYaml, addr), "Type: node", "Type: cluster", 1))
	st.meta.Writable = 0
	code, _, stderr = runCLIWith(fakeDeps(st), "-f", clusterPath, "unfreeze", "-confirm-audited", "-cache-was-not-verified-acknowledged")
	assert.Equal(t, routetool.ExitClean, code, stderr)
	assert.NotContains(t, stderr, "need a single instance")
}

// TestRunUnfreezeNeedsConfirmAudited:unfreeze 没有 -confirm-audited 就退出码 2(参数阶段,连 Redis 之前);
// 带上之后越过这道闸,走到连 Redis(这里地址不可达 → unreachable)。
func TestRunUnfreezeNeedsConfirmAudited(t *testing.T) {
	addr := closedLocalAddr(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, addr)+dbFirstYaml)

	code, stdout, stderr := runCLI("-f", path, "unfreeze")
	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, "-confirm-audited")
	assert.Contains(t, stderr, "用法:")
	assert.NotContains(t, stderr, "unreachable")

	code, stdout, stderr = runCLI("-f", path, "unfreeze", "-confirm-audited")
	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, "unreachable", "越过参数闸之后才会去连 Redis")
}

// TestRunFreezeUnreachableDBExitsTwo:freeze 只碰全局库、不连 mapping Redis(冻结是止血动作,Redis 坏了也要能冻);
// 全局库不可达 → 退出码 2,stderr 带 MySQL 的地址、不带 Redis 的。
func TestRunFreezeUnreachableDBExitsTwo(t *testing.T) {
	redisAddr, mysqlAddr := twoClosedLocalAddrs(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, redisAddr)+dbFirstYaml+fmt.Sprintf("SnapshotMySQL:\n  Host: %s\n", mysqlAddr))

	code, stdout, stderr := runCLI("-f", path, "freeze", "-reason=it")

	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, mysqlAddr, "失败应来自全局库")
	assert.NotContains(t, stderr, redisAddr, "freeze 不该去连 mapping Redis")
}

// TestRunStdoutCarriesOnlyTheSummary:stdout 只有一行统计;store 打的 logx 日志(这里由假库打)进 stderr,不进 stdout。
func TestRunStdoutCarriesOnlyTheSummary(t *testing.T) {
	path := writeYaml(t, fmt.Sprintf(minimalYaml, closedLocalAddr(t))+dbFirstYaml)
	st := &fakeStore{meta: store.RouteMeta{Generation: testGen, Writable: 1}, logOnFreeze: "fake store: freeze log line"}

	code, stdout, stderr := runCLIWith(fakeDeps(st), "-f", path, "freeze")

	assert.Equal(t, routetool.ExitClean, code, stderr)
	assert.Equal(t, "route_freeze changed=true writable=0\n", stdout, "stdout 恰好一行统计")
	assert.Contains(t, stderr, "fake store: freeze log line", "logx 改写到了 stderr")
}

// TestRunUnfreezeRefusesWithoutVerifiedMarker:经入口跑 unfreeze —— 没有就绪标记、或标记里的身份与当前 Redis 不符 → 退出码 2、
// 不解冻,stderr 指向 runbook §9.2 的清单重放与显式确认开关;加 -cache-was-not-verified-acknowledged 才放行(最低水位 0);
// 标记可信时不必确认,最低水位取标记的 watermark_ms。身份由注入的 deps 给出(miniredis 不支持 INFO server / replication)。
func TestRunUnfreezeRefusesWithoutVerifiedMarker(t *testing.T) {
	current := routetool.RedisIdentity{RunID: strings.Repeat("ab", 20), MasterReplID: strings.Repeat("cd", 20)}
	marker := func(t *testing.T, id routetool.RedisIdentity, wm uint64) string {
		t.Helper()
		raw, err := placement.EncodeRouteReadyMarker(placement.RouteReadyMarker{
			Generation:   testGen,
			RedisRunID:   id.RunID,
			MasterReplID: id.MasterReplID,
			SetAtMs:      wm + 1,
			WatermarkMs:  wm,
			Leader:       "route_leader@it:0a1b2c3d",
		})
		require.NoError(t, err)
		return raw
	}
	setup := func(t *testing.T) (*miniredis.Miniredis, string, *fakeStore, deps) {
		t.Helper()
		mr := miniredis.RunT(t)
		path := writeYaml(t, fmt.Sprintf(minimalYaml, mr.Addr())+dbFirstYaml)
		st := &fakeStore{meta: store.RouteMeta{Generation: testGen, Writable: 0}}
		d := fakeDeps(st)
		d.redisIdentity = func(context.Context, *redis.Redis) (routetool.RedisIdentity, error) { return current, nil }
		return mr, path, st, d
	}

	t.Run("没有标记_拒绝", func(t *testing.T) {
		_, path, st, d := setup(t)
		code, stdout, stderr := runCLIWith(d, "-f", path, "unfreeze", "-confirm-audited")
		assert.Equal(t, routetool.ExitFailed, code)
		assert.Empty(t, stdout)
		assert.Empty(t, st.unfreezeCalls, "确认不了缓存就不许解冻")
		assert.Contains(t, stderr, "§9.2")
		assert.Contains(t, stderr, "-cache-was-not-verified-acknowledged")
	})
	t.Run("身份不符_拒绝", func(t *testing.T) {
		mr, path, st, d := setup(t)
		other := routetool.RedisIdentity{RunID: strings.Repeat("01", 20), MasterReplID: current.MasterReplID}
		require.NoError(t, mr.Set(placement.RouteReadyKey, marker(t, other, 42)))
		code, _, stderr := runCLIWith(d, "-f", path, "unfreeze", "-confirm-audited")
		assert.Equal(t, routetool.ExitFailed, code)
		assert.Empty(t, st.unfreezeCalls)
		assert.Contains(t, stderr, "another Redis instance")
	})
	t.Run("显式确认_放行_水位0", func(t *testing.T) {
		_, path, st, d := setup(t)
		code, stdout, stderr := runCLIWith(d, "-f", path, "unfreeze", "-confirm-audited", "-cache-was-not-verified-acknowledged")
		assert.Equal(t, routetool.ExitClean, code, stderr)
		assert.Equal(t, []uint64{0}, st.unfreezeCalls)
		assert.Contains(t, stdout, "marker=absent")
	})
	t.Run("标记可信_不必确认_水位取标记", func(t *testing.T) {
		mr, path, st, d := setup(t)
		require.NoError(t, mr.Set(placement.RouteReadyKey, marker(t, current, 1_760_000_000_000)))
		code, stdout, stderr := runCLIWith(d, "-f", path, "unfreeze", "-confirm-audited")
		assert.Equal(t, routetool.ExitClean, code, stderr)
		assert.Equal(t, []uint64{1_760_000_000_000}, st.unfreezeCalls)
		assert.Contains(t, stdout, "marker=verified")
	})
}

// TestRunAuditFrozenRecoveryNeedsFrozenWriters:audit -frozen-recovery 在映射写者没冻结时退出码 2,而且在审计开始之前就拒绝
// (假库的扫描方法没实现,被调到会 panic);可写时库独有的行与「Redis 丢了已确认的发布」分不开,不能放过 db_only_unpublished。
func TestRunAuditFrozenRecoveryNeedsFrozenWriters(t *testing.T) {
	mr := miniredis.RunT(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, mr.Addr())+dbFirstYaml)
	st := &fakeStore{meta: store.RouteMeta{Generation: testGen, Writable: 1}}

	code, stdout, stderr := runCLIWith(fakeDeps(st), "-f", path, "audit", "-frozen-recovery")

	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, "-frozen-recovery only applies while the route writers are frozen")
}

// ── 第 1b-2 批收尾(2026-10-19):rewarm 与 audit -require-maprev ──────────────────────────────

// TestRunRewarmFlags:rewarm 的参数作用域与取值校验 —— 一律退出码 2、打印用法、不连任何东西。
// -wait 缺省 0;-wait > 0 时 -poll 必须为正、-wait 必须小于 -timeout(-timeout 缺省 30m)。
func TestRunRewarmFlags(t *testing.T) {
	for _, c := range []struct {
		name string
		args []string
		want string
	}{
		{"negative wait", []string{"-f", "x.yaml", "rewarm", "-wait=-1s"}, "-wait=-1s"},
		{"zero poll while waiting", []string{"-f", "x.yaml", "rewarm", "-wait=1m", "-poll=0s"}, "-poll=0s"},
		{"wait not shorter than timeout", []string{"-f", "x.yaml", "rewarm", "-wait=30m"}, "-wait=30m0s"},
		{"batch is not a rewarm flag", []string{"-f", "x.yaml", "rewarm", "-batch=10"}, "batch"},
		{"confirm-audited is not a rewarm flag", []string{"-f", "x.yaml", "rewarm", "-confirm-audited"}, "confirm-audited"},
		{"require-maprev is audit only", []string{"-f", "x.yaml", "rewarm", "-require-maprev"}, "require-maprev"},
		{"poll is rewarm only", []string{"-f", "x.yaml", "unfreeze", "-confirm-audited", "-poll=1s"}, "poll"},
		{"extra positional after rewarm", []string{"-f", "x.yaml", "rewarm", "now"}, "unexpected arguments"},
	} {
		t.Run(c.name, func(t *testing.T) {
			code, stdout, stderr := runCLI(c.args...)
			assert.Equal(t, routetool.ExitFailed, code)
			assert.Empty(t, stdout)
			assert.Contains(t, stderr, c.want)
			assert.Contains(t, stderr, "用法:")
		})
	}
}

// rewarmMarker 编一份 set_at_ms = setAtMs 的合规就绪标记。
func rewarmMarker(t *testing.T, setAtMs uint64) string {
	t.Helper()
	raw, err := placement.EncodeRouteReadyMarker(placement.RouteReadyMarker{
		Generation:   testGen,
		RedisRunID:   strings.Repeat("ab", 20),
		MasterReplID: strings.Repeat("cd", 20),
		SetAtMs:      setAtMs,
		WatermarkMs:  setAtMs - 1,
		Leader:       "route_leader@it:0a1b2c3d",
	})
	require.NoError(t, err)
	return raw
}

// presetControlKeys 写满三把控制键,外加一把不相干的映射键(rewarm 不许碰它)。
func presetControlKeys(t *testing.T, mr *miniredis.Miniredis, markerSetAtMs uint64) {
	t.Helper()
	wm, err := placement.EncodeRouteWatermark(placement.RouteWatermark{Generation: testGen, WatermarkMs: markerSetAtMs})
	require.NoError(t, err)
	require.NoError(t, mr.Set(placement.RouteReadyKey, rewarmMarker(t, markerSetAtMs)))
	require.NoError(t, mr.Set(placement.RouteWarmKey, strings.Repeat("0a", 16)))
	require.NoError(t, mr.Set(placement.RouteWatermarkKey, wm))
	require.NoError(t, mr.Set(placement.HomeZoneKey(1), "1"))
}

// TestRunRewarmDropsKeysWithoutTouchingTheDB:-wait=0(缺省)只连 mapping Redis、一把全局库都不开(回退 redis 模式时全局库可能已不归它管),
// 两种模式都允许;三把控制键没了,不相干的映射键还在;stdout 恰好一行统计。
func TestRunRewarmDropsKeysWithoutTouchingTheDB(t *testing.T) {
	for _, mode := range []struct{ name, yamlTail string }{{"redis模式", ""}, {"db_first", dbFirstYaml}} {
		t.Run(mode.name, func(t *testing.T) {
			mr := miniredis.RunT(t)
			presetControlKeys(t, mr, 1_760_000_000_000)
			path := writeYaml(t, fmt.Sprintf(minimalYaml, mr.Addr())+mode.yamlTail)
			opened := 0
			d := fakeDeps(&fakeStore{})
			d.openRouteStore = func(config.Config) (routeStore, error) {
				opened++
				return nil, fmt.Errorf("must not open the global DB")
			}

			code, stdout, stderr := runCLIWith(d, "-f", path, "rewarm")

			assert.Equal(t, routetool.ExitClean, code, stderr)
			assert.Zero(t, opened, "-wait=0 不开全局库")
			assert.Equal(t, "route_rewarm deleted=3 marker_was=decoded waited=false rearmed=false baseline_set_at_ms=0 new_set_at_ms=0\n", stdout)
			for _, k := range []string{placement.RouteReadyKey, placement.RouteWarmKey, placement.RouteWatermarkKey} {
				assert.False(t, mr.Exists(k), k)
			}
			assert.True(t, mr.Exists(placement.HomeZoneKey(1)), "映射键不归 rewarm 管")
			assert.Contains(t, stderr, "DROPPED ")
		})
	}
}

// TestRunRewarmWaitRefusedInRedisMode:redis 模式下 -wait > 0 在连任何东西之前退出码 2(没有 leader 重挂);
// MappingRedis 不是 node 时 rewarm 一律退出码 2(三把控制键一条 DEL,cluster 下会 CROSSSLOT)。
func TestRunRewarmWaitRefusedInRedisMode(t *testing.T) {
	addr := closedLocalAddr(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, addr)) // 不写 RouteStore 段 = redis 模式

	code, stdout, stderr := runCLI("-f", path, "rewarm", "-wait=1m")
	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, "RouteStore.Mode=redis")
	assert.Contains(t, stderr, "-wait=0")
	assert.NotContains(t, stderr, "unreachable", "必须在连 Redis 之前就拒绝")

	clusterPath := writeYaml(t, strings.Replace(fmt.Sprintf(minimalYaml, addr), "Type: node", "Type: cluster", 1)+dbFirstYaml)
	code, stdout, stderr = runCLI("-f", clusterPath, "rewarm")
	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, "single instance")
	assert.NotContains(t, stderr, "unreachable")
}

// TestRunRewarmWaitRefusedWhileFrozen:db_first 下 -wait > 0 而映射写者冻结着 → 退出码 2,三把键都不动(冻结期 leader 不预热,等下去必然超时)。
func TestRunRewarmWaitRefusedWhileFrozen(t *testing.T) {
	mr := miniredis.RunT(t)
	presetControlKeys(t, mr, 1_760_000_000_000)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, mr.Addr())+dbFirstYaml)
	st := &fakeStore{meta: store.RouteMeta{Generation: testGen, Writable: 0}}

	code, stdout, stderr := runCLIWith(fakeDeps(st), "-f", path, "rewarm", "-wait=1m", "-poll=1s")

	assert.Equal(t, routetool.ExitFailed, code)
	assert.Empty(t, stdout)
	assert.Contains(t, stderr, "frozen")
	for _, k := range []string{placement.RouteReadyKey, placement.RouteWarmKey, placement.RouteWatermarkKey} {
		assert.True(t, mr.Exists(k), k)
	}
}

// TestRunRewarmWaitSeesRearmedMarker:db_first 下 -wait > 0 —— 撤键之后「leader」(这里是一个 goroutine:看到令牌没了,
// 就挂一份 set_at_ms 更大的新标记)重挂,rewarm 退出码 0,stdout 记下基线与新标记的 set_at_ms;基线取被撤标记,不读库时钟。
func TestRunRewarmWaitSeesRearmedMarker(t *testing.T) {
	const oldSetAt = uint64(1_760_000_000_000)
	mr := miniredis.RunT(t)
	presetControlKeys(t, mr, oldSetAt)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, mr.Addr())+dbFirstYaml)
	st := &fakeStore{meta: store.RouteMeta{Generation: testGen, Writable: 1}}

	rearmed := rewarmMarker(t, oldSetAt+1) // 在测试 goroutine 里编好:require 不能在别的 goroutine 里 FailNow
	leaderDone := make(chan struct{})
	go func() {
		defer close(leaderDone)
		deadline := time.Now().Add(5 * time.Second)
		for mr.Exists(placement.RouteWarmKey) {
			if time.Now().After(deadline) {
				return
			}
			time.Sleep(time.Millisecond)
		}
		_ = mr.Set(placement.RouteReadyKey, rearmed)
	}()

	code, stdout, stderr := runCLIWith(fakeDeps(st), "-f", path, "rewarm", "-wait=10s", "-poll=5ms", "-timeout=1m")
	<-leaderDone

	assert.Equal(t, routetool.ExitClean, code, stderr)
	assert.Equal(t, fmt.Sprintf("route_rewarm deleted=3 marker_was=decoded waited=true rearmed=true baseline_set_at_ms=%d new_set_at_ms=%d\n",
		oldSetAt, oldSetAt+1), stdout)
	assert.Zero(t, st.clockCalls, "被撤标记可解码时基线取它的 set_at_ms,不读库时钟")
	assert.Contains(t, stderr, "REARMED ")
}

// TestRunRewarmWaitTimesOutExitsOne:db_first 下没有 leader 重挂 → 到 -wait 退出码 1,stdout 照出统计行(rearmed=false),
// stderr 指向 ArmReadyMarker 与 leader 指标;被撤时没有标记 → 基线取库时钟(读一次)。
func TestRunRewarmWaitTimesOutExitsOne(t *testing.T) {
	mr := miniredis.RunT(t)
	path := writeYaml(t, fmt.Sprintf(minimalYaml, mr.Addr())+dbFirstYaml)
	st := &fakeStore{meta: store.RouteMeta{Generation: testGen, Writable: 1}, dbNowMs: 1_760_000_000_000}

	code, stdout, stderr := runCLIWith(fakeDeps(st), "-f", path, "rewarm", "-wait=30ms", "-poll=10ms")

	assert.Equal(t, routetool.ExitAttention, code, stderr)
	assert.Equal(t, "route_rewarm deleted=0 marker_was=absent waited=true rearmed=false baseline_set_at_ms=1760000000000 new_set_at_ms=0\n", stdout)
	assert.Equal(t, 1, st.clockCalls)
	assert.Contains(t, stderr, "TIMEOUT")
	assert.Contains(t, stderr, "RouteStore.Reconcile.ArmReadyMarker")
	assert.Contains(t, stderr, "data_service_route_reconcile_leader")
}

// TestRunAuditRequireMapRevFlag:audit -require-maprev 接到 AuditOptions.RequireMapRev —— 缓存 maprev 落后于库 rev 的行默认口径不算差异
// (退出码 0、统计行没有 redis_rev_behind),严格口径退出码 1、统计行末尾 redis_rev_behind=1;redis 模式下连任何东西之前退出码 2。
func TestRunAuditRequireMapRevFlag(t *testing.T) {
	t.Run("redis模式_拒绝", func(t *testing.T) {
		path := writeYaml(t, fmt.Sprintf(minimalYaml, closedLocalAddr(t)))
		code, stdout, stderr := runCLI("-f", path, "audit", "-require-maprev")
		assert.Equal(t, routetool.ExitFailed, code)
		assert.Empty(t, stdout)
		assert.Contains(t, stderr, "RouteStore.Mode=redis")
		assert.NotContains(t, stderr, "unreachable")
	})

	mr := miniredis.RunT(t)
	require.NoError(t, mr.Set(placement.HomeZoneKey(1), "1"))
	require.NoError(t, mr.Set(placement.MapRevKey(1), "1"))
	path := writeYaml(t, fmt.Sprintf(minimalYaml, mr.Addr())+dbFirstYaml)
	ast := &auditFakeStore{
		fakeStore: &fakeStore{meta: store.RouteMeta{Generation: testGen, Writable: 1}},
		rows:      map[uint64]store.RouteRow{1: {PlayerID: 1, HomeZone: 1, Rev: 2}},
	}
	d := fakeDeps(ast.fakeStore)
	d.openRouteStore = func(config.Config) (routeStore, error) { return ast, nil }

	code, stdout, stderr := runCLIWith(d, "-f", path, "audit", "-recheck-delay=0s")
	assert.Equal(t, routetool.ExitClean, code, stderr)
	assert.NotContains(t, stdout, "redis_rev_behind", "默认口径统计行逐字不变")

	code, stdout, stderr = runCLIWith(d, "-f", path, "audit", "-require-maprev", "-recheck-delay=0s")
	assert.Equal(t, routetool.ExitAttention, code, stderr)
	assert.True(t, strings.HasSuffix(stdout, " redis_rev_behind=1\n"), stdout)
	assert.Contains(t, stderr, "DIFF kind=redis_rev_behind player=1 ")
}

// auditFakeStore 在 fakeStore 之上补 audit 要的三个只读方法(fakeStore 本身不实现它们:被调到就 panic,
// TestRunAuditFrozenRecoveryNeedsFrozenWriters 靠这一点证明门禁拦在审计之前)。
type auditFakeStore struct {
	*fakeStore
	rows map[uint64]store.RouteRow
}

func (f *auditFakeStore) ScanRoutes(_ context.Context, after store.RouteScanCursor, limit int) ([]store.RouteRow, error) {
	out := make([]store.RouteRow, 0, len(f.rows))
	for _, row := range f.rows {
		if after.Less(store.RouteScanCursorAt(row)) {
			out = append(out, row)
		}
	}
	sort.Slice(out, func(i, j int) bool { return store.RouteScanCursorAt(out[i]).Less(store.RouteScanCursorAt(out[j])) })
	if len(out) > limit {
		out = out[:limit]
	}
	return out, nil
}

func (f *auditFakeStore) GetRoutes(_ context.Context, ids []uint64) (map[uint64]store.RouteRow, error) {
	out := make(map[uint64]store.RouteRow)
	for _, id := range ids {
		if row, ok := f.rows[id]; ok {
			out[id] = row
		}
	}
	return out, nil
}

func (f *auditFakeStore) ListMergedInto(context.Context) ([]store.MergedIntoRow, error) {
	return nil, nil
}

// ── 替身与夹具 ────────────────────────────────────────────────────────────────

const testGen = "0123456789abcdef0123456789abcdef"

// fakeStore 是 routeStore 的假实现:只实现本文件用到的方法;其余方法落到嵌入的 nil 接口上,被调到就 panic ——
// 说明装配走错了路(例如 frozen-recovery 的门禁没拦住,审计照样开扫)。
type fakeStore struct {
	routeStore

	meta          store.RouteMeta
	freezeCalls   int
	unfreezeCalls []uint64
	// logOnFreeze 非空时 FreezeRouteWriters 用 logx 打这一行(模拟 store 自己的日志)。
	logOnFreeze string
	// dbNowMs 是 ListActiveMergeFences 回的库时钟(rewarm -wait 的基线);clockCalls 记调用次数。
	dbNowMs    uint64
	clockCalls int
}

func (f *fakeStore) ReadRouteMeta(context.Context) (store.RouteMeta, error) { return f.meta, nil }

func (f *fakeStore) FreezeRouteWriters(context.Context) (bool, error) {
	f.freezeCalls++
	if f.logOnFreeze != "" {
		logx.Error(f.logOnFreeze)
	}
	changed := f.meta.Writable != 0
	f.meta.Writable = 0
	return changed, nil
}

func (f *fakeStore) UnfreezeRouteWriters(_ context.Context, minWatermarkMs uint64) (bool, error) {
	f.unfreezeCalls = append(f.unfreezeCalls, minWatermarkMs)
	f.meta.Writable = 1
	return true, nil
}

func (f *fakeStore) Close() error { return nil }

// ListActiveMergeFences 只给 rewarm -wait 取库时钟(没有生效围栏)。dbNowMs 为 0 时按库时钟读不到回错(与真 store 同口径)。
func (f *fakeStore) ListActiveMergeFences(context.Context) (store.ActiveMergeFences, error) {
	f.clockCalls++
	if f.dbNowMs == 0 {
		return store.ActiveMergeFences{}, fmt.Errorf("fake store: database clock read as 0")
	}
	return store.ActiveMergeFences{DBNowMs: f.dbNowMs}, nil
}

// fakeDeps 用真 connectMappingRedis(miniredis 或不可达地址都行)、假库,身份读取默认报错(用例按需替换)。
func fakeDeps(st *fakeStore) deps {
	return deps{
		connectRedis:   connectMappingRedis,
		openRouteStore: func(config.Config) (routeStore, error) { return st, nil },
		redisIdentity: func(context.Context, *redis.Redis) (routetool.RedisIdentity, error) {
			return routetool.RedisIdentity{}, fmt.Errorf("identity not scripted in this test")
		},
	}
}

// minimalYaml 是能被 conf.Load 接受的最小配置(同 internal/config 的 minimalYaml),MappingRedis.Host 由用例填。
const minimalYaml = `
Name: dataservice.rpc
ListenOn: 127.0.0.1:0
MappingRedis:
  Host: %s
  Type: node
Regions:
  - Id: 1
    Zones: [1]
    Redis:
      Addrs:
        - 127.0.0.1:7001
`

// dbFirstYaml 追加在 minimalYaml 之后,把 RouteStore 切成 db_first。
const dbFirstYaml = "RouteStore:\n  Mode: db_first\n"

func writeYaml(t *testing.T, content string) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "data_service.yaml")
	require.NoError(t, os.WriteFile(path, []byte(content), 0o600))
	return path
}

// closedLocalAddr 返回一个刚关掉的本机端口:连它必然被拒绝。
func closedLocalAddr(t *testing.T) string {
	t.Helper()
	l, err := net.Listen("tcp", "127.0.0.1:0")
	require.NoError(t, err)
	addr := l.Addr().String()
	require.NoError(t, l.Close())
	return addr
}

// twoClosedLocalAddrs 返回两个不同的、刚关掉的本机端口(两个监听同时开着才保证端口不同,拿到地址后一起关掉)。
func twoClosedLocalAddrs(t *testing.T) (string, string) {
	t.Helper()
	a, err := net.Listen("tcp", "127.0.0.1:0")
	require.NoError(t, err)
	b, err := net.Listen("tcp", "127.0.0.1:0")
	require.NoError(t, err)
	first, second := a.Addr().String(), b.Addr().String()
	require.NoError(t, a.Close())
	require.NoError(t, b.Close())
	return first, second
}
