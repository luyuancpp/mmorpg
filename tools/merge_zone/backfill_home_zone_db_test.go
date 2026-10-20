package main

// backfill_home_zone.go 的 -route-source=db 用例(W9,规格 §6.3)。替身见 merge_run_db_test.go。

import (
	"context"
	"errors"
	"fmt"
	"reflect"
	"strings"
	"testing"

	"github.com/redis/go-redis/v9"
)

// backfillFake 模拟 BackfillHomeBatch:home 表里没有的插入,0 的补上,== zone 的 already,其余 conflict。
func backfillFake(home map[uint64]uint32) func(uint32, []uint64) (routeBatchResult[routeBackfillVerdict], error) {
	return func(zone uint32, ids []uint64) (routeBatchResult[routeBackfillVerdict], error) {
		res := newRouteBatchResult[routeBackfillVerdict](len(ids))
		for _, id := range ids {
			h, ok := home[id]
			switch {
			case !ok:
				res.Verdicts[id] = routeBackfillInserted
			case h == 0:
				res.Verdicts[id] = routeBackfillFilled
			case h == zone:
				res.Verdicts[id] = routeBackfillAlready
				res.Target = append(res.Target, mergeRouteRow{PlayerID: id, HomeZone: zone, Rev: 2})
				continue
			default:
				res.Verdicts[id] = routeBackfillConflict
				continue
			}
			home[id] = zone
			res.Changed++
			res.Target = append(res.Target, mergeRouteRow{PlayerID: id, HomeZone: zone, Rev: 1})
		}
		return res, nil
	}
}

func backfillBackend(home map[uint64]uint32) (*routeBackend, *fakeRouteDB, *fakeRouteCache) {
	rb, db, cache := newTestRouteBackend()
	db.readFence = func(uint32) (routeFenceState, error) { return routeFenceState{Present: true}, nil }
	db.readMerged = func(uint32) (uint32, string, bool, error) { return 0, "", false, nil }
	db.backfill = backfillFake(home)
	return rb, db, cache
}

// TestBackfillHomeZoneDB_CountsAndPublishesEveryChunk:zone 库每批 1000 人切成 500 人一个库事务;每个事务之后发布本批目标状态的全部行;
// 计数与 Redis 版同口径(插入 / 补 home 计 created,已是本 zone 计 already,别的 zone 计 mismatched 不动)。
func TestBackfillHomeZoneDB_CountsAndPublishesEveryChunk(t *testing.T) {
	ids := testIDs(routeWriteBatchLimit + 4)
	home := map[uint64]uint32{1: 0, 2: testDBSrc, 3: 777}
	rb, db, cache := backfillBackend(home)
	rep, err := backfillHomeZoneDB(context.Background(), rb, sliceSource(ids), testDBSrc, false)
	if err != nil {
		t.Fatalf("backfill: %v", err)
	}
	if rep.RowsScanned != len(ids) || rep.Created != len(ids)-2 || rep.AlreadyMapped != 1 || rep.Mismatched != 1 || home[3] != 777 {
		t.Fatalf("report = %s home[3]=%d", rep, home[3])
	}
	if db.count("BackfillHomeBatch") != 2 || len(cache.publishedIDs()) != 2 {
		t.Fatalf("db=%v cache=%v", db.called(), cache.called())
	}
}

// TestBackfillHomeZoneDB_RefusedByTheMergedIntoRowInTheDB:库里记着该 zone 已被合走(Redis 镜像丢了也一样)→ 一个批事务都不开。
func TestBackfillHomeZoneDB_RefusedByTheMergedIntoRowInTheDB(t *testing.T) {
	rb, db, cache := backfillBackend(map[uint64]uint32{})
	db.readMerged = func(uint32) (uint32, string, bool, error) { return testDBDst, testDBMarkRun, true, nil }
	_, err := backfillHomeZoneDB(context.Background(), rb, sliceSource(testIDs(3)), testDBSrc, false)
	if !errors.Is(err, errRouteZoneMergedAway) {
		t.Fatalf("error = %v, want errRouteZoneMergedAway", err)
	}
	if db.count("BackfillHomeBatch") != 0 || len(cache.called()) != 0 {
		t.Fatalf("db=%v cache=%v: nothing may be written into a merged-away zone", db.called(), cache.called())
	}
}

// TestBackfillHomeZoneDB_RefusedWhileFenced:该 zone 的围栏生效(合服进行中)→ 拒绝;读不到围栏 = 证明不了,同样拒绝。
func TestBackfillHomeZoneDB_RefusedWhileFenced(t *testing.T) {
	rb, db, _ := backfillBackend(map[uint64]uint32{})
	db.readFence = func(uint32) (routeFenceState, error) {
		return routeFenceState{Present: true, Active: true, RunID: "r"}, nil
	}
	if _, err := backfillHomeZoneDB(context.Background(), rb, sliceSource(testIDs(1)), testDBSrc, false); err == nil ||
		!strings.Contains(err.Error(), "is being merged") {
		t.Fatalf("fenced: error = %v", err)
	}
	db.readFence = func(uint32) (routeFenceState, error) { return routeFenceState{}, errors.New("1205") }
	if _, err := backfillHomeZoneDB(context.Background(), rb, sliceSource(testIDs(1)), testDBSrc, false); err == nil ||
		!strings.Contains(err.Error(), "cannot prove") {
		t.Fatalf("unreadable fence: error = %v", err)
	}
	if db.count("BackfillHomeBatch") != 0 {
		t.Fatalf("db calls = %v", db.called())
	}
}

// TestBackfillHomeZoneDB_FencedMidRunStops:批事务在库里复核到围栏生效(errRouteZoneFenced)→ 停下,后面的批不跑。
func TestBackfillHomeZoneDB_FencedMidRunStops(t *testing.T) {
	ids := testIDs(routeWriteBatchLimit + 1)
	rb, db, _ := backfillBackend(map[uint64]uint32{})
	inner := db.backfill
	db.backfill = func(zone uint32, chunk []uint64) (routeBatchResult[routeBackfillVerdict], error) {
		if db.count("BackfillHomeBatch") > 1 {
			return routeBatchResult[routeBackfillVerdict]{}, fmt.Errorf("%w: zone 901 (run_id=\"m\")", errRouteZoneFenced)
		}
		return inner(zone, chunk)
	}
	rep, err := backfillHomeZoneDB(context.Background(), rb, sliceSource(ids), testDBSrc, false)
	if !errors.Is(err, errRouteZoneFenced) || rep.Created != routeWriteBatchLimit {
		t.Fatalf("backfill = (%s, %v)", rep, err)
	}
}

// TestBackfillHomeZoneDB_PublishFailureStopsAndRerunRepublishes:提交后发布失败 → 停下、不跑下一批;重跑时已写的走 already、照样发布。
func TestBackfillHomeZoneDB_PublishFailureStopsAndRerunRepublishes(t *testing.T) {
	ids := testIDs(routeWriteBatchLimit + 1)
	home := map[uint64]uint32{}
	rb, db, cache := backfillBackend(home)
	cache.publishErr = func(call int, _ []mergeRouteRow) error {
		if call == 0 {
			return fmt.Errorf("%w: WAITAOF acked 0", errRoutePublishNotDurable)
		}
		return nil
	}
	if _, err := backfillHomeZoneDB(context.Background(), rb, sliceSource(ids), testDBSrc, false); !errors.Is(err, errRoutePublishNotDurable) {
		t.Fatalf("first run: error = %v", err)
	}
	if db.count("BackfillHomeBatch") != 1 {
		t.Fatalf("db calls = %v", db.called())
	}
	cache.publishErr = nil
	rep, err := backfillHomeZoneDB(context.Background(), rb, sliceSource(ids), testDBSrc, false)
	if err != nil || rep.AlreadyMapped != routeWriteBatchLimit || rep.Created != 1 {
		t.Fatalf("rerun = (%s, %v)", rep, err)
	}
	pub := cache.publishedIDs()
	if len(pub) != 3 || !reflect.DeepEqual(pub[1], pub[0]) {
		t.Fatalf("rerun must republish the first batch: published batch sizes %d", len(pub))
	}
}

// TestBackfillHomeZone_RedisSourceUnchanged:rb == nil 时与今天的 backfillHomeZone 发出同样的 Redis 命令(dry-run:标记 GET +
// 每批一条 pipeline GET)。
func TestBackfillHomeZone_RedisSourceUnchanged(t *testing.T) {
	ctx := context.Background()
	ids := testIDs(3)
	reply := func(_ int, cmd redis.Cmder) (any, error) {
		if cmd.Name() == "get" {
			return nil, redis.Nil
		}
		return nil, fmt.Errorf("unexpected %v", cmd.Args())
	}
	assertSameRedisTraffic(t, reply,
		func(rdb *redis.Client) error {
			_, err := backfillHomeZone(ctx, rdb, sliceSource(ids), testDBSrc, true)
			return err
		},
		func(rdb *redis.Client) error {
			_, err := stepBackfillHomeZone(ctx, nil, rdb, sliceSource(ids), testDBSrc, true)
			return err
		})
}
