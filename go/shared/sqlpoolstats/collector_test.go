package sqlpoolstats

import (
	"database/sql"
	"math"
	"strings"
	"testing"
	"time"

	"github.com/prometheus/client_golang/prometheus"
)

const testService = "unit"

// gatherPools 把一个只注册了本收集器的注册表抓一次,返回 指标名 → pool → 值;同时核对每条序列都带
// sql_client=testService 的常量 label(告警表达式按它分组,丢了就静默落空)。
// label 名写字面量,不引用实现里的 poolLabel / clientLabel:它们是告警表达式 sum by (namespace, sql_client, pool)
// 的契约(deploy/k8s/mysql-connection-alerts.yaml),不能跟着实现常量一起改。
func gatherPools(t *testing.T, c prometheus.Collector) map[string]map[string]float64 {
	t.Helper()
	reg := prometheus.NewPedanticRegistry()
	if err := reg.Register(c); err != nil {
		t.Fatalf("register: %v", err)
	}
	families, err := reg.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	out := map[string]map[string]float64{}
	for _, mf := range families {
		byPool := map[string]float64{}
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
			if client != testService {
				t.Fatalf("%s{pool=%q} has sql_client=%q, want %q", mf.GetName(), pool, client, testService)
			}
			if _, dup := byPool[pool]; dup {
				t.Fatalf("%s has two series for pool=%q", mf.GetName(), pool)
			}
			switch {
			case m.GetGauge() != nil:
				byPool[pool] = m.GetGauge().GetValue()
			case m.GetCounter() != nil:
				byPool[pool] = m.GetCounter().GetValue()
			default:
				t.Fatalf("%s{pool=%q} is neither a gauge nor a counter", mf.GetName(), pool)
			}
		}
		out[mf.GetName()] = byPool
	}
	return out
}

func fixedSource(samples ...Sample) Source {
	return func() []Sample { return samples }
}

// 同一类别的两个池逐项求和;等待时长换算成秒。
func TestCollectorSumsSamplesPerPool(t *testing.T) {
	src := fixedSource(
		Sample{Pool: "home", Stats: sql.DBStats{
			MaxOpenConnections: 8, OpenConnections: 3, InUse: 2, Idle: 1,
			WaitCount: 5, WaitDuration: time.Second,
			MaxIdleClosed: 1, MaxIdleTimeClosed: 2, MaxLifetimeClosed: 3,
		}},
		Sample{Pool: "home", Stats: sql.DBStats{
			MaxOpenConnections: 8, OpenConnections: 4, InUse: 1, Idle: 3,
			WaitCount: 7, WaitDuration: 500 * time.Millisecond,
			MaxIdleClosed: 10, MaxIdleTimeClosed: 20, MaxLifetimeClosed: 30,
		}},
	)
	got := gatherPools(t, NewCollector(testService, []string{"home"}, src))

	want := map[string]float64{
		"mmorpg_sql_pool_pools":                       2,
		"mmorpg_sql_pool_max_open_connections":        16,
		"mmorpg_sql_pool_open_connections":            7,
		"mmorpg_sql_pool_in_use_connections":          3,
		"mmorpg_sql_pool_idle_connections":            4,
		"mmorpg_sql_pool_wait_count_total":            12,
		"mmorpg_sql_pool_wait_duration_seconds_total": 1.5,
		"mmorpg_sql_pool_max_idle_closed_total":       11,
		"mmorpg_sql_pool_max_idle_time_closed_total":  22,
		"mmorpg_sql_pool_max_lifetime_closed_total":   33,
	}
	if len(got) != len(want) {
		t.Fatalf("got %d metric families, want %d: %v", len(got), len(want), got)
	}
	for name, v := range want {
		if g, ok := got[name]["home"]; !ok || g != v {
			t.Fatalf("%s{pool=home} = %v (present=%v), want %v", name, g, ok, v)
		}
	}

	// 等待时长之和与样本顺序无关:go/db 的 PoolSamples 遍历 map,同一组池每次抓取给出的顺序都不固定。
	// 按浮点秒累加时 (0.1+0.2)+0.3 = 0.6000000000000001、(0.3+0.2)+0.1 = 0.6,两次抓取差 1 ULP,
	// 变小的那次会被 Prometheus 当成 counter reset;按整数纳秒累加,两种顺序都恰为 0.6。
	const waitDuration = "mmorpg_sql_pool_wait_duration_seconds_total"
	ascending := fixedSource(
		Sample{Pool: "home", Stats: sql.DBStats{WaitDuration: 100 * time.Millisecond}},
		Sample{Pool: "home", Stats: sql.DBStats{WaitDuration: 200 * time.Millisecond}},
		Sample{Pool: "home", Stats: sql.DBStats{WaitDuration: 300 * time.Millisecond}},
	)
	descending := fixedSource(
		Sample{Pool: "home", Stats: sql.DBStats{WaitDuration: 300 * time.Millisecond}},
		Sample{Pool: "home", Stats: sql.DBStats{WaitDuration: 200 * time.Millisecond}},
		Sample{Pool: "home", Stats: sql.DBStats{WaitDuration: 100 * time.Millisecond}},
	)
	up := gatherPools(t, NewCollector(testService, []string{"home"}, ascending))[waitDuration]["home"]
	down := gatherPools(t, NewCollector(testService, []string{"home"}, descending))[waitDuration]["home"]
	if up != down || up != 0.6 {
		t.Fatalf("%s{pool=home} depends on sample order: ascending=%v descending=%v, want both 0.6", waitDuration, up, down)
	}
}

// 声明了的类别即使此刻没有池也输出一组 0(pools=0),面板与告警不会因为「还没打开过」而没有序列。
func TestCollectorPrimesDeclaredPoolsWithZeros(t *testing.T) {
	got := gatherPools(t, NewCollector(testService, []string{"a", "b"}, fixedSource()))
	if len(got) != 10 {
		t.Fatalf("got %d metric families, want 10: %v", len(got), got)
	}
	for name, byPool := range got {
		if len(byPool) != 2 {
			t.Fatalf("%s has pools %v, want exactly a and b (other must not be primed)", name, byPool)
		}
		for _, pool := range []string{"a", "b"} {
			v, ok := byPool[pool]
			if !ok || v != 0 {
				t.Fatalf("%s{pool=%q} = %v (present=%v), want 0", name, pool, v, ok)
			}
		}
	}

	// source 为 nil 与返回空同语义。
	nilSource := gatherPools(t, NewCollector(testService, []string{"a"}, nil))
	if v, ok := nilSource["mmorpg_sql_pool_pools"]["a"]; !ok || v != 0 {
		t.Fatalf("nil source: pools{pool=a} = %v (present=%v), want 0", v, ok)
	}
}

// 未声明的类别(调用方笔误)并进 pool="other",不扩张 label 基数;声明的类别不受影响。
func TestCollectorFoldsUndeclaredPoolIntoOther(t *testing.T) {
	src := fixedSource(
		Sample{Pool: "typo_1", Stats: sql.DBStats{MaxOpenConnections: 4, OpenConnections: 1}},
		Sample{Pool: "typo_2", Stats: sql.DBStats{MaxOpenConnections: 6, OpenConnections: 2}},
	)
	got := gatherPools(t, NewCollector(testService, []string{"a"}, src))
	pools := got["mmorpg_sql_pool_pools"]
	if len(pools) != 2 || pools["a"] != 0 || pools[OtherPool] != 2 {
		t.Fatalf("pools = %v, want a=0 and other=2", pools)
	}
	if v := got["mmorpg_sql_pool_max_open_connections"][OtherPool]; v != 10 {
		t.Fatalf("max_open_connections{pool=other} = %v, want 10", v)
	}
	for name, byPool := range got {
		for pool := range byPool {
			if strings.HasPrefix(pool, "typo") {
				t.Fatalf("%s leaked the undeclared pool name %q into a label", name, pool)
			}
		}
	}
}

// database/sql 的 MaxOpenConnections=0 表示不限:这一类的上限是 +Inf,不能被求成一个看着有限的数。
func TestCollectorReportsUnlimitedPoolAsInf(t *testing.T) {
	src := fixedSource(
		Sample{Pool: "a", Stats: sql.DBStats{MaxOpenConnections: 8}},
		Sample{Pool: "a", Stats: sql.DBStats{MaxOpenConnections: 0}},
		Sample{Pool: "b", Stats: sql.DBStats{MaxOpenConnections: 5}},
	)
	got := gatherPools(t, NewCollector(testService, []string{"a", "b"}, src))
	if v := got["mmorpg_sql_pool_max_open_connections"]["a"]; !math.IsInf(v, 1) {
		t.Fatalf("max_open_connections{pool=a} = %v, want +Inf", v)
	}
	if v := got["mmorpg_sql_pool_max_open_connections"]["b"]; v != 5 {
		t.Fatalf("max_open_connections{pool=b} = %v, want 5", v)
	}
}

// source panic:本次抓取本收集器一条都不输出(不是输出 0),Gather 不报错,进程不被带崩。
func TestCollectorSkipsScrapeWhenSourcePanics(t *testing.T) {
	src := func() []Sample { panic("boom") }
	got := gatherPools(t, NewCollector(testService, []string{"a"}, src))
	for name := range got {
		if strings.HasPrefix(name, "mmorpg_sql_pool_") {
			t.Fatalf("a panicking source must suppress the whole scrape, but %s was emitted: %v", name, got)
		}
	}
}

// Pedantic 注册表会核对 Describe 与 Collect 一致、常量 label 合法、同一抓取内无重复序列。
func TestCollectorIsPedanticallyConsistent(t *testing.T) {
	src := fixedSource(
		Sample{Pool: "a", Stats: sql.DBStats{MaxOpenConnections: 3, OpenConnections: 2, InUse: 1, Idle: 1}},
		Sample{Pool: "unknown", Stats: sql.DBStats{MaxOpenConnections: 1}},
	)
	c := NewCollector(testService, []string{"a", "b"}, src)
	got := gatherPools(t, c)
	if len(got) != 10 {
		t.Fatalf("got %d metric families, want 10", len(got))
	}
	// 同一个收集器实例抓两次,结果逐条一致:累加器是 Collect 内的局部变量,不残留上一次抓取的累加值。
	// (gatherPools 每次新建注册表,所以两次抓取之间共享的只有收集器本身。)
	again := gatherPools(t, c)
	if len(again) != len(got) {
		t.Fatalf("second scrape of the same collector has %d metric families, first had %d", len(again), len(got))
	}
	for name, byPool := range got {
		for pool, v := range byPool {
			if w, ok := again[name][pool]; !ok || w != v {
				t.Fatalf("second scrape of the same collector disagrees on %s{pool=%q}: %v vs %v (present=%v)", name, pool, v, w, ok)
			}
		}
	}
}

func TestNewCollectorRejectsInvalidPools(t *testing.T) {
	cases := []struct {
		name    string
		service string
		pools   []string
	}{
		{name: "empty service", service: "", pools: []string{"a"}},
		{name: "empty pool", service: testService, pools: []string{"a", ""}},
		{name: "duplicate pool", service: testService, pools: []string{"a", "a"}},
		{name: "reserved other", service: testService, pools: []string{OtherPool}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			defer func() {
				if recover() == nil {
					t.Fatalf("NewCollector(%q, %v) must panic", tc.service, tc.pools)
				}
			}()
			NewCollector(tc.service, tc.pools, nil)
		})
	}
}
