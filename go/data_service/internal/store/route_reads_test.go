package store

import (
	"regexp"
	"strings"
	"testing"

	"shared/placement"
)

// 第 3 批读方语句(route_reads.go 的 RD1–RD4)的形状守卫:只看文本,不连库。
// 执行语义(LEFT JOIN 的 NULL 口径、索引是否命中)由 route_reads_integration_test.go 在真库上证。

// routeReadSQLForTest 是本文件全部读语句,按编号索引。
func routeReadSQLForTest() map[string]string {
	return map[string]string{
		"RD1": routeReadHomeFenceSQL,
		"RD2": routeScanByHomeSQL,
		"RD3": routeScanByStorageSQL,
		"RD4": routeListActiveFencesSQL,
	}
}

// TestRouteReadSQLIsReadOnly:读方语句一律是事务外一致读 —— 不加锁(FOR UPDATE / FOR SHARE / LOCK IN SHARE MODE)、
// 不写任何表。读方加锁会与 W1 的围栏分片锁 / 玩家行锁排队,把登录读拖进建角的锁等待里。
func TestRouteReadSQLIsReadOnly(t *testing.T) {
	write := regexp.MustCompile(`(?i)\b(INSERT|UPDATE|DELETE|REPLACE|TRUNCATE|ALTER|DROP|CREATE)\b`)
	for name, stmt := range routeReadSQLForTest() {
		upper := strings.ToUpper(stmt)
		if !strings.HasPrefix(upper, "SELECT ") {
			t.Fatalf("%s must be a SELECT: %q", name, stmt)
		}
		for _, lock := range []string{"FOR UPDATE", "FOR SHARE", "LOCK IN SHARE MODE"} {
			if strings.Contains(upper, lock) {
				t.Fatalf("%s must not take locks (%s): %q", name, lock, stmt)
			}
		}
		// 列名 updated_by / updated_at_ms 含 "updated",按词边界匹配不会误中;真出现写关键字就是写语句混进了读文件。
		if m := write.FindString(stmt); m != "" {
			t.Fatalf("%s contains the write keyword %q: %q", name, m, stmt)
		}
	}
}

// TestRouteReadHomeFenceSQLShape:RD1 一条语句同时读 home 与围栏(同一快照),按聚簇键点查,围栏读 63 号片(参数化,
// 不写字面量),生效判据用库时钟。
func TestRouteReadHomeFenceSQLShape(t *testing.T) {
	stmt := routeReadHomeFenceSQL
	for _, want := range []string{
		"FROM player_route AS r LEFT JOIN zone_merge_fence AS f ON f.zone_id = r.home_zone AND f.shard = ?",
		"WHERE r.route_shard = ? AND r.player_id = ?",
		"f.run_id IS NOT NULL AND f.expires_at_ms > " + dbNowMsSQL,
	} {
		if !strings.Contains(stmt, want) {
			t.Fatalf("RD1 must contain %q: %q", want, stmt)
		}
	}
	if got := strings.Count(stmt, "?"); got != 3 {
		t.Fatalf("RD1 has %d placeholders, want 3 (fence shard, route shard, player_id): %q", got, stmt)
	}
	if strings.Contains(stmt, "63") {
		t.Fatalf("RD1 must bind the fence shard, not spell it as a literal: %q", stmt)
	}
}

// TestRouteFenceReadShardIsLastShard:读方读的围栏片 = MaxRouteFenceShards − 1(规格 §13.3 第 1 条),
// 且不可能被 FenceShards < 64 的 W1 锁到(W1 锁 player_id % FenceShards < FenceShards ≤ 63)。
func TestRouteFenceReadShardIsLastShard(t *testing.T) {
	if routeFenceReadShard != placement.MaxRouteFenceShards-1 {
		t.Fatalf("routeFenceReadShard = %d, want MaxRouteFenceShards-1 = %d", routeFenceReadShard, placement.MaxRouteFenceShards-1)
	}
	if routeFenceReadShard != 63 {
		t.Fatalf("routeFenceReadShard = %d, want 63", routeFenceReadShard)
	}
	for shards := uint32(1); shards < placement.MaxRouteFenceShards; shards++ {
		got, err := placement.RouteFenceShard(uint64(shards)*1000+uint64(shards)-1, shards)
		if err != nil {
			t.Fatalf("RouteFenceShard(_, %d): %v", shards, err)
		}
		if got >= routeFenceReadShard {
			t.Fatalf("FenceShards=%d lets W1 lock shard %d >= read shard %d", shards, got, routeFenceReadShard)
		}
	}
}

// TestRouteScanByIndexSQLShape:RD2 / RD3 照提案 §11.2 G1a 模板 —— 等值前缀 + `a > x OR (a = x AND b > y)` 键集
// (不是行比较)、按聚簇键排序、带 LIMIT;选出的列与 ScanRoutes 相同(scanRouteRow withKey 的列序)。
func TestRouteScanByIndexSQLShape(t *testing.T) {
	for name, tc := range map[string]struct {
		stmt, column string
	}{
		"RD2": {routeScanByHomeSQL, "home_zone"},
		"RD3": {routeScanByStorageSQL, "storage_id"},
	} {
		for _, want := range []string{
			"SELECT route_shard, player_id, " + routeRowColumnsSQL + " FROM player_route",
			"WHERE " + tc.column + " = ? AND (route_shard > ? OR (route_shard = ? AND player_id > ?))",
			" ORDER BY route_shard, player_id LIMIT ?",
		} {
			if !strings.Contains(tc.stmt, want) {
				t.Fatalf("%s must contain %q: %q", name, want, tc.stmt)
			}
		}
		if strings.Contains(tc.stmt, "(route_shard, player_id) >") {
			t.Fatalf("%s must not use a row comparison (MySQL does not range-optimise it): %q", name, tc.stmt)
		}
		if got := strings.Count(tc.stmt, "?"); got != 5 {
			t.Fatalf("%s has %d placeholders, want 5: %q", name, got, tc.stmt)
		}
	}
}

// TestRouteListActiveFencesSQLShape:RD4 的左表是只有一行的库时钟派生表(没有生效围栏也回一行),
// 生效判据与 RD1 同口径(另把 expires_at_ms 为 NULL 当生效),按 zone 排序,围栏片参数化。
func TestRouteListActiveFencesSQLShape(t *testing.T) {
	stmt := routeListActiveFencesSQL
	for _, want := range []string{
		"FROM (SELECT " + dbNowMsSQL + " AS now_ms) AS n",
		"LEFT JOIN zone_merge_fence AS f ON f.shard = ? AND f.run_id IS NOT NULL",
		"AND (f.expires_at_ms IS NULL OR f.expires_at_ms > n.now_ms)",
		"ORDER BY f.zone_id",
	} {
		if !strings.Contains(stmt, want) {
			t.Fatalf("RD4 must contain %q: %q", want, stmt)
		}
	}
	if got := strings.Count(stmt, "?"); got != 1 {
		t.Fatalf("RD4 has %d placeholders, want 1 (fence shard): %q", got, stmt)
	}
}

// TestRouteReadsFileHasNoTableWrites 把 route_store_test.go 的两条 AST 守卫在本文件上的结论钉一遍:route_reads.go 的
// 字符串字面量里不得出现路由表的写语句(那两条守卫扫的是整个 module,这里只为让失败点直接指向读文件)。
func TestRouteReadsFileHasNoTableWrites(t *testing.T) {
	write := regexp.MustCompile("(?i)(INSERT\\s+INTO|UPDATE|DELETE\\s+FROM|REPLACE\\s+INTO|TRUNCATE\\s+TABLE)\\s+`?(" +
		PlayerRouteTableName + "|" + ZoneMergeFenceTableName + "|" + ZoneMergedIntoTableName + "|" +
		RouteMetaTableName + "|" + RouteReconcileTableName + ")\\b")
	for name, stmt := range routeReadSQLForTest() {
		if write.MatchString(stmt) {
			t.Fatalf("%s writes a route table: %q", name, stmt)
		}
	}
}
