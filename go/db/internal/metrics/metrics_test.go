package metrics

import (
	"testing"

	"github.com/prometheus/client_golang/prometheus"
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
