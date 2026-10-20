//go:build merge_integration

package main

// -route-source=db 的搬库端到端:真全局库(player_route / zone_merge_fence)+ 真 mapping Redis(发布 + WAITAOF)+ 真 zone 库
// (R3 / R4),状态机进程内驱动(规格 §6.3 W11、§13.3 第 2 条)。
//
//	MERGE_ZONE_IT_ROUTE_DB_DSN='<user>:<pass>@tcp(<host>:<port>)/<一次性库>' \
//	  go test -tags merge_integration -count=1 -v -run "TestIT_RouteRelocate" ./...
//
// 依赖与 route_store_integration_test.go 相同:路由库必须是已由 `data_service -f <yaml> -migrate` 建好表的一次性库,mapping Redis
// (DB 9)要支持 `WAITAOF 1 0`;不满足时 SKIP(MERGE_ZONE_IT_REQUIRE=1 时 FAIL)。本组只写 player_route 里 itRouteRelocPlayers
// 这几名玩家的行与 zone 901 的围栏片(RelocateCAS 按 home 确保 63 号片存在),开始与结束时都删掉。
// 不经真二进制:-route-source=db 的入口接线在 main.go(另一批改动),这里直接装配 sqlRelocatePlacements。

import (
	"context"
	"fmt"
	"strings"
	"testing"
	"time"

	"github.com/redis/go-redis/v9"
)

// itRouteRelocPlayers 落在两个不同的行片上(…000 % 64 = 40,…001 → 41),与 route_store_integration_test.go 的玩家不重叠。
var itRouteRelocPlayers = []uint64{9_000_001_000, 9_000_001_001}

// itRouteRelocCleanup 删掉本组写过的路由行与 zone 901 的围栏片。测试代码直接写表:AST 守卫不扫 _test.go。
func itRouteRelocCleanup(t *testing.T, st *mergeRouteStore) {
	t.Helper()
	ctx := context.Background()
	for _, id := range itRouteRelocPlayers {
		if _, err := st.db.ExecContext(ctx, "DELETE FROM player_route WHERE route_shard = ? AND player_id = ?", routeShardArg(id), id); err != nil {
			t.Fatalf("cleanup player_route %d: %v", id, err)
		}
	}
	if _, err := st.db.ExecContext(ctx, "DELETE FROM zone_merge_fence WHERE zone_id = ?", uint64(itSrcZone)); err != nil {
		t.Fatalf("cleanup zone_merge_fence %d: %v", itSrcZone, err)
	}
}

// itRouteRelocSetup 清空一次性状态,打开路由库与 mapping Redis,给本组玩家在 zone 901 造三张玩家表的行(不写 Redis 映射:
// db 源下映射只在库里),返回装好的库接缝。
func itRouteRelocSetup(t *testing.T) (*mergeRouteStore, *redis.Client, sqlRelocatePlacements) {
	t.Helper()
	itReset(t)
	st := itRouteStore(t)
	itRouteRelocCleanup(t, st)
	t.Cleanup(func() { itRouteRelocCleanup(t, st) })
	rdb := itRouteRedis(t)
	itSeedPlayers(t, itRouteRelocPlayers, false)
	return st, rdb, sqlRelocatePlacements{store: st, pub: newRoutePublisher(rdb, itRouteDurability(t))}
}

// itRouteRelocEngine 用库接缝 + 真 R2 / R3 / R4 接缝(shared Redis 的排序锁、zone_901_db → zone_902_db)装配状态机。
func itRouteRelocEngine(t *testing.T, m *relocateManifest, places relocatePlacements) *relocateEngine {
	t.Helper()
	ctx := context.Background()
	db := itOpenReadCommitted(t)
	src, dst := zoneDBName(itSrcZone), zoneDBName(itDstZone)
	cols := map[string][]string{}
	for _, tbl := range itPlayerTables {
		c, err := alignedPlayerColumns(ctx, db, src, dst, tbl)
		if err != nil {
			t.Fatal(err)
		}
		cols[tbl] = c
	}
	rows, err := newSQLRelocateRows(ctx, db, src, dst, itPlayerTables, cols)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(rows.Close)
	e := newRelocateEngine(m, func(*relocateManifest) error { return nil }, places, redisOrderingLocks{rdb: itRedis(t, itSharedRD)}, rows,
		map[uint32]bool{itSrcZone: true, itDstZone: true}, 100, time.Second)
	e.lockPoll = 50 * time.Millisecond
	return e
}

// itRouteCache 返回一名玩家在 mapping Redis 里的三键("home|placement|maprev",不存在写 "-")。
func itRouteCache(t *testing.T, rdb *redis.Client, id uint64) string {
	t.Helper()
	vals, err := rdb.MGet(context.Background(), playerZoneKey(id), placementKey(id), mapRevKey(id)).Result()
	if err != nil {
		t.Fatal(err)
	}
	parts := make([]string, len(vals))
	for i, v := range vals {
		s, ok := v.(string)
		if !ok {
			s = "-"
		}
		parts[i] = s
	}
	return strings.Join(parts, "|")
}

// itRouteRow 读一名玩家的库行(必须存在)。
func itRouteRow(t *testing.T, st *mergeRouteStore, id uint64) mergeRouteRow {
	t.Helper()
	rows, err := st.ReadRoutes(context.Background(), []uint64{id})
	if err != nil {
		t.Fatal(err)
	}
	row, ok := rows[id]
	if !ok {
		t.Fatalf("player %d has no player_route row", id)
	}
	return row
}

// TestIT_RouteRelocate_DBSourceRunsR1ToR5:db 源下 R1–R5 跑通 —— 库里落点三列与 rev、缓存三键都到 "{T}:{v+1}",行拷进 T;
// 之后 verify:route_cache 的比对为 0 处不一致,读库的 storage-audit 认出源库已无人指向、只剩冷副本。
func TestIT_RouteRelocate_DBSourceRunsR1ToR5(t *testing.T) {
	requireIT(t)
	st, rdb, places := itRouteRelocSetup(t)
	ctx := context.Background()
	p0, p1 := itRouteRelocPlayers[0], itRouteRelocPlayers[1]
	itSeedRoutes(t, st, itSrcZone, p0, p1)
	// p1 已有落点 "901:3"(rev 2);p0 没有落点,按 v=1 冻结。
	if _, err := st.db.ExecContext(ctx, "UPDATE player_route SET storage_id = ?, placement_version = 3, rev = 2 WHERE route_shard = ? AND player_id = ?",
		uint64(itSrcZone), routeShardArg(p1), p1); err != nil {
		t.Fatal(err)
	}

	m := newRelocateManifest("reloc-it-db", itSrcZone, itDstZone, 1, itPlayerTables, itRouteRelocPlayers, "it", time.Now())
	if err := itRouteRelocEngine(t, m, places).run(ctx); err != nil {
		t.Fatalf("relocate: %v", err)
	}
	if got := relocStates(m); got != fmt.Sprintf("%d:switched %d:switched", p0, p1) {
		t.Fatalf("states = %s", got)
	}
	for id, want := range map[uint64]struct {
		version, rev uint64
	}{p0: {2, 3}, p1: {4, 4}} {
		row := itRouteRow(t, st, id)
		if row.StorageID != itDstZone || row.PlacementVersion != want.version || row.FrozenRunID != "" || row.Rev != want.rev || row.HomeZone != itSrcZone {
			t.Errorf("player %d DB row = %+v, want %d:%d stable at rev %d, home untouched", id, row, itDstZone, want.version, want.rev)
		}
		if got, wantCache := itRouteCache(t, rdb, id), fmt.Sprintf("901|902:%d|%d", want.version, want.rev); got != wantCache {
			t.Errorf("player %d cache = %s, want %s", id, got, wantCache)
		}
		if _, found := itTransform(t, itOpen(t), zoneDBName(itDstZone), id); !found {
			t.Errorf("player %d row was not copied into %s", id, zoneDBName(itDstZone))
		}
	}
	if bad, sample, err := routeCacheMismatches(ctx, st, rdb, itRouteRelocPlayers); err != nil || bad != 0 {
		t.Fatalf("route cache check after the move = (%d, %v, %v), want 0 mismatches", bad, sample, err)
	}
	rep, err := auditStorageWith(ctx, itOpen(t), routeReadSideDB{db: st}, itSrcZone)
	if err != nil {
		t.Fatalf("storage-audit from the DB: %v", err)
	}
	if rep.Effective() != 0 || rep.ColdCopies != len(itRouteRelocPlayers) || rep.MalformedRecords != 0 {
		t.Fatalf("storage-audit of %d after the move = %s, want nothing placed there and %d cold copies", itSrcZone, rep, len(itRouteRelocPlayers))
	}
}

// TestIT_RouteRelocate_ResumeAfterRedisLossClaimsFromDB:R1 冻结已提交并确认,随后 mapping Redis 被清空、清单也没来得及记下冻结;
// 续跑按库里的 frozen_run_id 认领本次冻结的人,先把冻结补发布,再走 R2–R5。
func TestIT_RouteRelocate_ResumeAfterRedisLossClaimsFromDB(t *testing.T) {
	requireIT(t)
	st, rdb, places := itRouteRelocSetup(t)
	ctx := context.Background()
	itSeedRoutes(t, st, itSrcZone, itRouteRelocPlayers...)
	const run = "reloc-it-db-resume"

	first := newRelocateManifest(run, itSrcZone, itDstZone, 1, itPlayerTables, itRouteRelocPlayers, "it", time.Now())
	if err := itRouteRelocEngine(t, first, places).freeze(ctx, []int{0, 1}); err != nil {
		t.Fatalf("R1: %v", err)
	}
	for _, id := range itRouteRelocPlayers {
		if row := itRouteRow(t, st, id); row.FrozenRunID != run || row.Rev != 2 {
			t.Fatalf("player %d after R1 = %+v, want frozen by %s at rev 2", id, row, run)
		}
		if got := itRouteCache(t, rdb, id); got != "901|"+frozenValue(itSrcZone, 1, run)+"|2" {
			t.Fatalf("player %d cache after R1 = %s", id, got)
		}
	}
	if err := rdb.FlushDB(ctx).Err(); err != nil {
		t.Fatal(err)
	}

	resumed := newRelocateManifest(run, itSrcZone, itDstZone, 1, itPlayerTables, itRouteRelocPlayers, "it", time.Now())
	if err := itRouteRelocEngine(t, resumed, places).run(ctx); err != nil {
		t.Fatalf("resume: %v", err)
	}
	for _, id := range itRouteRelocPlayers {
		if p := relocPlayer(resumed, id); p.State != relocSwitched || p.Version != 1 || p.HomeZone != itSrcZone {
			t.Errorf("player %d = %+v, want the freeze adopted from the DB (v1, home %d) and switched", id, p, itSrcZone)
		}
		if got := itRouteCache(t, rdb, id); got != "901|902:2|3" {
			t.Errorf("player %d cache after the resume = %s, want 901|902:2|3", id, got)
		}
		if _, found := itTransform(t, itOpen(t), zoneDBName(itDstZone), id); !found {
			t.Errorf("player %d row was not copied", id)
		}
	}
}
