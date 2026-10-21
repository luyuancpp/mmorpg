package main

import (
	"context"
	"database/sql/driver"
	"errors"
	"testing"

	"github.com/go-sql-driver/mysql"
)

func TestCurrentTiDBDSNReplacesHistoricalPoliciesIncludingCaseAliases(t *testing.T) {
	for name, build := range map[string]func(string) (string, error){"业务池": readCommittedDSN, "路由池": routeDBDSN} {
		t.Run(name, func(t *testing.T) {
			input := "fixture@tcp(127.0.0.1:4000)/fixture?parseTime=true&tidb_snapshot=%27123%27&TIDB_SNAPSHOT=%27456%27&tidb_read_staleness=-60&tidb_low_resolution_tso=ON&tidb_replica_read=%27follower%27&tidb_read_consistency=%27weak%27&tidb_txn_mode=%27optimistic%27&TX_ISOLATION=%27REPEATABLE-READ%27"
			out, err := build(input)
			if err != nil {
				t.Fatal(err)
			}
			cfg, err := mysql.ParseDSN(out)
			if err != nil {
				t.Fatal(err)
			}
			if cfg.DBName != "fixture" || cfg.Addr != "127.0.0.1:4000" || !cfg.ParseTime {
				t.Fatal("强读规范化改坏连接目标/编码选项")
			}
			for key, want := range map[string]string{"transaction_isolation": "'READ-COMMITTED'", "tidb_txn_mode": "'pessimistic'", "tidb_replica_read": "'leader'", "tidb_read_consistency": "'strict'", "tidb_snapshot": "''", "tidb_read_staleness": "0", "tidb_low_resolution_tso": "OFF"} {
				if cfg.Params[key] != want {
					t.Fatalf("%s未固定到强读策略", key)
				}
			}
			if _, ok := cfg.Params["TIDB_SNAPSHOT"]; ok {
				t.Fatal("大小写历史快照别名仍存活")
			}
			if _, ok := cfg.Params["TX_ISOLATION"]; ok {
				t.Fatal("大小写旧隔离级别别名仍存活")
			}
			if again, err := build(out); err != nil || again != out {
				t.Fatal("规范化不幂等")
			}
		})
	}
}

// 应答脚本只验证启动失败语义；真正强TiDB会话与被污染DSN的覆盖由merge_integration验收。
func TestCurrentTiDBStartupRefusesUnsafeReadback(t *testing.T) {
	for _, c := range []struct {
		name string
		row  []driver.Value
	}{
		{"历史快照", []driver.Value{"pessimistic", "READ-COMMITTED", "leader", "strict", "123", int64(0), int64(0)}},
		{"副本旧读", []driver.Value{"pessimistic", "READ-COMMITTED", "follower", "strict", "", int64(0), int64(0)}},
		{"时间旧读", []driver.Value{"pessimistic", "READ-COMMITTED", "leader", "strict", "", int64(-5), int64(0)}},
		{"低精度TSO", []driver.Value{"pessimistic", "READ-COMMITTED", "leader", "strict", "", int64(0), int64(1)}},
	} {
		t.Run(c.name, func(t *testing.T) {
			st, _ := newFakeRouteSQLStore(t, routeScript{}.on("VERSION", rsRows([]driver.Value{"8.0.11-TiDB-v8.5.0", "TiDB Server"})).on("TXN_MODE", rsRows(c.row)))
			if err := requireCurrentTiDB(context.Background(), st.db); !errors.Is(err, errCurrentTiDB) {
				t.Fatalf("不安全读回必须拒绝: %v", err)
			}
		})
	}
}
