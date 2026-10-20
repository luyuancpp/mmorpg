package main

// pin_placement.go 的 -route-source=db 用例(W10,规格 §6.3、§13.3 第 5 条)。替身见 merge_run_db_test.go。

import (
	"context"
	"errors"
	"fmt"
	"reflect"
	"strings"
	"testing"

	"github.com/redis/go-redis/v9"
)

// pinZoneRows 是一个 zone 的 player_route(home = zone),按聚簇键排好;pinZoneFake / scanFake 都从它读。
type pinZoneRows struct {
	zone uint32
	rows []mergeRouteRow // 按 (route_shard, player_id) 升序
}

func newPinZoneRows(zone uint32, ids ...uint64) *pinZoneRows {
	z := &pinZoneRows{zone: zone}
	for _, id := range ids {
		z.rows = append(z.rows, mergeRouteRow{PlayerID: id, HomeZone: zone, Rev: 1})
	}
	for i := 1; i < len(z.rows); i++ {
		for j := i; j > 0 && routeCursorOf(z.rows[j].PlayerID).less(routeCursorOf(z.rows[j-1].PlayerID)); j-- {
			z.rows[j], z.rows[j-1] = z.rows[j-1], z.rows[j]
		}
	}
	return z
}

// pinBatch 模拟 PinZonePlacementBatch:游标之后、还没落点的前 limit 行钉成 "{zone}:1";不足 limit = 翻完。
func (z *pinZoneRows) pinBatch(zone uint32, after routeScanCursor, limit int) (routeBatchResult[pinVerdict], routeScanCursor, bool, error) {
	res := newRouteBatchResult[pinVerdict](limit)
	next := after
	for i := range z.rows {
		r := &z.rows[i]
		if !after.less(routeCursorOf(r.PlayerID)) || r.StorageID != 0 {
			continue
		}
		if len(res.Target) == limit {
			break
		}
		*r = r.withPlacement(placementRecord{StorageID: zone, Version: 1})
		r.Rev++
		res.Verdicts[r.PlayerID] = pinVerdictPin
		res.Changed++
		res.Target = append(res.Target, *r)
		next = routeCursorOf(r.PlayerID)
	}
	return res, next, len(res.Target) < limit, nil
}

func (z *pinZoneRows) scan(_ uint32, after routeScanCursor, limit int) ([]mergeRouteRow, error) {
	var out []mergeRouteRow
	for _, r := range z.rows {
		if after.less(routeCursorOf(r.PlayerID)) && len(out) < limit {
			out = append(out, r)
		}
	}
	return out, nil
}

func pinZoneBackend(z *pinZoneRows) (*routeBackend, *fakeRouteDB, *fakeRouteCache) {
	rb, db, cache := newTestRouteBackend()
	db.readFence = func(uint32) (routeFenceState, error) { return routeFenceState{Present: true}, nil }
	db.readMerged = func(uint32) (uint32, string, bool, error) { return 0, "", false, nil }
	db.pinZone = z.pinBatch
	db.scanByHome = z.scan
	return rb, db, cache
}

// TestPinZonePlacementsDB_PagesAcrossShardsThenCatchesUp:≤ limit 人一批,键集游标跨行片推进;每批发布之后才下一批;
// 收尾对全部已钉 "{zone}:1" 的行做 catch-up。
func TestPinZonePlacementsDB_PagesAcrossShardsThenCatchesUp(t *testing.T) {
	// 行片 0、1、63 各有人:聚簇键序 (0,64) (1,1) (1,65) (63,63) (63,127);每批 2 人,批与批的边界跨行片。
	z := newPinZoneRows(testDBSrc, 63, 64, 1, 65, 127)
	rb, db, cache := pinZoneBackend(z)
	rep, err := pinZonePlacementsDB(context.Background(), rb, testDBSrc, 2, false)
	if err != nil || rep.Pinned != 5 {
		t.Fatalf("pin = (%s, %v), want 5 pinned", rep, err)
	}
	if db.count("PinZonePlacementBatch") != 3 {
		t.Fatalf("db calls = %v, want 3 batches of <= 2", db.called())
	}
	want := [][]uint64{{64, 1}, {65, 63}, {127}}
	if got := cache.publishedIDs(); !reflect.DeepEqual(got, want) {
		t.Fatalf("published batches = %v, want %v (clustered-key order across shards)", got, want)
	}
	if len(cache.caughtUp) != 1 || len(cache.caughtUp[0]) != 5 {
		t.Fatalf("catch-up = %v, want one pass over the 5 pinned rows", cache.caughtUp)
	}
}

// TestPinZonePlacementsDB_PublishFailureStops:批已提交、发布失败 → 不跑下一批、不做收尾,返回错误。
func TestPinZonePlacementsDB_PublishFailureStops(t *testing.T) {
	z := newPinZoneRows(testDBSrc, 1, 2, 3)
	rb, db, cache := pinZoneBackend(z)
	cache.publishErr = func(int, []mergeRouteRow) error { return fmt.Errorf("%w: conflict", errRoutePublishRejected) }
	if _, err := pinZonePlacementsDB(context.Background(), rb, testDBSrc, 2, false); !errors.Is(err, errRoutePublishRejected) {
		t.Fatalf("error = %v", err)
	}
	if db.count("PinZonePlacementBatch") != 1 || len(cache.caughtUp) != 0 {
		t.Fatalf("db=%v catch-up=%d", db.called(), len(cache.caughtUp))
	}
}

// TestPinZonePlacementsDB_RerunRepublishesThroughCatchUp:上一次钉了却没发布成 → 重跑时加锁扫描看不见它们(已有落点),由收尾 catch-up 补发。
func TestPinZonePlacementsDB_RerunRepublishesThroughCatchUp(t *testing.T) {
	z := newPinZoneRows(testDBSrc, 1, 2)
	for i := range z.rows {
		z.rows[i] = z.rows[i].withPlacement(placementRecord{StorageID: testDBSrc, Version: 1})
	}
	rb, _, cache := pinZoneBackend(z)
	rep, err := pinZonePlacementsDB(context.Background(), rb, testDBSrc, 2, false)
	if err != nil || rep.Pinned != 0 {
		t.Fatalf("rerun = (%s, %v)", rep, err)
	}
	if len(cache.caughtUp) != 1 || len(cache.caughtUp[0]) != 2 {
		t.Fatalf("catch-up = %v, want the two pinned rows", cache.caughtUp)
	}
}

// TestPinZonePlacementsDB_FencedMidRunStops:中途有合服立起围栏(批事务回 errRouteZoneFenced)→ 整轮停下。
func TestPinZonePlacementsDB_FencedMidRunStops(t *testing.T) {
	z := newPinZoneRows(testDBSrc, 1, 2, 3)
	rb, db, _ := pinZoneBackend(z)
	db.pinZone = func(zone uint32, after routeScanCursor, limit int) (routeBatchResult[pinVerdict], routeScanCursor, bool, error) {
		if db.count("PinZonePlacementBatch") > 1 {
			return routeBatchResult[pinVerdict]{}, after, false, fmt.Errorf("%w: zone 901 (run_id=\"m\")", errRouteZoneFenced)
		}
		return z.pinBatch(zone, after, limit)
	}
	_, err := pinZonePlacementsDB(context.Background(), rb, testDBSrc, 2, false)
	if !errors.Is(err, errRouteZoneFenced) || !strings.Contains(err.Error(), "entered a merge while pinning") {
		t.Fatalf("error = %v", err)
	}
}

// TestPinZonePlacementsDB_Preconditions:围栏生效 / 库里有合走标记 / 读不到 → 拒绝,一个批事务都不开。
func TestPinZonePlacementsDB_Preconditions(t *testing.T) {
	for _, tc := range []struct {
		name   string
		fence  routeFenceState
		fErr   error
		merged bool
		want   string
	}{
		{"fenced", routeFenceState{Present: true, Active: true, RunID: "m"}, nil, false, "is being merged"},
		{"fence-unreadable", routeFenceState{}, errors.New("1205"), false, "cannot prove"},
		{"merged-away", routeFenceState{}, nil, true, "was merged away"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			rb, db, cache := pinZoneBackend(newPinZoneRows(testDBSrc, 1))
			db.readFence = func(uint32) (routeFenceState, error) { return tc.fence, tc.fErr }
			db.readMerged = func(uint32) (uint32, string, bool, error) { return testDBDst, "r", tc.merged, nil }
			_, err := pinZonePlacementsDB(context.Background(), rb, testDBSrc, 2, false)
			if err == nil || !strings.Contains(err.Error(), tc.want) {
				t.Fatalf("error = %v, want %q", err, tc.want)
			}
			if db.count("PinZonePlacementBatch") != 0 || len(cache.called()) != 0 {
				t.Fatalf("db=%v cache=%v", db.called(), cache.called())
			}
		})
	}
}

// TestPinZonePlacementsDB_DryRunOnlyReads:dry-run 只做一致读分类,不开批事务、不发布。
func TestPinZonePlacementsDB_DryRunOnlyReads(t *testing.T) {
	z := newPinZoneRows(testDBSrc, 1, 2, 3)
	z.rows[0] = z.rows[0].withPlacement(placementRecord{StorageID: testDBSrc, Version: 1})
	z.rows[1] = z.rows[1].withPlacement(placementRecord{StorageID: 1000901, Version: 2})
	rb, db, cache := pinZoneBackend(z)
	rep, err := pinZonePlacementsDB(context.Background(), rb, testDBSrc, 2, true)
	if err != nil || rep.Candidates != 3 || rep.Pinned != 1 || rep.AlreadyPinned != 1 || rep.Kept != 1 {
		t.Fatalf("dry-run = (%s, %v)", rep, err)
	}
	if db.count("PinZonePlacementBatch") != 0 || len(cache.called()) != 0 {
		t.Fatalf("db=%v cache=%v", db.called(), cache.called())
	}
}

// TestPinZonePlacements_RedisSourceUnchanged:rb == nil 时与今天的 pinZonePlacements 发出同样的 Redis 命令(围栏 EXISTS → 标记 GET →
// SCAN …;本替身不应答 SCAN,两边在同一处以同一个错误停下)。
func TestPinZonePlacements_RedisSourceUnchanged(t *testing.T) {
	ctx := context.Background()
	reply := func(_ int, cmd redis.Cmder) (any, error) {
		switch cmd.Name() {
		case "exists":
			return int64(0), nil
		case "get":
			return nil, redis.Nil
		}
		return nil, fmt.Errorf("unexpected %v", cmd.Args())
	}
	assertSameRedisTraffic(t, reply,
		func(rdb *redis.Client) error {
			_, err := pinZonePlacements(ctx, rdb, testDBSrc, false)
			return err
		},
		func(rdb *redis.Client) error {
			_, err := stepPinZonePlacements(ctx, nil, rdb, testDBSrc, false)
			return err
		})
}
