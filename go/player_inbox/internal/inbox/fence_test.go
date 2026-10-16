package inbox

import (
	"testing"

	inboxpb "proto/player_inbox"
)

// TestDecideFenceTable 钉住设计 §7.8 的围栏表:七种结局(NO_STREAM 由调用方在没有流行时直接给出)。
func TestDecideFenceTable(t *testing.T) {
	const (
		required = inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_EPOCH_REQUIRED
		ahead    = inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_EPOCH_AHEAD
		first    = inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_FIRST
		match    = inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_MATCH
		advance  = inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_ADVANCE
		stale    = inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_STALE
	)
	cases := []struct {
		name           string
		req, fence, r  uint64
		want           inboxpb.PlayerInboxFenceVerdict
		allows, writes bool
	}{
		{"req 为 0:拒绝(fail-closed)", 0, 0, 5, required, false, false},
		{"req 为 0 且已有围栏:仍是 EPOCH_REQUIRED", 0, 3, 5, required, false, false},
		{"首次:fence=0 且 req ≤ R", 4, 0, 4, first, true, true},
		{"首次但 req 超过 R:EPOCH_AHEAD", 5, 0, 4, ahead, false, false},
		{"首次且 R 缺失(键丢失读成 0):EPOCH_AHEAD", 1, 0, 0, ahead, false, false},
		{"相等:MATCH(与 R 无关)", 7, 7, 0, match, true, false},
		{"推进:fence < req ≤ R", 8, 7, 9, advance, true, true},
		{"推进到恰好等于 R", 9, 7, 9, advance, true, true},
		{"推进但超过 R:EPOCH_AHEAD", 10, 7, 9, ahead, false, false},
		{"旧 owner:req < fence", 6, 7, 9, stale, false, false},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			got := DecideFence(tc.req, tc.fence, tc.r)
			if got != tc.want {
				t.Fatalf("DecideFence(req=%d, fence=%d, R=%d) = %v, want %v", tc.req, tc.fence, tc.r, got, tc.want)
			}
			if FenceAllows(got) != tc.allows {
				t.Errorf("FenceAllows(%v) = %v, want %v", got, !tc.allows, tc.allows)
			}
			if FenceWrites(got) != tc.writes {
				t.Errorf("FenceWrites(%v) = %v, want %v", got, !tc.writes, tc.writes)
			}
		})
	}
}

// TestNeedsEpochBound:只有 req > fence(且 req ≠ 0)时才读 R —— 这条决定核心 Redis 的读频(设计 §7.3 负载表)。
func TestNeedsEpochBound(t *testing.T) {
	cases := []struct {
		req, fence uint64
		want       bool
	}{
		{0, 0, false},
		{0, 5, false},
		{3, 5, false},
		{5, 5, false},
		{6, 5, true},
		{1, 0, true},
	}
	for _, tc := range cases {
		if got := NeedsEpochBound(tc.req, tc.fence); got != tc.want {
			t.Errorf("NeedsEpochBound(%d, %d) = %v, want %v", tc.req, tc.fence, got, tc.want)
		}
	}
}

// TestDecideFenceIgnoresRWhenNotNeeded:不需要上界时结果与 R 无关(调用方此时根本没读 R)。
func TestDecideFenceIgnoresRWhenNotNeeded(t *testing.T) {
	for _, tc := range []struct{ req, fence uint64 }{{0, 3}, {2, 3}, {3, 3}} {
		a := DecideFence(tc.req, tc.fence, 0)
		b := DecideFence(tc.req, tc.fence, 1<<40)
		if a != b {
			t.Errorf("req=%d fence=%d:R 不同结果不同 %v vs %v", tc.req, tc.fence, a, b)
		}
	}
}

// TestClassifyStale 钉住 STALE 的两种分类:R > req 是僵尸;R ≤ req 是计数器倒退(critical)。
func TestClassifyStale(t *testing.T) {
	cases := []struct {
		req, r uint64
		want   StaleKind
	}{
		{5, 9, StaleZombie},         // 旧 owner 迟到,新 owner 已铸出更大的 epoch
		{5, 5, StaleFenceRegressed}, // 设计原文:R == req
		{5, 3, StaleFenceRegressed}, // R 比请求方还小:同样只可能是倒退
	}
	for _, tc := range cases {
		if got := ClassifyStale(tc.req, tc.r); got != tc.want {
			t.Errorf("ClassifyStale(req=%d, R=%d) = %v, want %v", tc.req, tc.r, got, tc.want)
		}
	}
	if StaleUnclassified.String() != "unclassified" || StaleZombie.String() != "zombie" ||
		StaleFenceRegressed.String() != "fence_regressed" {
		t.Error("StaleKind 的标签被改了:日志 / runbook 按这些字面量检索")
	}
}

// TestAdvanceThenOldEpochIsStale:ADVANCE 之后旧 epoch 的请求一律 STALE(设计 §7.8、§11.4 验收「ADVANCE 之后旧 Ack 被拒」)。
func TestAdvanceThenOldEpochIsStale(t *testing.T) {
	fence := uint64(3)
	v := DecideFence(4, fence, 4)
	if v != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_ADVANCE {
		t.Fatalf("want ADVANCE, got %v", v)
	}
	fence = 4 // 调用方在锁内写回
	if got := DecideFence(3, fence, 4); got != inboxpb.PlayerInboxFenceVerdict_PLAYER_INBOX_FENCE_STALE {
		t.Fatalf("旧 epoch 应判 STALE,got %v", got)
	}
}
