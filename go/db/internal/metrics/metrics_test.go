package metrics

import (
	"os"
	"path/filepath"
	"regexp"
	"testing"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/redis/go-redis/v9"
)

// TestSQLPoolCollectorDeclaresDbPools 钉住 go/db 的连接池类别:没有任何池时(nil source)三类各输出一组序列,
// 常量 label sql_client="db"。告警 DbExtraStorePoolsBeyondPlan 按 pool="extra_zone" / "extra_non_zone" 写死,
// 类别名一改,告警会静默落空 —— 这条用例让改名先在这里红。
func TestSQLPoolCollectorDeclaresDbPools(t *testing.T) {
	reg := prometheus.NewPedanticRegistry()
	if err := reg.Register(NewSQLPoolCollector(nil)); err != nil {
		t.Fatalf("register: %v", err)
	}
	families, err := reg.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	for _, mf := range families {
		if mf.GetName() != "mmorpg_sql_pool_pools" {
			continue
		}
		seen := map[string]float64{}
		for _, m := range mf.GetMetric() {
			var pool, client string
			for _, l := range m.GetLabel() {
				switch l.GetName() {
				case "pool":
					pool = l.GetValue()
				case "sql_client":
					client = l.GetValue()
				}
			}
			if client != "db" {
				t.Fatalf("pool %q has sql_client=%q, want db", pool, client)
			}
			seen[pool] = m.GetGauge().GetValue()
		}
		want := []string{PoolHome, PoolExtraZone, PoolExtraNonZone}
		if len(seen) != len(want) {
			t.Fatalf("pools = %v, want exactly %v", seen, want)
		}
		for _, pool := range want {
			if v, ok := seen[pool]; !ok || v != 0 {
				t.Fatalf("mmorpg_sql_pool_pools{pool=%q} = %v (present=%v), want 0", pool, v, ok)
			}
		}
		if PoolExtraZone != "extra_zone" || PoolExtraNonZone != "extra_non_zone" {
			t.Fatalf("pool class names are pinned by deploy/k8s/mysql-connection-alerts.yaml, got %q / %q",
				PoolExtraZone, PoolExtraNonZone)
		}
		return
	}
	t.Fatal("mmorpg_sql_pool_pools was not emitted")
}

// TestConsumerGroupHandoffSeriesPrecreated 钉住交接闸指标的预建序列:告警 DbConsumerGroupHandoffOwnBehindPeer /
// DbConsumerGroupHandoffConflict / DbConsumerGroupHandoffSeedAborted 用 increase(),序列从无到 1 时会漏掉第一次,
// 所以 register() 之后这些序列必须已经以 0 存在。outcome 集合是修订稿 R5 / R12 之后的 8 个(含 wait_peer_state_unknown,
// 不含已删除的 peer_unreachable_own_complete)。
func TestConsumerGroupHandoffSeriesPrecreated(t *testing.T) {
	register()
	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	// 按名字索引(存下标,不必引入 client_model 的类型名)。
	byName := map[string]int{}
	for i, mf := range families {
		byName[mf.GetName()] = i
	}
	labelValues := func(name, label string) map[string]float64 {
		i, ok := byName[name]
		if !ok {
			t.Fatalf("%s was not registered", name)
		}
		mf := families[i]
		out := map[string]float64{}
		for _, m := range mf.GetMetric() {
			for _, l := range m.GetLabel() {
				if l.GetName() == label {
					out[l.GetValue()] = m.GetCounter().GetValue()
				}
			}
		}
		return out
	}

	conflictIdx, ok := byName["db_consumer_group_handoff_conflict_total"]
	if !ok || len(families[conflictIdx].GetMetric()) != 1 {
		t.Fatal("db_consumer_group_handoff_conflict_total must exist as a single unlabeled series")
	}
	conflict := families[conflictIdx]
	if v := conflict.GetMetric()[0].GetCounter().GetValue(); v != 0 {
		t.Fatalf("db_consumer_group_handoff_conflict_total = %v, want 0", v)
	}

	gates := labelValues("db_consumer_group_handoff_gate_total", "outcome")
	wantGates := []string{
		"no_peer_offsets", "peer_released", "wait_peer_holding", "wait_peer_rebalancing",
		"wait_peer_state_unknown", "wait_own_behind_peer", "wait_peer_unavailable", "wait_own_unavailable",
	}
	if len(gates) != len(wantGates) {
		t.Fatalf("gate outcomes = %v, want exactly %v", gates, wantGates)
	}
	for _, outcome := range wantGates {
		if v, ok := gates[outcome]; !ok || v != 0 {
			t.Fatalf("db_consumer_group_handoff_gate_total{outcome=%q} = %v (present=%v), want 0", outcome, v, ok)
		}
	}
	if _, ok := gates["peer_unreachable_own_complete"]; ok {
		t.Fatal("peer_unreachable_own_complete was removed (addendum R5) and must not be pre-created")
	}

	results := labelValues("db_consumer_group_handoff_total", "result")
	for _, result := range []string{"seeded", "own_present", "no_offsets", "aborted"} {
		if v, ok := results[result]; !ok || v != 0 {
			t.Fatalf("db_consumer_group_handoff_total{result=%q} = %v (present=%v), want 0", result, v, ok)
		}
	}
	if len(results) != 4 {
		t.Fatalf("handoff results = %v, want exactly 4", results)
	}

	for _, name := range []string{
		"db_consumer_group_rebalance_gap_seconds",
		"db_consumer_group_handoff_waiting_seconds",
		"db_consumer_group_handoff_wait_seconds",
	} {
		if _, ok := byName[name]; !ok {
			t.Fatalf("%s was not registered", name)
		}
	}

	// 常量与 Help 一一对应(告警按这些字面量写):常量一改,这里先红。
	if HandoffGateWaitOwnBehindPeer != "wait_own_behind_peer" || HandoffSeedAborted != "aborted" {
		t.Fatalf("alert-pinned label values changed: %q / %q", HandoffGateWaitOwnBehindPeer, HandoffSeedAborted)
	}
}

// gatherByLabel 从注册表收集 family 的序列,按 label 的值索引(值取仪表或计数器)。
func gatherByLabel(t *testing.T, reg *prometheus.Registry, family, label string) map[string]float64 {
	t.Helper()
	families, err := reg.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	out := map[string]float64{}
	for _, mf := range families {
		if mf.GetName() != family {
			continue
		}
		for _, m := range mf.GetMetric() {
			for _, l := range m.GetLabel() {
				if l.GetName() != label {
					continue
				}
				if g := m.GetGauge(); g != nil {
					out[l.GetValue()] = g.GetValue()
				} else if c := m.GetCounter(); c != nil {
					out[l.GetValue()] = c.GetValue()
				}
			}
		}
	}
	return out
}

// TestStoreCooldownCollector_EmitsAllPools(launch-todo §2.3 第 11 条):三类连接池每次都各出一条序列,
// 没有冷却记录的类别为 0;source 返回 nil map 时三类全为 0。告警按 pool 的取值写,少一条就会静默落空。
func TestStoreCooldownCollector_EmitsAllPools(t *testing.T) {
	cases := []struct {
		name   string
		source map[string]int
		want   map[string]float64
	}{
		{name: "home cooling", source: map[string]int{PoolHome: 2},
			want: map[string]float64{PoolHome: 2, PoolExtraZone: 0, PoolExtraNonZone: 0}},
		{name: "nil map", source: nil,
			want: map[string]float64{PoolHome: 0, PoolExtraZone: 0, PoolExtraNonZone: 0}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			reg := prometheus.NewPedanticRegistry()
			source := tc.source
			if err := reg.Register(NewStoreCooldownCollector(func() map[string]int { return source })); err != nil {
				t.Fatalf("register: %v", err)
			}
			got := gatherByLabel(t, reg, "db_store_cooldown_stores", "pool")
			if len(got) != len(tc.want) {
				t.Fatalf("pools = %v, want %v", got, tc.want)
			}
			for pool, want := range tc.want {
				if v, ok := got[pool]; !ok || v != want {
					t.Fatalf("db_store_cooldown_stores{pool=%q} = %v (present=%v), want %v", pool, v, ok, want)
				}
			}
		})
	}
}

// fakeRedisPoolClient 返回固定的 PoolStats 与 Options。
type fakeRedisPoolClient struct {
	stats redis.PoolStats
	opts  redis.Options
}

func (f *fakeRedisPoolClient) PoolStats() *redis.PoolStats { s := f.stats; return &s }
func (f *fakeRedisPoolClient) Options() *redis.Options     { return &f.opts }

var redisPoolFamilies = []string{
	"db_redis_pool_hits_total", "db_redis_pool_misses_total", "db_redis_pool_timeouts_total", "db_redis_pool_wait_total",
	"db_redis_pool_wait_seconds_total", "db_redis_pool_unusable_total", "db_redis_pool_stale_conns_total",
	"db_redis_pool_conns", "db_redis_pool_idle_conns", "db_redis_pool_pending_requests", "db_redis_pool_max_conns",
}

// placement 与 main 是同一个对象(Placement.Redis 缺省)时只导出 redis_client=main 一份:汇总脚本跨 redis_client
// 求和,导出两份会把等待次数翻倍。
func TestRedisPoolCollector_SharedClientExportedOnce(t *testing.T) {
	shared := &fakeRedisPoolClient{stats: redis.PoolStats{Hits: 3, WaitCount: 5}, opts: redis.Options{PoolSize: 10}}
	reg := prometheus.NewPedanticRegistry()
	if err := reg.Register(NewRedisPoolCollector(shared, shared)); err != nil {
		t.Fatalf("register: %v", err)
	}
	for _, family := range redisPoolFamilies {
		got := gatherByLabel(t, reg, family, "redis_client")
		if len(got) != 1 {
			t.Fatalf("%s = %v, want exactly one series (redis_client=main)", family, got)
		}
		if _, ok := got[RedisClientMain]; !ok {
			t.Fatalf("%s = %v, want the series labelled redis_client=main", family, got)
		}
	}
	if v := gatherByLabel(t, reg, "db_redis_pool_wait_total", "redis_client")[RedisClientMain]; v != 5 {
		t.Fatalf("db_redis_pool_wait_total{main} = %v, want 5 (not doubled)", v)
	}
}

// 两个不同的 client 各出一组;计数、等待秒数(纳秒换算成秒)、池大小都按快照导出。nil 的 placement 不导出。
func TestRedisPoolCollector_SeparatePlacementClient(t *testing.T) {
	mainClient := &fakeRedisPoolClient{
		stats: redis.PoolStats{Hits: 7, Misses: 2, Timeouts: 1, WaitCount: 4, WaitDurationNs: 1_500_000_000,
			Unusable: 1, StaleConns: 3, TotalConns: 9, IdleConns: 6, PendingRequests: 2},
		opts: redis.Options{PoolSize: 10},
	}
	placementClient := &fakeRedisPoolClient{stats: redis.PoolStats{Hits: 11, WaitDurationNs: 250_000_000}, opts: redis.Options{PoolSize: 4}}
	reg := prometheus.NewPedanticRegistry()
	if err := reg.Register(NewRedisPoolCollector(mainClient, placementClient)); err != nil {
		t.Fatalf("register: %v", err)
	}
	expect := func(family, client string, want float64) {
		t.Helper()
		got, ok := gatherByLabel(t, reg, family, "redis_client")[client]
		if !ok || got != want {
			t.Fatalf("%s{redis_client=%q} = %v (present=%v), want %v", family, client, got, ok, want)
		}
	}
	expect("db_redis_pool_hits_total", RedisClientMain, 7)
	expect("db_redis_pool_hits_total", RedisClientPlacement, 11)
	expect("db_redis_pool_wait_seconds_total", RedisClientMain, 1.5)
	expect("db_redis_pool_wait_seconds_total", RedisClientPlacement, 0.25)
	expect("db_redis_pool_max_conns", RedisClientMain, 10)
	expect("db_redis_pool_max_conns", RedisClientPlacement, 4)
	expect("db_redis_pool_conns", RedisClientMain, 9)
	expect("db_redis_pool_idle_conns", RedisClientMain, 6)
	expect("db_redis_pool_pending_requests", RedisClientMain, 2)
	expect("db_redis_pool_stale_conns_total", RedisClientMain, 3)

	var nilPlacement *fakeRedisPoolClient
	reg2 := prometheus.NewPedanticRegistry()
	if err := reg2.Register(NewRedisPoolCollector(mainClient, nilPlacement)); err != nil {
		t.Fatalf("register: %v", err)
	}
	if got := gatherByLabel(t, reg2, "db_redis_pool_hits_total", "redis_client"); len(got) != 1 {
		t.Fatalf("a nil placement client must not be exported, got %v", got)
	}
}

// writePathRedisSteps 是一条写任务依次经过的 Redis 往返(G1 治标之后:核锁在复核之后、缓存回写之前)。
var writePathRedisSteps = []string{
	"lock_acquire", "cursor_get", "epoch_get", "placement_lookup", "expand_get", "placement_recheck",
	"lease_verify", "cache_set", "cursor_set", "epoch_mark", "lock_release",
}

// TestRedisStepNamesPinned 钉住 11 个 step 的名字与顺序:tools/scripts/stress_summarize.ps1 的 2e 段按这些名字
// 与顺序出列,改名 / 合并 step 必须同改脚本与 test_stress_summarize.ps1。
func TestRedisStepNamesPinned(t *testing.T) {
	if len(RedisSteps) != 11 {
		t.Fatalf("RedisSteps has %d entries, want 11: %v", len(RedisSteps), RedisSteps)
	}
	seen := map[string]bool{}
	for _, step := range RedisSteps {
		if seen[step] {
			t.Fatalf("duplicate step %q in %v", step, RedisSteps)
		}
		seen[step] = true
	}
	for i, want := range writePathRedisSteps {
		if RedisSteps[i] != want {
			t.Fatalf("RedisSteps[%d] = %q, want %q (write-path order %v)", i, RedisSteps[i], want, writePathRedisSteps)
		}
	}
}

// TestRedisStepNamesMatchStressSummarize 读 stress_summarize.ps1 的 $redisSteps 列表,与 RedisSteps 逐项比对:
// 两边不一致时脚本整列静默变成 n/a,只有这里能先红。
func TestRedisStepNamesMatchStressSummarize(t *testing.T) {
	path := filepath.Join("..", "..", "..", "..", "tools", "scripts", "stress_summarize.ps1")
	raw, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", path, err)
	}
	block := regexp.MustCompile(`(?s)\$redisSteps\s*=\s*@\(([^)]*)\)`).FindSubmatch(raw)
	if block == nil {
		t.Fatalf("$redisSteps list not found in %s", path)
	}
	var names []string
	for _, m := range regexp.MustCompile(`'([a-z_]+)'`).FindAllSubmatch(block[1], -1) {
		names = append(names, string(m[1]))
	}
	if len(names) != len(RedisSteps) {
		t.Fatalf("stress_summarize.ps1 lists %v, RedisSteps is %v", names, RedisSteps)
	}
	for i := range names {
		if names[i] != RedisSteps[i] {
			t.Fatalf("stress_summarize.ps1 step %d = %q, RedisSteps[%d] = %q", i, names[i], i, RedisSteps[i])
		}
	}
}

// TestPipelineCountersPrecreated:告警与汇总脚本用 increase() 读的计数器取值在 registerPipeline 之后必须已经存在。
func TestPipelineCountersPrecreated(t *testing.T) {
	registerPipeline()
	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	present := map[string]map[string]bool{}
	for _, mf := range families {
		for _, m := range mf.GetMetric() {
			for _, l := range m.GetLabel() {
				if present[mf.GetName()] == nil {
					present[mf.GetName()] = map[string]bool{}
				}
				present[mf.GetName()][l.GetName()+"="+l.GetValue()] = true
			}
			if len(m.GetLabel()) == 0 {
				if present[mf.GetName()] == nil {
					present[mf.GetName()] = map[string]bool{}
				}
				present[mf.GetName()][""] = true
			}
		}
	}
	want := map[string][]string{
		"db_ordering_guard_total": {"outcome=apply", "outcome=apply_no_cursor", "outcome=stale", "outcome=incomparable",
			"outcome=error"},
		"db_ordering_lease_lost_total": {"reason=owner_changed", "reason=renew_exhausted", "reason=renew_deadline",
			"reason=verify_error"},
		"db_ordering_lock_release_total": {"outcome=released", "outcome=not_owner", "outcome=error",
			"outcome=skipped_sql_abandoned"},
		"db_ordering_lease_renew_retry_total": {""},
	}
	for family, series := range want {
		for _, s := range series {
			if !present[family][s] {
				t.Fatalf("%s{%s} was not pre-created", family, s)
			}
		}
	}
}
