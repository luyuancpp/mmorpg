package routereconcile

import (
	"context"
	"errors"
	"testing"
	"time"

	"data_service/internal/metrics"
	"data_service/internal/store"

	"shared/placement"
)

// 一轮扫描跨越三拍。任务在第一拍扫描之后开始，第一轮完成不能证明任务开始后的完整覆盖。
// 测试只观察 Cache 接口上的游标、预算和 ArmReady 副作用，不直接读取 leader 的扫描状态。
func TestFenceMirrorScanRoundMustStartAfterPrewarm(t *testing.T) {
	h := newHarness(t, true, nil)
	var scanContexts []context.Context
	h.c.fenceScanHook = func(ctx context.Context, cursor uint64, pages int) ([]FenceMirror, uint64, error) {
		if pages <= 0 || pages > 2 {
			t.Fatalf("每拍扫描预算必须在 1..2 页内，实际 %d", pages)
		}
		if _, bounded := ctx.Deadline(); !bounded {
			t.Fatal("围栏扫描必须带独立截止时间")
		}
		scanContexts = append(scanContexts, ctx)
		switch cursor {
		case 0:
			return nil, 11, nil
		case 11:
			return nil, 22, nil
		case 22:
			return nil, 0, nil
		default:
			t.Fatalf("扫描未沿用后继游标: %d", cursor)
			return nil, cursor, errors.New("游标不符")
		}
	}
	for tick := 1; tick <= 5; tick++ {
		h.mustTick(metrics.RouteReconcileTickOK)
		if h.readyPresent() || h.c.arms != 0 {
			t.Fatalf("第 %d 拍尚无任务开始后的完整扫描，不得挂标记", tick)
		}
		if scanContexts[len(scanContexts)-1].Err() == nil {
			t.Fatal("本拍扫描结束后必须释放其截止时间资源")
		}
	}
	h.mustTick(metrics.RouteReconcileTickOK)
	if !h.readyPresent() || h.c.arms != 1 {
		t.Fatal("任务开始之后开始的第二轮扫描完成，应挂一次标记")
	}
	if len(h.c.fenceScans) != 6 || h.c.count("SetWarmToken") != 1 {
		t.Fatal("每拍最多一次有界扫描，等扫描时不得反复预热")
	}
	if h.obs.event(metrics.RouteReconcileEventArmingRefusedUnresolved) != 0 {
		t.Fatal("扫描尚未覆盖只应推迟挂标记，不应计作未决拒绝")
	}
}

func TestFenceMirrorScanFailureRetainsCursorAndDoesNotArm(t *testing.T) {
	h := newHarness(t, true, nil)
	failed := false
	h.c.fenceScanHook = func(_ context.Context, cursor uint64, _ int) ([]FenceMirror, uint64, error) {
		switch cursor {
		case 0:
			return nil, 77, nil
		case 77:
			if !failed {
				failed = true
				return nil, cursor, context.DeadlineExceeded
			}
			return nil, 0, nil
		default:
			t.Fatalf("扫描失败后游标被改写为 %d", cursor)
			return nil, cursor, errors.New("游标不符")
		}
	}
	for tick := 1; tick <= 4; tick++ {
		h.mustTick(metrics.RouteReconcileTickOK)
		if h.readyPresent() || h.c.arms != 0 {
			t.Fatalf("第 %d 拍扫描失败或覆盖不完整，不得挂标记", tick)
		}
	}
	if calls := h.c.fenceScans; len(calls) != 4 || calls[0].cursor != 0 || calls[1].cursor != 77 || calls[2].cursor != 77 || calls[3].cursor != 0 {
		t.Fatalf("失败必须从同一游标重扫，实际调用 %+v", calls)
	}
	h.mustTick(metrics.RouteReconcileTickOK)
	if !h.readyPresent() {
		t.Fatal("失败恢复后完成一整轮新扫描，应最终允许挂标记")
	}
}

func TestFenceMirrorForeignSurvivesEmptyPagesUntilRereadAbsent(t *testing.T) {
	h := newHarness(t, true, nil)
	key := placement.MergeFenceKey(99)
	legacy := `{"run_id":"legacy"}`
	h.c.set(key, legacy)
	first := true
	h.c.fenceScanHook = func(_ context.Context, cursor uint64, _ int) ([]FenceMirror, uint64, error) {
		next := uint64(13)
		if cursor != 0 {
			next = 0
		}
		if first {
			first = false
			return []FenceMirror{{Key: key, Zone: 99, Value: legacy}}, next, nil
		}
		return nil, next, nil
	}
	h.mustTick(metrics.RouteReconcileTickOK)
	for tick := 0; tick < 3; tick++ {
		if tick == 1 {
			// 发现过的 foreign 即使后来变成可解析值，也必须等它实际消失，不能按合法镜像 CAD。
			h.c.set(key, holderJSON("replaced", 7))
		}
		h.mustTick(metrics.RouteReconcileTickOK)
		if h.readyPresent() || h.c.count("SetWarmToken") != 0 || h.c.count("CompareAndDeleteFenceMirror") != 0 {
			t.Fatal("空扫描页或值变合法均不能清除仍存在的 foreign 围栏")
		}
	}
	if h.c.count("GetFenceMirrors") < 3 {
		t.Fatal("已经发现的 foreign 键必须跨拍按键复读")
	}
	h.c.del(key)
	for tick := 0; tick < 6 && !h.readyPresent(); tick++ {
		h.mustTick(metrics.RouteReconcileTickOK)
	}
	if !h.readyPresent() || h.c.arms != 1 {
		t.Fatal("真实复读确认 foreign 已消失且完成新扫描后，应允许挂一次标记")
	}
}

func TestFenceMirrorKnownExpiredUsesSQLRecheckBeforeDelete(t *testing.T) {
	for _, reacquired := range []bool{false, true} {
		t.Run(map[bool]string{false: "保持释放", true: "复读时重新获取"}[reacquired], func(t *testing.T) {
			h := newHarness(t, false, nil)
			key, old := placement.MergeFenceKey(5), holderJSON("old", 3)
			h.st.fenceZones[5] = struct{}{}
			h.c.set(key, old)
			active := []store.ActiveMergeFence{{ZoneID: 5, RunID: "new", FenceEpoch: 4, HolderJSON: holderJSON("new", 4), ExpiresAtMs: 3_000_000}}
			h.st.fenceListHook = func(call int) []store.ActiveMergeFence {
				if call >= 2 && reacquired {
					return active
				}
				return nil
			}
			// 故意让本拍扫描没有命中：名单里已经释放的行仍要按键读，不能等 SCAN 绕完整库。
			h.c.fenceScanHook = func(context.Context, uint64, int) ([]FenceMirror, uint64, error) { return nil, 9, nil }
			h.mustTick(metrics.RouteReconcileTickOK)
			if h.st.fenceLists < 2 {
				t.Fatal("对过期镜像执行 CAD 前必须复读 SQL 生效围栏")
			}
			_, present := h.c.get(key)
			if present != reacquired {
				t.Fatalf("复读重新获取=%t，镜像存在=%t", reacquired, present)
			}
			wantDeletes := 1
			if reacquired {
				wantDeletes = 0
			}
			if h.c.count("CompareAndDeleteFenceMirror") != wantDeletes {
				t.Fatal("SQL 复读生效时不得删除，否则按原完整值删除一次")
			}
		})
	}
}

func TestFenceMirrorScanStateResetsAfterIdentityOrLeaseChange(t *testing.T) {
	for _, change := range []string{"Redis身份", "租约任期"} {
		t.Run(change, func(t *testing.T) {
			h := newHarness(t, false, nil)
			key := placement.MergeFenceKey(99)
			h.c.set(key, `{"run_id":"legacy"}`)
			first := true
			h.c.fenceScanHook = func(_ context.Context, cursor uint64, _ int) ([]FenceMirror, uint64, error) {
				if first {
					first = false
					return []FenceMirror{{Key: key, Zone: 99, Value: `{"run_id":"legacy"}`}}, 93, nil
				}
				return nil, 0, nil
			}
			h.mustTick(metrics.RouteReconcileTickOK)
			h.c.del(key)
			getsBefore := len(h.c.fenceGets)
			if change == "Redis身份" {
				h.c.setIdentity(idB)
			} else {
				h.st.mu.Lock()
				h.st.holder, h.st.leaseUntil = "other-leader", h.st.nowMs+100_000
				h.st.mu.Unlock()
				h.clk.advance(20 * time.Second)
				h.tick()
				if h.obs.isLeader() {
					t.Fatal("别人持有库租约时必须降级")
				}
				h.st.mu.Lock()
				h.st.holder, h.st.leaseUntil = "", 0
				h.st.mu.Unlock()
			}
			h.mustTick(metrics.RouteReconcileTickOK)
			if calls := h.c.fenceScans; len(calls) != 2 || calls[1].cursor != 0 {
				t.Fatalf("新身份或任期必须从游标 0 重扫，实际 %+v", calls)
			}
			for _, keys := range h.c.fenceGets[getsBefore:] {
				for _, got := range keys {
					if got == key {
						t.Fatal("新身份或任期不得继承旧实例的 foreign 键名单")
					}
				}
			}
		})
	}
}

// 生效围栏补写遇到 foreign 后，即使旧键随即消失，本拍也还没补齐镜像，不能用“已经没有 foreign”放行。
func TestFenceMirrorActiveForeignDisappearingStillRequiresSuccessfulWrite(t *testing.T) {
	h := newHarness(t, true, nil)
	h.mustTick(metrics.RouteReconcileTickOK)
	key, holder := placement.MergeFenceKey(5), holderJSON("active", 4)
	h.st.fences = []store.ActiveMergeFence{{ZoneID: 5, RunID: "active", FenceEpoch: 4, HolderJSON: holder, ExpiresAtMs: 3_000_000}}
	h.c.set(key, `{"run_id":"legacy"}`)
	h.c.fenceGetHook = func(context.Context, []string) ([]FenceMirror, error) {
		// SetFenceMirror 已看到旧值并返回 Foreign；模拟外部清理在名单 MGET 之前删除它。
		h.c.del(key)
		return nil, nil
	}
	h.mustTick(metrics.RouteReconcileTickOK)
	if h.readyPresent() || h.c.arms != 0 {
		t.Fatal("foreign 消失不等于生效围栏已成功补写，本拍不得挂标记")
	}
	if _, present := h.c.get(key); present {
		t.Fatal("本拍围栏键应确实缺席，避免测试只靠 foreign 仍存在而通过")
	}
	h.c.fenceGetHook = nil
	h.mustTick(metrics.RouteReconcileTickOK)
	if got, present := h.c.get(key); !present || got != holder {
		t.Fatal("下一拍必须先真实写入生效围栏镜像")
	}
	if !h.readyPresent() || h.c.arms != 1 {
		t.Fatal("补写成功并取得完整扫描证据后才可挂一次标记")
	}
}
