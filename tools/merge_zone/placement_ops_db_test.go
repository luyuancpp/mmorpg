package main

// placement_ops.go 的 -route-source=db 用例(W7 合服 pin 步骤、R8 落点扫描,规格 §6.3、§13.3 第 2 条)。替身见 merge_run_db_test.go。

import (
	"context"
	"errors"
	"fmt"
	"reflect"
	"strconv"
	"strings"
	"testing"

	"github.com/redis/go-redis/v9"
)

// pinFake 模拟 PinPlacementBatch:records 里有值的保持(值 == "{src}:1" 计 already_pinned,否则 kept);没有记录且 home == src 的钉;
// 其余 home_moved。Target = 钉的 + 早已钉的 + 保留的(与 route_store.go 同口径)。
func pinFake(records map[uint64]string, home map[uint64]uint32, src uint32) func(uint32, uint32, string, []uint64) (routeBatchResult[pinVerdict], error) {
	return func(_, _ uint32, _ string, ids []uint64) (routeBatchResult[pinVerdict], error) {
		res := newRouteBatchResult[pinVerdict](len(ids))
		for _, id := range ids {
			homeRaw := ""
			if h, ok := home[id]; ok {
				homeRaw = strconv.FormatUint(uint64(h), 10)
			}
			v := decidePin(records[id], homeRaw, src)
			res.Verdicts[id] = v
			row := mergeRouteRow{PlayerID: id, HomeZone: home[id], Rev: 3}
			switch v {
			case pinVerdictPin:
				records[id] = stableValue(src, 1)
				res.Changed++
				fallthrough
			case pinVerdictAlreadyPinned, pinVerdictKept:
				rec, err := parsePlacement(records[id])
				if err != nil {
					return res, err
				}
				res.Target = append(res.Target, row.withPlacement(rec))
			}
		}
		return res, nil
	}
}

func pinBackend(n int) (*routeBackend, *fakeRouteDB, *fakeRouteCache, []uint64, map[uint64]string) {
	rb, db, cache := newTestRouteBackend()
	ids := testIDs(n)
	records := map[uint64]string{}
	db.pin = pinFake(records, homesAt(testDBSrc, ids), testDBSrc)
	return rb, db, cache, ids, records
}

// TestPinManifestPlacementsDB_BatchesPublishedInOrder:每批「事务 → 本批目标状态的全部行发布」之后才下一批。
func TestPinManifestPlacementsDB_BatchesPublishedInOrder(t *testing.T) {
	rb, db, cache, ids, _ := pinBackend(routeWriteBatchLimit + 3)
	rep, err := pinManifestPlacementsDB(context.Background(), rb, ids, nil, testDBSrc, testDBDst, testDBRun, nil, false)
	if err != nil || rep.Pinned != len(ids) || rep.Candidates != len(ids) {
		t.Fatalf("pin = (%s, %v)", rep, err)
	}
	if db.count("PinPlacementBatch") != 2 || !reflect.DeepEqual(cache.called(), []string{"publishRoutes", "publishRoutes"}) {
		t.Fatalf("db=%v cache=%v", db.called(), cache.called())
	}
}

// TestPinManifestPlacementsDB_PublishFailureStopsBeforeNextBatch:第一批已提交、发布失败 → 不跑第二批,步骤未完成。
func TestPinManifestPlacementsDB_PublishFailureStopsBeforeNextBatch(t *testing.T) {
	rb, db, cache, ids, _ := pinBackend(routeWriteBatchLimit + 3)
	cache.publishErr = func(call int, _ []mergeRouteRow) error {
		return fmt.Errorf("%w: WAITAOF acked 0", errRoutePublishNotDurable)
	}
	_, err := pinManifestPlacementsDB(context.Background(), rb, ids, nil, testDBSrc, testDBDst, testDBRun, nil, false)
	if !errors.Is(err, errRoutePublishNotDurable) || !strings.Contains(err.Error(), "NOT done") {
		t.Fatalf("error = %v", err)
	}
	if db.count("PinPlacementBatch") != 1 {
		t.Fatalf("db calls = %v: the next batch must wait for the previous batch's publish", db.called())
	}
}

// TestPinManifestPlacementsDB_RerunRepublishesPinned:上一次钉了却没发布成 → 重跑全员 already_pinned、照样发布。
func TestPinManifestPlacementsDB_RerunRepublishesPinned(t *testing.T) {
	rb, _, cache, ids, records := pinBackend(5)
	for _, id := range ids {
		records[id] = stableValue(testDBSrc, 1)
	}
	rep, err := pinManifestPlacementsDB(context.Background(), rb, ids, nil, testDBSrc, testDBDst, testDBRun, nil, false)
	if err != nil || rep.AlreadyPinned != len(ids) || rep.Pinned != 0 {
		t.Fatalf("rerun = (%s, %v)", rep, err)
	}
	if got := cache.publishedIDs(); len(got) != 1 || !reflect.DeepEqual(got[0], ids) {
		t.Fatalf("republished = %v, want %v", got, ids)
	}
}

// TestPinManifestPlacementsDB_FenceLostBeforeFirstBatch:闸门失权 → 一个批事务都不开。
func TestPinManifestPlacementsDB_FenceLostBeforeFirstBatch(t *testing.T) {
	rb, db, cache, ids, _ := pinBackend(3)
	gate, _ := countingGate(1)
	if _, err := pinManifestPlacementsDB(context.Background(), rb, ids, nil, testDBSrc, testDBDst, testDBRun, gate, false); !errors.Is(err, errRouteFenceLost) {
		t.Fatalf("error = %v, want errRouteFenceLost", err)
	}
	if len(db.called())+len(cache.called()) != 0 {
		t.Fatalf("db=%v cache=%v: nothing may run after the fence is lost", db.called(), cache.called())
	}
}

// TestPinManifestPlacementsDB_ExistingRecordMustMatchTheManifest:已有记录 == 清单记下的原值计 kept;不等 / home 已不是 src 计异常,
// 本批照样发布(提交了的就是真源),整步报错。
func TestPinManifestPlacementsDB_ExistingRecordMustMatchTheManifest(t *testing.T) {
	rb, db, cache := newTestRouteBackend()
	records := map[uint64]string{1: "1000901:3", 2: "1000901:4"}
	home := map[uint64]uint32{1: testDBSrc, 2: testDBSrc, 3: 777}
	db.pin = pinFake(records, home, testDBSrc)
	rep, err := pinManifestPlacementsDB(context.Background(), rb, []uint64{1, 2, 3}, map[uint64]string{1: "1000901:3", 2: "1000901:2"},
		testDBSrc, testDBDst, testDBRun, nil, false)
	if err == nil || rep.Kept != 1 || rep.AnomalyCount != 2 {
		t.Fatalf("pin = (%s, %v), want kept=1 anomalies=2 and an error", rep, err)
	}
	if got := cache.publishedIDs(); len(got) != 1 || !reflect.DeepEqual(got[0], []uint64{1, 2}) {
		t.Fatalf("published = %v, want the committed target rows [1 2]", got)
	}
}

// TestPinManifestPlacements_RedisSourceUnchanged:rb == nil 时与今天的 pinManifestPlacements 发出同样的 Redis 命令。
func TestPinManifestPlacements_RedisSourceUnchanged(t *testing.T) {
	ctx := context.Background()
	ids := testIDs(3)
	reply := func(_ int, cmd redis.Cmder) (any, error) {
		switch cmd.Name() {
		case "script":
			return "0123456789abcdef0123456789abcdef01234567", nil
		case "evalsha":
			return []any{int64(placementCASWritten), "", "901"}, nil
		}
		return nil, fmt.Errorf("unexpected %v", cmd.Args())
	}
	assertSameRedisTraffic(t, reply,
		func(rdb *redis.Client) error {
			_, err := pinManifestPlacements(ctx, rdb, ids, nil, testDBSrc, false)
			return err
		},
		func(rdb *redis.Client) error {
			_, err := stepPinManifestPlacements(ctx, nil, rdb, ids, nil, testDBSrc, testDBDst, testDBRun, nil, false)
			return err
		})
}

// TestReadPlacementsDB_MatchesTheRedisShape:库行编码成与 Redis 读同形状(值串逐字节相同),没有行 = 两键都不存在。
func TestReadPlacementsDB_MatchesTheRedisShape(t *testing.T) {
	_, db, _ := newTestRouteBackend()
	db.readRoutes = func([]uint64) (map[uint64]mergeRouteRow, error) {
		return map[uint64]mergeRouteRow{
			1: {PlayerID: 1, HomeZone: testDBSrc, StorageID: 1000901, PlacementVersion: 2, FrozenRunID: "run-r"},
			2: {PlayerID: 2, HomeZone: testDBSrc},
		}, nil
	}
	reads, err := readPlacementsDB(context.Background(), db, []uint64{1, 2, 3})
	if err != nil {
		t.Fatalf("readPlacementsDB: %v", err)
	}
	want, _ := decodePlacementRead("1000901:2:frozen:run-r", "901")
	if !reflect.DeepEqual(reads[0], want) || reads[1].RecordPresent || reads[1].HomeRaw != "901" || reads[2].RecordPresent || reads[2].HomePresent {
		t.Fatalf("reads = %+v", reads)
	}
}
