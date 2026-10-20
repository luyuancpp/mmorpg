//go:build integration

package main

// 真库用例:sqlRouteTable 的三条语句在真 MySQL / TiDB 上的语义(跳取 home_zone、计数不含 NULL 归属、主键点查)。
//
// 运行(Codex):
//
//	$env:DATA_CONSISTENCY_IT_ROUTE_DB_DSN = "<账号>@tcp(127.0.0.1:3306)/<一次性库>"   # 口令不写进任何文件
//	$env:DATA_CONSISTENCY_IT_REQUIRE = "1"                                            # 缺 DSN / 连不上判 FAIL 而不是 SKIP
//	go test -tags integration -run TestRouteDBIntegration -count=1 -v .
//
// 库必须已由 data_service -migrate 建好 player_route(本工具不复制 DDL,规格 §13.3 第 7 条同口径)。
// 用例只插、删自己的行(随机的高位 player_id、zone 999101 / 999102),不清别人的数据。

import (
	"context"
	"database/sql"
	"math/rand/v2"
	"os"
	"testing"
	"time"
)

func openRouteIT(t *testing.T) *sql.DB {
	t.Helper()
	dsn := os.Getenv("DATA_CONSISTENCY_IT_ROUTE_DB_DSN")
	require := os.Getenv("DATA_CONSISTENCY_IT_REQUIRE") == "1"
	if dsn == "" {
		if require {
			t.Fatal("DATA_CONSISTENCY_IT_REQUIRE=1 but DATA_CONSISTENCY_IT_ROUTE_DB_DSN is empty")
		}
		t.Skip("set DATA_CONSISTENCY_IT_ROUTE_DB_DSN to a database migrated by data_service -migrate")
	}
	db, err := sql.Open("mysql", dsn)
	if err != nil {
		t.Fatalf("open route db: %v", err)
	}
	t.Cleanup(func() { _ = db.Close() })
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := db.PingContext(ctx); err != nil {
		if require {
			t.Fatalf("route db unreachable: %v", err)
		}
		t.Skipf("route db unreachable: %v", err)
	}
	var n int
	if err := db.QueryRowContext(ctx, "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = 'player_route'").Scan(&n); err != nil || n != 1 {
		t.Fatalf("player_route not found in the DSN database (run data_service -migrate first): n=%d err=%v", n, err)
	}
	return db
}

func TestRouteDBIntegration(t *testing.T) {
	db := openRouteIT(t)
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	const zoneA, zoneB = uint32(999101), uint32(999102)
	base := uint64(1)<<50 + rand.Uint64N(1<<40)
	type row struct {
		id   uint64
		home *uint32
	}
	a, b := zoneA, zoneB
	rows := []row{{base + 1, &a}, {base + 2, &a}, {base + 3, &a}, {base + 4, &b}, {base + 5, nil}}

	src := dbRouteSource{t: sqlRouteTable{db: db}, dbName: "it"}
	before, err := src.CountPlayers(ctx)
	if err != nil {
		t.Fatalf("CountPlayers before insert: %v", err)
	}

	t.Cleanup(func() {
		cctx, ccancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer ccancel()
		for _, r := range rows {
			if _, err := db.ExecContext(cctx, "DELETE FROM player_route WHERE route_shard = ? AND player_id = ?", r.id%routeRowShards, r.id); err != nil {
				t.Errorf("cleanup player_id=%d: %v", r.id, err)
			}
		}
	})
	for _, r := range rows {
		var home any
		if r.home != nil {
			home = *r.home
		}
		if _, err := db.ExecContext(ctx,
			"INSERT INTO player_route (route_shard, player_id, home_zone, rev, updated_by, created_at_ms, updated_at_ms) VALUES (?, ?, ?, 1, 'dcc-it', 1, 1)",
			r.id%routeRowShards, r.id, home); err != nil {
			t.Fatalf("insert player_id=%d: %v", r.id, err)
		}
	}

	zones, rep, err := src.LiveZones(ctx)
	if err != nil {
		t.Fatalf("LiveZones: %v", err)
	}
	for _, z := range []uint32{zoneA, zoneB} {
		if _, ok := zones[z]; !ok {
			t.Errorf("LiveZones missing %d (report %+v)", z, rep)
		}
	}
	after, err := src.CountPlayers(ctx)
	if err != nil {
		t.Fatalf("CountPlayers after insert: %v", err)
	}
	if after-before != 4 {
		t.Errorf("CountPlayers grew by %d, want 4 (NULL home must not count)", after-before)
	}

	tbl := sqlRouteTable{db: db}
	next, ok, err := tbl.NextHomeZone(ctx, uint64(zoneA)+1)
	if err != nil || !ok || next <= zoneA {
		t.Errorf("NextHomeZone(%d) = %d, %v, %v; want a zone > %d", zoneA, next, ok, err, zoneA)
	}

	for id, want := range map[uint64]bool{base + 1: true, base + 4: true, base + 5: false, base + 6: false} {
		has, err := src.HasHome(ctx, id)
		if err != nil || has != want {
			t.Errorf("HasHome(%d) = %v, %v; want %v", id, has, err, want)
		}
	}

	// 只记录执行计划,不断言(小表上优化器可能选主键或全表);回报里看 key 列是不是 idx_route_home。
	if plan, err := db.QueryContext(ctx, "EXPLAIN "+sqlCountWithHome); err == nil {
		cols, _ := plan.Columns()
		for plan.Next() {
			vals := make([]sql.RawBytes, len(cols))
			ptrs := make([]any, len(cols))
			for i := range vals {
				ptrs[i] = &vals[i]
			}
			if err := plan.Scan(ptrs...); err == nil {
				for i, c := range cols {
					t.Logf("EXPLAIN %s = %s", c, vals[i])
				}
			}
		}
		_ = plan.Close()
	} else {
		t.Logf("EXPLAIN failed (informational only): %v", err)
	}
}
