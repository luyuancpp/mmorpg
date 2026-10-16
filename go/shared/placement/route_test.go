package placement

import (
	"errors"
	"math"
	"testing"
)

// revValidVectors / revInvalidVectors 是 rev 规范形式的测试向量。发布脚本的 valid_rev 与 data_service 的
// 导入 / 审计(routetool)都按这一口径判 rev;改这里必须同步看 PublishRouteScript 与 ValidRev。
var revValidVectors = []string{
	"1",
	"9",
	"10",
	"18446744073709551615", // MaxUint64
}

var revInvalidVectors = []string{
	"",
	"0",
	"01",
	"-1",
	"1a",
	" 1",
	"1 ",
	"+1",
	"123456789012345678901", // 21 位
	"18446744073709551616",  // MaxUint64 + 1:20 位但溢出
	"99999999999999999999",
}

func TestRouteKeys(t *testing.T) {
	if got := MapRevKey(90002); got != "player:maprev:90002" {
		t.Fatalf("MapRevKey = %q", got)
	}
	if RouteReadyKey != "mapping:route:ready" {
		t.Fatalf("RouteReadyKey = %q", RouteReadyKey)
	}

	three := PublishRouteKeys(7, 0)
	wantThree := []string{"player:zone:7", "player:placement:7", "player:maprev:7"}
	if len(three) != len(wantThree) {
		t.Fatalf("PublishRouteKeys(7, 0) = %v, want %v(fenceZone=0 不带围栏键)", three, wantThree)
	}
	for i := range wantThree {
		if three[i] != wantThree[i] {
			t.Fatalf("PublishRouteKeys(7, 0)[%d] = %q, want %q(KEYS 顺序是脚本契约)", i, three[i], wantThree[i])
		}
	}

	four := PublishRouteKeys(7, 2)
	if len(four) != 4 {
		t.Fatalf("PublishRouteKeys(7, 2) = %v, want 4 keys", four)
	}
	if four[3] != MergeFenceKey(2) {
		t.Fatalf("PublishRouteKeys(7, 2)[3] = %q, want %q(第 4 把必须是围栏键,脚本按 KEYS[4] 复核)", four[3], MergeFenceKey(2))
	}
	for i := range wantThree {
		if four[i] != wantThree[i] {
			t.Fatalf("PublishRouteKeys(7, 2)[%d] = %q, want %q", i, four[i], wantThree[i])
		}
	}
}

func TestRevVectors(t *testing.T) {
	for _, s := range revValidVectors {
		if !ValidRev(s) {
			t.Fatalf("ValidRev(%q) = false, want true", s)
		}
	}
	for _, s := range revInvalidVectors {
		if ValidRev(s) {
			t.Fatalf("ValidRev(%q) = true, want false", s)
		}
	}

	if got := FormatRev(0); got != "" {
		t.Fatalf("FormatRev(0) = %q, want empty(0 不是合法版本)", got)
	}
	for _, rev := range []uint64{1, 10, 18446744073709551615} {
		s := FormatRev(rev)
		if !ValidRev(s) {
			t.Fatalf("FormatRev(%d) = %q 过不了 ValidRev", rev, s)
		}
	}
}

func TestCompareRevLengthBeforeLexicographic(t *testing.T) {
	for _, tc := range []struct {
		a, b string
		want int
	}{
		{"9", "10", -1}, // 字典序会说 "9" > "10",必须先比长度
		{"2", "10", -1},
		{"10", "9", 1},
		{"10", "10", 0},
		{"18446744073709551615", "18446744073709551614", 1},
		{"18446744073709551614", "18446744073709551615", -1},
	} {
		got, err := CompareRev(tc.a, tc.b)
		if err != nil {
			t.Fatalf("CompareRev(%q, %q) error: %v", tc.a, tc.b, err)
		}
		if got != tc.want {
			t.Fatalf("CompareRev(%q, %q) = %d, want %d", tc.a, tc.b, got, tc.want)
		}
	}
	for _, bad := range []string{"", "01", "18446744073709551616"} {
		if _, err := CompareRev(bad, "1"); !errors.Is(err, ErrMalformed) {
			t.Fatalf("CompareRev(%q, \"1\") error = %v, want ErrMalformed", bad, err)
		}
		if _, err := CompareRev("1", bad); !errors.Is(err, ErrMalformed) {
			t.Fatalf("CompareRev(\"1\", %q) error = %v, want ErrMalformed", bad, err)
		}
	}
}

func TestClassifyPublishReply(t *testing.T) {
	const (
		rev  = "5"
		home = "2"
		plc  = "2:1"
	)
	for _, tc := range []struct {
		name string
		raw  any
		want PublishReply
	}{
		{
			name: "已写入",
			raw:  []any{int64(1), rev},
			want: PublishReply{Outcome: PublishApplied, CurRev: rev, CurHome: home, CurPlacement: plc},
		},
		{
			name: "缓存更新_被取代",
			raw:  []any{int64(0), "10", home, plc},
			want: PublishReply{Outcome: PublishSuperseded, CurRev: "10", CurHome: home, CurPlacement: plc},
		},
		{
			name: "同版本同值_已确认",
			raw:  []any{int64(0), rev, home, plc},
			want: PublishReply{Outcome: PublishConfirmed, CurRev: rev, CurHome: home, CurPlacement: plc},
		},
		{
			name: "同版本_home不同_冲突",
			raw:  []any{int64(0), rev, "3", plc},
			want: PublishReply{Outcome: PublishConflict, CurRev: rev, CurHome: "3", CurPlacement: plc},
		},
		{
			name: "同版本_落点不同_冲突",
			raw:  []any{int64(0), rev, home, "2:2"},
			want: PublishReply{Outcome: PublishConflict, CurRev: rev, CurHome: home, CurPlacement: "2:2"},
		},
		{
			name: "无maprev_遗留值不同",
			raw:  []any{int64(2), "", "3", ""},
			want: PublishReply{Outcome: PublishLegacyConflict, CurHome: "3", CurPlacement: ""},
		},
		{
			name: "合服围栏",
			raw:  []any{int64(3)},
			want: PublishReply{Outcome: PublishFenced},
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			got, err := ClassifyPublishReply(tc.raw, rev, home, plc)
			if err != nil {
				t.Fatalf("ClassifyPublishReply(%#v) error: %v", tc.raw, err)
			}
			if got != tc.want {
				t.Fatalf("ClassifyPublishReply(%#v) = %+v, want %+v", tc.raw, got, tc.want)
			}
		})
	}

	// 脚本不可能产生的回复:一律 fail-closed。
	for _, tc := range []struct {
		name string
		raw  any
	}{
		{"已写入但rev不是本次", []any{int64(1), "6"}},
		{"已写入但rev不是串", []any{int64(1), int64(5)}},
		{"未改但缓存rev小于本次", []any{int64(0), "4", home, plc}},
		{"未改但缓存rev是20位且溢出", []any{int64(0), "18446744073709551616", home, plc}},
		{"未改但缓存rev有前导零", []any{int64(0), "05", home, plc}},
		{"未改但元素个数不对", []any{int64(0), rev, home}},
		{"遗留回复但值反而相同", []any{int64(2), "", home, plc}},
		{"遗留回复带了rev", []any{int64(2), "1", "3", ""}},
		{"nil", nil},
		{"空数组", []any{}},
		{"首元素不是整数", []any{"1", "3"}},
		{"未知回复码", []any{int64(4)}},
		{"围栏回复多了元素", []any{int64(3), "x"}},
		{"不是数组", "OK"},
	} {
		t.Run("拒绝_"+tc.name, func(t *testing.T) {
			if got, err := ClassifyPublishReply(tc.raw, rev, home, plc); !errors.Is(err, ErrUnexpectedPublishReply) {
				t.Fatalf("ClassifyPublishReply(%#v) = (%+v, %v), want ErrUnexpectedPublishReply", tc.raw, got, err)
			}
		})
	}
}

func TestPublishOutcomeStringsAreStableLowCardinality(t *testing.T) {
	for _, tc := range []struct {
		got, want string
	}{
		{PublishApplied.String(), "applied"},
		{PublishSuperseded.String(), "superseded"},
		{PublishConfirmed.String(), "confirmed"},
		{PublishConflict.String(), "conflict"},
		{PublishLegacyConflict.String(), "legacy_conflict"},
		{PublishFenced.String(), "fenced"},
		{PublishOutcome(0).String(), "unknown"},
	} {
		if tc.got != tc.want {
			t.Fatalf("outcome label = %q, want %q", tc.got, tc.want)
		}
	}
}

func TestRouteFenceShard(t *testing.T) {
	if got, err := RouteFenceShard(17, 8); err != nil || got != 1 {
		t.Fatalf("RouteFenceShard(17, 8) = (%d, %v), want (1, nil)", got, err)
	}
	if got, err := RouteFenceShard(17, 1); err != nil || got != 0 {
		t.Fatalf("RouteFenceShard(17, 1) = (%d, %v), want (0, nil)", got, err)
	}
	if got, err := RouteFenceShard(127, MaxRouteFenceShards); err != nil || got != 63 {
		t.Fatalf("RouteFenceShard(127, 64) = (%d, %v), want (63, nil)", got, err)
	}
	for _, shards := range []uint32{0, MaxRouteFenceShards + 1} {
		if _, err := RouteFenceShard(17, shards); err == nil {
			t.Fatalf("RouteFenceShard(17, %d) 应报错(只接受 [1, %d])", shards, MaxRouteFenceShards)
		}
	}
}

// TestRouteRowShard 钉住 player_route 的行分片:模数是聚簇主键的一部分,任何环境建过表之后改它,
// 存量行的 route_shard 全部与新算法不一致(按新片点查读不到旧行,CHECK 也会拒绝新写)。
func TestRouteRowShard(t *testing.T) {
	if RouteRowShards != 64 {
		t.Fatalf("RouteRowShards = %d, want 64:改它会让存量行的主键全部失效(player_route 聚簇主键首列 = player_id %% 64,"+
			"schema.go 的 chk_route_shard 写死 64),只能新表迁移", RouteRowShards)
	}
	for _, tc := range []struct {
		id   uint64
		want uint32
	}{
		{0, 0},
		{1, 1},
		{15, 15},
		{16, 16}, // 模数还是 16 的话这里是 0
		{63, 63}, // 最后一片
		{64, 0},  // 回绕
		{65, 1},
		// 4242 = 66×64 + 18;后两个的低 6 位全 1。
		{4242, 18},
		{1<<55 - 1, 63},
		{math.MaxUint64, 63},
	} {
		if got := RouteRowShard(tc.id); got != tc.want {
			t.Fatalf("RouteRowShard(%d) = %d, want %d", tc.id, got, tc.want)
		}
	}
}
