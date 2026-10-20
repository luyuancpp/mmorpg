package main

// unmerge.go 的 -route-source=db 用例(W8 5'、R8 半撤销判定,规格 §6.3)。替身见 merge_run_db_test.go。

import (
	"context"
	"errors"
	"fmt"
	"reflect"
	"testing"

	"github.com/redis/go-redis/v9"
)

func restoreBackend(home map[uint64]uint32) (*routeBackend, *fakeRouteDB, *fakeRouteCache) {
	rb, db, cache := newTestRouteBackend()
	db.restore = func(src, dst uint32, runID string, ids []uint64) (routeBatchResult[mappingOutcome], error) {
		if src != testDBSrc || dst != testDBDst || runID != testDBRun {
			return routeBatchResult[mappingOutcome]{}, fmt.Errorf("RestoreHomeBatch(%d, %d, %q)", src, dst, runID)
		}
		return homeMoveFake(home, testDBDst, testDBSrc)(src, dst, runID, ids)
	}
	return rb, db, cache
}

// TestRestoreMappingForIDsDB_CASBatchesPublished:每批 CAS 改回(dst → src)→ 发布本批目标状态的全部行 → 下一批;第三个 zone 不动。
func TestRestoreMappingForIDsDB_CASBatchesPublished(t *testing.T) {
	ids := testIDs(routeWriteBatchLimit + 2)
	home := homesAt(testDBDst, ids)
	home[1] = 777 // 第三个 zone:不是这次合服造成的,撤销不越权
	rb, db, cache := restoreBackend(home)
	n, err := restoreMappingForIDsDB(context.Background(), rb, ids, testDBSrc, testDBDst, testDBRun, nil, false)
	if err != nil || n != len(ids)-1 {
		t.Fatalf("restore = (%d, %v), want %d", n, err, len(ids)-1)
	}
	if home[1] != 777 || db.count("RestoreHomeBatch") != 2 || len(cache.publishedIDs()) != 2 {
		t.Fatalf("home[1]=%d db=%v published=%d", home[1], db.called(), len(cache.publishedIDs()))
	}
}

// TestRestoreMappingForIDsDB_PublishFailureStopsBeforeNextBatch:第一批已提交、发布失败 → 不跑第二批(撤销中止、保留围栏)。
func TestRestoreMappingForIDsDB_PublishFailureStopsBeforeNextBatch(t *testing.T) {
	ids := testIDs(routeWriteBatchLimit + 2)
	rb, db, cache := restoreBackend(homesAt(testDBDst, ids))
	cache.publishErr = func(int, []mergeRouteRow) error { return fmt.Errorf("%w: conflict", errRoutePublishRejected) }
	n, err := restoreMappingForIDsDB(context.Background(), rb, ids, testDBSrc, testDBDst, testDBRun, nil, false)
	if !errors.Is(err, errRoutePublishRejected) || n != routeWriteBatchLimit {
		t.Fatalf("restore = (%d, %v), want the first batch counted and the publish error", n, err)
	}
	if db.count("RestoreHomeBatch") != 1 {
		t.Fatalf("db calls = %v", db.called())
	}
}

// TestRestoreMappingForIDsDB_RerunRepublishesRestored:上一次改回了却没发布成 → 重跑走「已是 src」分支、照样发布。
func TestRestoreMappingForIDsDB_RerunRepublishesRestored(t *testing.T) {
	ids := testIDs(4)
	rb, _, cache := restoreBackend(homesAt(testDBSrc, ids))
	n, err := restoreMappingForIDsDB(context.Background(), rb, ids, testDBSrc, testDBDst, testDBRun, nil, false)
	if err != nil || n != 0 {
		t.Fatalf("rerun = (%d, %v)", n, err)
	}
	if got := cache.publishedIDs(); len(got) != 1 || !reflect.DeepEqual(got[0], ids) {
		t.Fatalf("republished = %v, want %v", got, ids)
	}
}

// TestRestoreMappingForIDsDB_FenceLostStops:闸门在第二批之前失权 → 第二批不跑。
func TestRestoreMappingForIDsDB_FenceLostStops(t *testing.T) {
	ids := testIDs(routeWriteBatchLimit + 2)
	rb, db, _ := restoreBackend(homesAt(testDBDst, ids))
	gate, _ := countingGate(2)
	if _, err := restoreMappingForIDsDB(context.Background(), rb, ids, testDBSrc, testDBDst, testDBRun, gate, false); !errors.Is(err, errRouteFenceLost) {
		t.Fatalf("error = %v, want errRouteFenceLost", err)
	}
	if db.count("RestoreHomeBatch") != 1 {
		t.Fatalf("db calls = %v", db.called())
	}
}

// TestManifestPlayersBackAtDB:清单里有人在库里已是 src = 半撤销;读失败交给调用方(按半撤销处理)。
func TestManifestPlayersBackAtDB(t *testing.T) {
	_, db, _ := newTestRouteBackend()
	db.readRoutes = func([]uint64) (map[uint64]mergeRouteRow, error) {
		return map[uint64]mergeRouteRow{1: {PlayerID: 1, HomeZone: testDBDst}, 2: {PlayerID: 2, HomeZone: testDBSrc}}, nil
	}
	if back, err := manifestPlayersBackAtDB(context.Background(), db, []uint64{1, 2}, testDBSrc); err != nil || !back {
		t.Fatalf("back = (%v, %v), want true", back, err)
	}
	if back, err := manifestPlayersBackAtDB(context.Background(), db, []uint64{1}, testDBSrc); err != nil || back {
		t.Fatalf("back = (%v, %v), want false", back, err)
	}
	db.readRoutes = func([]uint64) (map[uint64]mergeRouteRow, error) { return nil, errors.New("1205") }
	if _, err := manifestPlayersBackAtDB(context.Background(), db, []uint64{1}, testDBSrc); err == nil {
		t.Fatal("a DB error must be returned")
	}
}

// TestUnmergeMappingSteps_RedisSourceUnchanged:rb == nil 时 5'(MGET → pipeline SET)与半撤销判定发出的 Redis 命令与改动前逐条相同。
func TestUnmergeMappingSteps_RedisSourceUnchanged(t *testing.T) {
	ctx := context.Background()
	ids := testIDs(3)
	reply := func(_ int, cmd redis.Cmder) (any, error) {
		switch cmd.Name() {
		case "mget":
			return []any{"902", "901", nil}, nil
		case "set":
			return "OK", nil
		}
		return nil, fmt.Errorf("unexpected %v", cmd.Args())
	}
	assertSameRedisTraffic(t, reply,
		func(rdb *redis.Client) error {
			_, err := restoreMappingForIDs(ctx, rdb, ids, testDBSrc, testDBDst, false)
			return err
		},
		func(rdb *redis.Client) error {
			_, err := stepRestoreMapping(ctx, nil, rdb, ids, testDBSrc, testDBDst, testDBRun, nil, false)
			return err
		})
	assertSameRedisTraffic(t, reply,
		func(rdb *redis.Client) error {
			_, err := manifestPlayersBackAt(ctx, rdb, ids, testDBSrc)
			return err
		},
		func(rdb *redis.Client) error {
			_, err := stepManifestPlayersBackAt(ctx, nil, rdb, ids, testDBSrc)
			return err
		})
}
