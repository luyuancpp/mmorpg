package main

// merged_into.go 的 -route-source=db 用例(W6 / 5'a / R8,规格 §6.3)。替身见 merge_run_db_test.go。

import (
	"context"
	"errors"
	"fmt"
	"reflect"
	"strings"
	"testing"

	"github.com/redis/go-redis/v9"
)

// TestMarkMergedIntoDB_CommitThenMirror:库事务(带本次围栏 run_id 与清单 run_id)→ 提交 → 镜像写库返回的那一个值。
func TestMarkMergedIntoDB_CommitThenMirror(t *testing.T) {
	rb, db, cache := newTestRouteBackend()
	want, _ := encodeRouteMergedIntoValue(testDBDst, testDBMarkRun)
	db.markMerged = func(src, dst uint32, fenceRunID, markRunID string) (string, error) {
		if src != testDBSrc || dst != testDBDst || fenceRunID != testDBRun || markRunID != testDBMarkRun {
			return "", fmt.Errorf("MarkMergedInto(%d, %d, %q, %q)", src, dst, fenceRunID, markRunID)
		}
		return want, nil
	}
	var mirrored string
	cache.setMerged = func(src uint32, value string) error {
		mirrored = fmt.Sprintf("%d=%s", src, value)
		return nil
	}
	if err := markMergedIntoDB(context.Background(), rb, testDBSrc, testDBDst, testDBRun, testDBMarkRun, false); err != nil {
		t.Fatalf("markMergedIntoDB: %v", err)
	}
	if mirrored != fmt.Sprintf("%d=%s", testDBSrc, want) {
		t.Fatalf("mirror = %q, want %d=%s", mirrored, testDBSrc, want)
	}
}

// TestMarkMergedIntoDB_FenceLostWritesNoMirror:库事务按 fencing token 拒绝(errRouteFenceLost)→ 镜像不碰,错误原样上抛。
func TestMarkMergedIntoDB_FenceLostWritesNoMirror(t *testing.T) {
	rb, db, cache := newTestRouteBackend()
	db.markMerged = func(uint32, uint32, string, string) (string, error) {
		return "", fmt.Errorf("%w: zone 901 holds run_id=\"other\"", errRouteFenceLost)
	}
	if err := markMergedIntoDB(context.Background(), rb, testDBSrc, testDBDst, testDBRun, testDBMarkRun, false); !errors.Is(err, errRouteFenceLost) {
		t.Fatalf("error = %v, want errRouteFenceLost", err)
	}
	if len(cache.called()) != 0 {
		t.Fatalf("cache calls = %v: a refused marker must not be mirrored", cache.called())
	}
}

// TestMarkMergedIntoDB_DryRunRefusesAnotherTarget:dry-run 只读库;源区已合进别的 zone 即拒绝。
func TestMarkMergedIntoDB_DryRunRefusesAnotherTarget(t *testing.T) {
	rb, db, cache := newTestRouteBackend()
	db.readMerged = func(uint32) (uint32, string, bool, error) { return 903, "run-x", true, nil }
	err := markMergedIntoDB(context.Background(), rb, testDBSrc, testDBDst, testDBRun, testDBMarkRun, true)
	if err == nil || !strings.Contains(err.Error(), "merged into zone 903") {
		t.Fatalf("error = %v", err)
	}
	if got := db.called(); !reflect.DeepEqual(got, []string{"ReadMergedInto"}) || len(cache.called()) != 0 {
		t.Fatalf("dry-run calls db=%v cache=%v", got, cache.called())
	}
}

// TestClearMergedIntoDB_DBThenMirrorByFullValue:先删库(按 src、dst、清单 run_id,带本次撤销的围栏 run_id),再按完整值删镜像;
// 库里没删到也照样按本次的值删镜像(上一次删了库、没删镜像的补上);删镜像失败 = 错误(撤销保留围栏)。
func TestClearMergedIntoDB_DBThenMirrorByFullValue(t *testing.T) {
	value, _ := encodeRouteMergedIntoValue(testDBDst, testDBMarkRun)
	for _, deletedInDB := range []bool{true, false} {
		rb, db, cache := newTestRouteBackend()
		db.clearMerged = func(src, dst uint32, fenceRunID, markRunID string) (bool, error) {
			if src != testDBSrc || dst != testDBDst || fenceRunID != testDBRun || markRunID != testDBMarkRun {
				return false, fmt.Errorf("ClearMergedInto(%d, %d, %q, %q)", src, dst, fenceRunID, markRunID)
			}
			return deletedInDB, nil
		}
		var cad string
		cache.delMerged = func(src uint32, v string) (bool, error) {
			cad = fmt.Sprintf("%d=%s", src, v)
			return true, nil
		}
		got, err := clearMergedIntoDB(context.Background(), rb, testDBSrc, testDBDst, testDBRun, testDBMarkRun, false)
		if err != nil || got != deletedInDB {
			t.Fatalf("deletedInDB=%v: clear = (%v, %v)", deletedInDB, got, err)
		}
		if cad != fmt.Sprintf("%d=%s", testDBSrc, value) {
			t.Fatalf("deletedInDB=%v: mirror CAD = %q, want the full value %s", deletedInDB, cad, value)
		}
	}
	rb, db, cache := newTestRouteBackend()
	db.clearMerged = func(uint32, uint32, string, string) (bool, error) { return true, nil }
	cache.delMerged = func(uint32, string) (bool, error) { return false, errors.New("i/o timeout") }
	if _, err := clearMergedIntoDB(context.Background(), rb, testDBSrc, testDBDst, testDBRun, testDBMarkRun, false); err == nil {
		t.Fatal("a failed mirror compare-and-delete must be reported (the unmerge keeps its fence)")
	}
}

// TestCheckMergedIntoBeforeMergeDB:读库,判定与 Redis 版同一个纯函数(源区合进别处 / 目标区已被合走 / 读失败 → 拒绝)。
func TestCheckMergedIntoBeforeMergeDB(t *testing.T) {
	type row struct {
		dst   uint32
		found bool
		err   error
	}
	for _, tc := range []struct {
		name     string
		src, dst row
		want     string
	}{
		{"none", row{}, row{}, ""},
		{"same-target-resume", row{dst: testDBDst, found: true}, row{}, ""},
		{"src-merged-elsewhere", row{dst: 903, found: true}, row{}, "already merged into zone 903"},
		{"dst-merged-away", row{}, row{dst: 903, found: true}, "was itself merged into zone 903"},
		{"src-read-error", row{err: errors.New("1205")}, row{}, "cannot prove zone 901"},
	} {
		t.Run(tc.name, func(t *testing.T) {
			_, db, _ := newTestRouteBackend()
			db.readMerged = func(z uint32) (uint32, string, bool, error) {
				r := tc.src
				if z == testDBDst {
					r = tc.dst
				}
				return r.dst, "run-x", r.found, r.err
			}
			err := checkMergedIntoBeforeMergeDB(context.Background(), db, testDBSrc, testDBDst)
			if tc.want == "" {
				if err != nil {
					t.Fatalf("check: %v", err)
				}
				return
			}
			if err == nil || !strings.Contains(err.Error(), tc.want) {
				t.Fatalf("error = %v, want %q", err, tc.want)
			}
		})
	}
}

// TestMergedIntoSteps_RedisSourceUnchanged:rb == nil 时 W6 / 5'a / 前置核对发出的 Redis 命令与改动前逐条相同。
func TestMergedIntoSteps_RedisSourceUnchanged(t *testing.T) {
	ctx := context.Background()
	marker, _ := encodeRouteMergedIntoValue(testDBDst, testDBMarkRun)
	reply := func(_ int, cmd redis.Cmder) (any, error) {
		switch cmd.Name() {
		case "get":
			return marker, nil
		case "set":
			return "OK", nil
		case "del":
			return int64(1), nil
		}
		return nil, fmt.Errorf("unexpected %v", cmd.Args())
	}
	assertSameRedisTraffic(t, reply,
		func(rdb *redis.Client) error {
			return markMergedInto(ctx, rdb, testDBSrc, testDBDst, testDBMarkRun, false)
		},
		func(rdb *redis.Client) error {
			return stepMarkMergedInto(ctx, nil, rdb, testDBSrc, testDBDst, testDBRun, testDBMarkRun, false)
		})
	assertSameRedisTraffic(t, reply,
		func(rdb *redis.Client) error {
			_, err := clearMergedInto(ctx, rdb, testDBSrc, testDBDst, testDBMarkRun, false)
			return err
		},
		func(rdb *redis.Client) error {
			_, err := stepClearMergedInto(ctx, nil, rdb, testDBSrc, testDBDst, testDBRun, testDBMarkRun, false)
			return err
		})
	assertSameRedisTraffic(t, reply,
		func(rdb *redis.Client) error { return checkMergedIntoBeforeMerge(ctx, rdb, testDBSrc, testDBDst) },
		func(rdb *redis.Client) error {
			return stepCheckMergedIntoBeforeMerge(ctx, nil, rdb, testDBSrc, testDBDst)
		})
}
