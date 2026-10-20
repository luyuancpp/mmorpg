package logic

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strings"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/google/uuid"
	goredis "github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/logx/logtest"

	"guild/internal/constants"
	"guild/internal/data"
	pb "proto/guild"
	"shared/placement"
)

// fakeFence 让用例精确控制闸门的三种回答,不必起 Redis。
type fakeFence struct {
	merging  bool
	err      error
	calls    int
	lastZone uint32
}

func (f *fakeFence) MergeInProgress(_ context.Context, zoneID uint32) (bool, error) {
	f.calls++
	f.lastZone = zoneID
	return f.merging, f.err
}

// TestCreateGuild_RefusedWhileZoneIsMerging 是本项修复的核心:
// 合服窗口里建出来的公会会留在一个即将下线的 zone,merge_zone 已经扫过它,
// 于是会长登进目标区看不到自己的公会。
func TestCreateGuild_RefusedWhileZoneIsMerging(t *testing.T) {
	fence := &fakeFence{merging: true}
	// repo/ids 都传 nil:闸门在 CreateGuild 的**第一行**,拒绝时不该碰到它们。
	// 这条断言本身就是"闸门必须在铸号之前"的证明 —— 顺序错了这里会 nil panic。
	l := NewGuildLogic(nil, nil, nil, fence, nil)

	resp, err := l.CreateGuild(context.Background(), &pb.CreateGuildRequest{
		PlayerId: 1001, Name: "merging-zone-guild", ZoneId: 7,
	})
	// 拒绝改回 tip(§11.1):"这个区正在合服,等几分钟"是可预期的运维窗口,
	// 回 gRPC 错误会被 serverbase 记成服务端故障,客户端还会进重连隔离。
	require.NoError(t, err)
	assert.Equal(t, constants.ErrZoneMerging, resp.GetErrorMessage().GetId())
	assert.Nil(t, resp.GetGuild(), "被拒绝的建帮不能回一个公会")
	assert.Equal(t, uint32(7), fence.lastZone, "查的必须是请求里的 zone")
}

// TestCreateGuild_FenceUnreadableFailsClosed:查不到闸门状态 ≠ 没有闸门。
// 与"确实在合服"回同一个码(玩家看到的都是"稍后再试"),两者靠诊断参数区分。
func TestCreateGuild_FenceUnreadableFailsClosed(t *testing.T) {
	fence := &fakeFence{err: errors.New("dial tcp: connection refused")}
	l := NewGuildLogic(nil, nil, nil, fence, nil)

	resp, err := l.CreateGuild(context.Background(), &pb.CreateGuildRequest{
		PlayerId: 1002, Name: "unreadable-fence", ZoneId: 7,
	})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrZoneMerging, resp.GetErrorMessage().GetId())
	assert.Equal(t, []string{"merge fence unreadable"}, resp.GetErrorMessage().GetParameters(),
		"读不到闸门与真的在合服必须能在日志里分开")
}

// TestMergeFenceTip_SkippedWhenNotConfigured:没配 MergeMarkerRedis 时闸门整体跳过,
// 建帮照常 —— 否则所有还没配这段的环境会因为一个可选加固而建不了公会。
func TestMergeFenceTip_SkippedWhenNotConfigured(t *testing.T) {
	l := NewGuildLogic(nil, nil, nil, nil, nil)
	assert.Nil(t, l.mergeFenceTip(context.Background(), 7))
}

// TestMergeFenceTip_ZoneZeroIsNotChecked:zone 0 没有对应的标记键(内部调用不带 zone),
// 查它没有意义。
func TestMergeFenceTip_ZoneZeroIsNotChecked(t *testing.T) {
	fence := &fakeFence{merging: true}
	l := NewGuildLogic(nil, nil, nil, fence, nil)
	assert.Nil(t, l.mergeFenceTip(context.Background(), 0))
	assert.Zero(t, fence.calls, "zone=0 不该产生一次 Redis 往返")
}

func TestMergeFenceTip_PassesWhenMarkerAbsent(t *testing.T) {
	fence := &fakeFence{merging: false}
	l := NewGuildLogic(nil, nil, nil, fence, nil)
	assert.Nil(t, l.mergeFenceTip(context.Background(), 7))
	assert.Equal(t, 1, fence.calls)
}

// ── RedisMergeFence:与 data_service 共享的键名契约 ──────────────

func TestMergeFenceKeyContract(t *testing.T) {
	// 这串字面量是与 data_service / tools/merge_zone 的契约,
	// 三处必须一字不差(见 merge_fence.go 顶部)。
	assert.Equal(t, "merge:in_progress:42", MergeFenceKey(42))
}

func TestRedisMergeFence_ExistenceIsTheWholeContract(t *testing.T) {
	mr := miniredis.RunT(t)
	rdb := goredis.NewClient(&goredis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { rdb.Close() })
	fence := NewRedisMergeFence(rdb)
	ctx := context.Background()

	merging, err := fence.MergeInProgress(ctx, 3)
	require.NoError(t, err)
	assert.False(t, merging)

	// 值是 JSON,但闸门从不解析它 —— 键在就是封锁。
	require.NoError(t, mr.Set(MergeFenceKey(3), `{"started_at":1757000000}`))
	merging, err = fence.MergeInProgress(ctx, 3)
	require.NoError(t, err)
	assert.True(t, merging)

	// 值就算是一坨读不懂的东西也照样封锁:判据只有"存在"。
	require.NoError(t, mr.Set(MergeFenceKey(4), "not-json-at-all"))
	merging, err = fence.MergeInProgress(ctx, 4)
	require.NoError(t, err)
	assert.True(t, merging)

	// 工具跑完 DEL(或 TTL 到期)→ 放行。
	mr.Del(MergeFenceKey(3))
	merging, err = fence.MergeInProgress(ctx, 3)
	require.NoError(t, err)
	assert.False(t, merging)
}

func TestRedisMergeFence_RedisDownIsAnError(t *testing.T) {
	mr := miniredis.RunT(t)
	rdb := goredis.NewClient(&goredis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { rdb.Close() })
	fence := NewRedisMergeFence(rdb)

	mr.Close()
	_, err := fence.MergeInProgress(context.Background(), 3)
	require.Error(t, err, "Redis 不可达必须报错,由调用方 fail-closed;绝不能静默返回 false")
}

// TestNewRedisMergeFence_NilClientYieldsNilFence 钉住 guild.go 的接线前提:
// 没配时构造函数返回 nil 指针,那边据此传一个 nil **接口**给 GuildLogic。
func TestNewRedisMergeFence_NilClientYieldsNilFence(t *testing.T) {
	assert.Nil(t, NewRedisMergeFence(nil))
}

// TestMergeFenceKeyMatchesPlacement:读脚本的 KEYS 用 placement.MergeFenceKey 组装,
// 与本包(以及 data_service / merge_zone)的拼法必须一字不差,否则开关打开后闸门永远看不到键。
func TestMergeFenceKeyMatchesPlacement(t *testing.T) {
	for _, zone := range []uint32{1, 7, 42, 999999} {
		assert.Equal(t, placement.MergeFenceKey(zone), MergeFenceKey(zone))
	}
}

// ── RouteReadyMergeFence(MergeMarkerRedis.RequireRouteReady=true)──────────
//
// 规格 docs/handoff/launch-readiness-20261013/spec_D1_batch1b2_2_3.md §5.1 / §5.4:只有 Trusted 才按闸门键判,
// 其余 verdict 与读脚本出错一律返回 error(调用方当封锁)。miniredis 不支持 INFO 的 server / replication 段,
// 下面的单测经 RouteReadyReader 注入回复;脚本本身的执行语义见 TestRouteReadyMergeFence_RealRedis。

const (
	rrTestRunID      = "1111111111111111111111111111111111111111"
	rrTestReplID     = "2222222222222222222222222222222222222222"
	rrTestOtherRunID = "3333333333333333333333333333333333333333"
	rrTestOtherRepl  = "4444444444444444444444444444444444444444"
)

// fakeRouteReadyReader 回放一份读脚本回复,并记下每次调用的 KEYS。
type fakeRouteReadyReader struct {
	reply     any
	err       error
	exists    bool
	existsErr error

	readKeys   [][]string
	existsKeys []string
}

func (r *fakeRouteReadyReader) ReadRouteReady(_ context.Context, keys []string) (any, error) {
	r.readKeys = append(r.readKeys, append([]string(nil), keys...))
	return r.reply, r.err
}

func (r *fakeRouteReadyReader) KeyExists(_ context.Context, key string) (bool, error) {
	r.existsKeys = append(r.existsKeys, key)
	return r.exists, r.existsErr
}

// routeReadyTestMarker 编码一个记着 (runID, replID) 的合法就绪标记。
func routeReadyTestMarker(t *testing.T, runID, replID string) string {
	t.Helper()
	raw, err := placement.EncodeRouteReadyMarker(placement.RouteReadyMarker{
		Generation:   "0123456789abcdef0123456789abcdef",
		RedisRunID:   runID,
		MasterReplID: replID,
		SetAtMs:      1760000000000,
		WatermarkMs:  1760000000000,
		Leader:       "data_service@guild-test:1",
	})
	require.NoError(t, err)
	return raw
}

// routeReadyReply 拼读脚本的回复:{marker, run_id, role, master_replid, 映射键的值...}。
func routeReadyReply(marker any, runID, role, replID string, values ...any) []any {
	return append([]any{marker, runID, role, replID}, values...)
}

// newObservedRouteReadyFence 返回一个把 verdict label 记进 labels 的闸门。
func newObservedRouteReadyFence(reader RouteReadyReader) (*RouteReadyMergeFence, *[]string) {
	labels := &[]string{}
	f := newRouteReadyMergeFence(reader)
	f.observe = func(label string) { *labels = append(*labels, label) }
	return f, labels
}

func TestRouteReadyMergeFence_TrustedValuePresentBlocks(t *testing.T) {
	reader := &fakeRouteReadyReader{reply: routeReadyReply(routeReadyTestMarker(t, rrTestRunID, rrTestReplID),
		rrTestRunID, "master", rrTestReplID, `{"fence_epoch":3}`)}
	fence, labels := newObservedRouteReadyFence(reader)

	merging, err := fence.MergeInProgress(context.Background(), 7)
	require.NoError(t, err)
	assert.True(t, merging)
	assert.Equal(t, [][]string{{placement.RouteReadyKey, "merge:in_progress:7"}}, reader.readKeys,
		"KEYS 必须是 [就绪标记, 本 zone 的闸门键],标记在前")
	assert.Empty(t, reader.existsKeys, "值已在,不该再多一次 EXISTS")
	assert.Equal(t, []string{"trusted"}, *labels)
}

func TestRouteReadyMergeFence_TrustedValueAbsentPassesAfterExists(t *testing.T) {
	reader := &fakeRouteReadyReader{reply: routeReadyReply(routeReadyTestMarker(t, rrTestRunID, rrTestReplID),
		rrTestRunID, "master", rrTestReplID, nil)}
	fence, labels := newObservedRouteReadyFence(reader)

	merging, err := fence.MergeInProgress(context.Background(), 7)
	require.NoError(t, err)
	assert.False(t, merging, "副本可信且闸门键不在 → 放行")
	assert.Equal(t, []string{"merge:in_progress:7"}, reader.existsKeys)
	assert.Equal(t, []string{"trusted"}, *labels)
}

// TestRouteReadyMergeFence_TrustedNonStringKeyStillBlocks:MGET 对非 string 类型的键回 nil,
// 老路径的 EXISTS 对它是封锁 —— 换读法不能把它变成放行。
func TestRouteReadyMergeFence_TrustedNonStringKeyStillBlocks(t *testing.T) {
	reader := &fakeRouteReadyReader{
		reply:  routeReadyReply(routeReadyTestMarker(t, rrTestRunID, rrTestReplID), rrTestRunID, "master", rrTestReplID, nil),
		exists: true,
	}
	fence, _ := newObservedRouteReadyFence(reader)

	merging, err := fence.MergeInProgress(context.Background(), 7)
	require.NoError(t, err)
	assert.True(t, merging)
}

func TestRouteReadyMergeFence_TrustedExistsErrorIsAnError(t *testing.T) {
	reader := &fakeRouteReadyReader{
		reply:     routeReadyReply(routeReadyTestMarker(t, rrTestRunID, rrTestReplID), rrTestRunID, "master", rrTestReplID, nil),
		existsErr: errors.New("i/o timeout"),
	}
	fence, _ := newObservedRouteReadyFence(reader)

	merging, err := fence.MergeInProgress(context.Background(), 7)
	require.Error(t, err, "EXISTS 补判失败必须报错,由调用方 fail-closed")
	assert.False(t, merging)
	assert.NotErrorIs(t, err, ErrMergeFenceRouteNotReady)
}

// TestRouteReadyMergeFence_UntrustedVerdictsAreErrors 覆盖 Trusted 以外的每一种 verdict:
// 值在不在都不用、不补 EXISTS,一律返回包了 ErrMergeFenceRouteNotReady 的 error。
func TestRouteReadyMergeFence_UntrustedVerdictsAreErrors(t *testing.T) {
	good := routeReadyTestMarker(t, rrTestRunID, rrTestReplID)
	cases := []struct {
		name  string
		reply []any
		label string
	}{
		{"absent_with_value", routeReadyReply(nil, rrTestRunID, "master", rrTestReplID, `{"fence_epoch":1}`), "absent"},
		{"absent_without_value", routeReadyReply(nil, rrTestRunID, "master", rrTestReplID, nil), "absent"},
		{"malformed", routeReadyReply("not-json", rrTestRunID, "master", rrTestReplID, nil), "malformed"},
		{"newer_version", routeReadyReply(`{"v":2,"set_at_ms":"later"}`, rrTestRunID, "master", rrTestReplID, nil), "newer_version"},
		{"identity_unavailable_run_id", routeReadyReply(good, "", "master", rrTestReplID, nil), "identity_unavailable"},
		{"identity_unavailable_role", routeReadyReply(good, rrTestRunID, "", rrTestReplID, nil), "identity_unavailable"},
		{"not_master", routeReadyReply(good, rrTestRunID, "slave", rrTestReplID, nil), "not_master"},
		{"identity_mismatch_run_id", routeReadyReply(good, rrTestOtherRunID, "master", rrTestReplID, nil), "identity_mismatch"},
		{"identity_mismatch_replid", routeReadyReply(good, rrTestRunID, "master", rrTestOtherRepl, `{"fence_epoch":1}`), "identity_mismatch"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			reader := &fakeRouteReadyReader{reply: tc.reply}
			fence, labels := newObservedRouteReadyFence(reader)

			merging, err := fence.MergeInProgress(context.Background(), 7)
			require.ErrorIs(t, err, ErrMergeFenceRouteNotReady)
			assert.False(t, merging)
			assert.Contains(t, err.Error(), "verdict="+tc.label)
			assert.Empty(t, reader.existsKeys, "副本不可信时不能拿 EXISTS 补判")
			assert.Equal(t, []string{tc.label}, *labels)
		})
	}
}

// TestRouteReadyMergeFence_ReadFailuresAreErrors:脚本执行出错与回复形状不对都是 error(不是 RouteNotReady),
// label 记 script_error。
func TestRouteReadyMergeFence_ReadFailuresAreErrors(t *testing.T) {
	good := routeReadyTestMarker(t, rrTestRunID, rrTestReplID)
	cases := []struct {
		name   string
		reader *fakeRouteReadyReader
		is     error
	}{
		{"script_error", &fakeRouteReadyReader{err: errors.New("ERR This Redis command is not allowed from script")}, nil},
		{"reply_not_array", &fakeRouteReadyReader{reply: "OK"}, placement.ErrUnexpectedRouteReadyReadReply},
		{"reply_missing_value", &fakeRouteReadyReader{reply: routeReadyReply(good, rrTestRunID, "master", rrTestReplID)}, placement.ErrUnexpectedRouteReadyReadReply},
		{"reply_value_wrong_type", &fakeRouteReadyReader{reply: routeReadyReply(good, rrTestRunID, "master", rrTestReplID, int64(1))}, placement.ErrUnexpectedRouteReadyReadReply},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			fence, labels := newObservedRouteReadyFence(tc.reader)

			merging, err := fence.MergeInProgress(context.Background(), 7)
			require.Error(t, err)
			assert.False(t, merging)
			assert.NotErrorIs(t, err, ErrMergeFenceRouteNotReady)
			if tc.is != nil {
				assert.ErrorIs(t, err, tc.is)
			}
			assert.Empty(t, tc.reader.existsKeys)
			assert.Equal(t, []string{routeReadyVerdictScriptError}, *labels)
		})
	}
}

func TestRouteReadyMergeFence_ZoneZeroAndNilAreNotChecked(t *testing.T) {
	reader := &fakeRouteReadyReader{err: errors.New("must not be called")}
	fence, labels := newObservedRouteReadyFence(reader)
	merging, err := fence.MergeInProgress(context.Background(), 0)
	require.NoError(t, err)
	assert.False(t, merging)
	assert.Empty(t, reader.readKeys, "zone=0 不该产生一次 Redis 往返")
	assert.Empty(t, *labels)

	var nilFence *RouteReadyMergeFence
	merging, err = nilFence.MergeInProgress(context.Background(), 7)
	require.NoError(t, err)
	assert.False(t, merging)
	assert.Nil(t, NewRouteReadyMergeFence(nil), "没配客户端时构造函数返回 nil,guild.go 据此不装闸门")
}

// TestCreateGuild_RouteNotReadyRefusedAsMerging 钉住「调用方不改」:副本不可信的 error 走既有的
// fail-closed 分支,回 kGuildZoneMerging + "merge fence unreadable"。
func TestCreateGuild_RouteNotReadyRefusedAsMerging(t *testing.T) {
	reader := &fakeRouteReadyReader{reply: routeReadyReply(nil, rrTestRunID, "master", rrTestReplID, nil)}
	fence, _ := newObservedRouteReadyFence(reader)
	l := NewGuildLogic(nil, nil, nil, fence, nil)

	resp, err := l.CreateGuild(context.Background(), &pb.CreateGuildRequest{
		PlayerId: 1003, Name: "route-not-ready", ZoneId: 7,
	})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrZoneMerging, resp.GetErrorMessage().GetId())
	assert.Equal(t, []string{"merge fence unreadable"}, resp.GetErrorMessage().GetParameters())
	assert.Nil(t, resp.GetGuild())
}

// countLevelLines 数 logtest 收集到的、级别为 level 且 content 含 substr 的日志行(go-zero 缺省 JSON 编码,一行一条)。
func countLevelLines(t *testing.T, logs *logtest.Buffer, level, substr string) int {
	t.Helper()
	n := 0
	for _, line := range strings.Split(logs.String(), "\n") {
		if strings.TrimSpace(line) == "" {
			continue
		}
		var entry struct {
			Level   string `json:"level"`
			Content string `json:"content"`
		}
		require.NoError(t, json.Unmarshal([]byte(line), &entry), "log line is not JSON: %s", line)
		if entry.Level == level && strings.Contains(entry.Content, substr) {
			n++
		}
	}
	return n
}

// TestLogMergeFenceRefusal_RouteNotReadyIsNotAnError(M1-4):副本不可信时的拒绝(全量预热期间经闸门的每一次写都会走到这里)
// 不打「merge fence unreadable」ERROR,建帮(mergeFenceTip)与经济(economyFence)两个调用点都钉住;拒绝本身不变
// (kGuildZoneMerging / data.ErrZoneMerging)。带 verdict 的错误包装不改错误文本。
// 对照组:闸门键读失败与读脚本执行出错是真故障,两个调用点各打一行 ERROR,文案与改动前逐字相同 —— 同时证明日志收集是通的。
// 红证据:把 logMergeFenceRefusal 的 errors.Is 分支删掉(一律 ERROR),route_not_ready 子用例的 ERROR 计数变成 6。
func TestLogMergeFenceRefusal_RouteNotReadyIsNotAnError(t *testing.T) {
	ctx := context.Background()
	t.Run("route_not_ready", func(t *testing.T) {
		logs := logtest.NewCollector(t)
		fence, _ := newObservedRouteReadyFence(&fakeRouteReadyReader{reply: routeReadyReply(nil, rrTestRunID, "master", rrTestReplID, nil)})
		l := NewGuildLogic(nil, nil, nil, fence, nil)
		for i := 0; i < 3; i++ {
			tip := l.mergeFenceTip(ctx, 7)
			require.NotNil(t, tip, "an untrusted replica still refuses the write")
			assert.Equal(t, constants.ErrZoneMerging, tip.GetId())
			require.ErrorIs(t, l.economyFence(ctx, 7), data.ErrZoneMerging)
		}
		assert.Zero(t, countLevelLines(t, logs, "error", ""), "an untrusted replica is an expected state, not a fence read failure")

		_, err := fence.MergeInProgress(ctx, 7)
		require.ErrorIs(t, err, ErrMergeFenceRouteNotReady)
		assert.Equal(t, "absent", mergeFenceRouteNotReadyVerdict(err))
		assert.Contains(t, err.Error(), "merge fence check for zone 7: "+ErrMergeFenceRouteNotReady.Error()+": verdict=absent",
			"the typed wrapper keeps the error text unchanged")
	})
	for _, tc := range []struct {
		name  string
		fence func() MergeFence
	}{
		{"redis_down", func() MergeFence { return &fakeFence{err: errors.New("dial tcp: connection refused")} }},
		{"script_error", func() MergeFence {
			f, _ := newObservedRouteReadyFence(&fakeRouteReadyReader{err: errors.New("ERR This Redis command is not allowed from script")})
			return f
		}},
	} {
		t.Run(tc.name, func(t *testing.T) {
			logs := logtest.NewCollector(t)
			l := NewGuildLogic(nil, nil, nil, tc.fence(), nil)
			require.NotNil(t, l.mergeFenceTip(ctx, 7))
			require.ErrorIs(t, l.economyFence(ctx, 7), data.ErrZoneMerging)
			assert.Equal(t, 1, countLevelLines(t, logs, "error", "[guild] merge fence unreadable for zone 7, refusing (fail closed)"))
			assert.Equal(t, 1, countLevelLines(t, logs, "error", "[GuildEconomy] merge fence unreadable for zone 7, refusing (fail closed)"))
		})
	}
	assert.Equal(t, "unknown", mergeFenceRouteNotReadyVerdict(errors.New("other")))
}

// TestRouteNotReadyRefusals_AtMostOneLinePerInterval(M1-4):副本不可信的拒绝第一次立即汇总一条,之后至多每个周期一条,
// 按「调用点/verdict」排序累计;副本恢复可信后,周期满了的下一次可信判定把最后一段补出来;没有新拒绝时可信判定不打。
// 时钟与输出都注入,不依赖墙钟。用例临时换掉进程内的汇总器(本包单测不并行),结束时换回。
func TestRouteNotReadyRefusals_AtMostOneLinePerInterval(t *testing.T) {
	logtest.Discard(t)
	clock := time.Date(2026, 10, 20, 0, 0, 0, 0, time.UTC)
	var lines []string
	prev := mergeFenceRouteNotReadyLog
	mergeFenceRouteNotReadyLog = newRouteNotReadyRefusals(func() time.Time { return clock }, time.Minute, func(format string, args ...any) {
		lines = append(lines, fmt.Sprintf(format, args...))
	})
	t.Cleanup(func() { mergeFenceRouteNotReadyLog = prev })

	ctx := context.Background()
	good := routeReadyTestMarker(t, rrTestRunID, rrTestReplID)
	absentFence, _ := newObservedRouteReadyFence(&fakeRouteReadyReader{reply: routeReadyReply(nil, rrTestRunID, "master", rrTestReplID, nil)})
	mismatchFence, _ := newObservedRouteReadyFence(&fakeRouteReadyReader{reply: routeReadyReply(good, rrTestOtherRunID, "master", rrTestReplID, nil)})
	trustedFence, _ := newObservedRouteReadyFence(&fakeRouteReadyReader{reply: routeReadyReply(good, rrTestRunID, "master", rrTestReplID, nil)})
	absent := NewGuildLogic(nil, nil, nil, absentFence, nil)
	mismatch := NewGuildLogic(nil, nil, nil, mismatchFence, nil)
	trustedRead := func() {
		t.Helper()
		merging, err := trustedFence.MergeInProgress(ctx, 7)
		require.NoError(t, err)
		require.False(t, merging)
	}

	require.NotNil(t, absent.mergeFenceTip(ctx, 7))
	require.Len(t, lines, 1)
	assert.Contains(t, lines[0], "first occurrence: guild/absent=1")
	assert.Contains(t, lines[0], "guild_route_ready_verdict_total", "the summary points at the verdict metric")

	clock = clock.Add(10 * time.Second)
	require.NotNil(t, absent.mergeFenceTip(ctx, 7))
	require.ErrorIs(t, absent.economyFence(ctx, 7), data.ErrZoneMerging)
	require.ErrorIs(t, mismatch.economyFence(ctx, 8), data.ErrZoneMerging)
	clock = clock.Add(20 * time.Second)
	trustedRead()
	require.Len(t, lines, 1, "nothing more within the interval, not even on a trusted read")

	clock = clock.Add(30 * time.Second)
	trustedRead()
	require.Len(t, lines, 2, "the first trusted read after a full interval prints the last batch")
	assert.Contains(t, lines[1], "last 1m0s: GuildEconomy/absent=1 GuildEconomy/identity_mismatch=1 guild/absent=1")

	clock = clock.Add(5 * time.Minute)
	trustedRead()
	require.Len(t, lines, 2, "a trusted read without new refusals prints nothing")
	require.NotNil(t, absent.mergeFenceTip(ctx, 7))
	require.Len(t, lines, 3)
	assert.Contains(t, lines[2], "last 5m0s: guild/absent=1")
}

func TestRouteReadyMergeFence_Probe(t *testing.T) {
	good := routeReadyTestMarker(t, rrTestRunID, rrTestReplID)

	t.Run("trusted", func(t *testing.T) {
		reader := &fakeRouteReadyReader{reply: routeReadyReply(good, rrTestRunID, "master", rrTestReplID)}
		fence, labels := newObservedRouteReadyFence(reader)
		verdict, err := fence.Probe(context.Background())
		require.NoError(t, err)
		assert.Equal(t, placement.RouteReadyTrusted, verdict)
		assert.Equal(t, [][]string{{placement.RouteReadyKey}}, reader.readKeys, "探针不带映射键")
		assert.Empty(t, *labels, "探针不计指标")
	})
	t.Run("untrusted_is_not_an_error", func(t *testing.T) {
		reader := &fakeRouteReadyReader{reply: routeReadyReply(nil, rrTestRunID, "master", rrTestReplID)}
		fence, _ := newObservedRouteReadyFence(reader)
		verdict, err := fence.Probe(context.Background())
		require.NoError(t, err, "标记还没挂上不算启动失败")
		assert.Equal(t, placement.RouteReadyAbsent, verdict)
	})
	t.Run("script_error_refuses_start", func(t *testing.T) {
		reader := &fakeRouteReadyReader{err: errors.New("ERR This Redis command is not allowed from script")}
		fence, _ := newObservedRouteReadyFence(reader)
		verdict, err := fence.Probe(context.Background())
		require.Error(t, err)
		assert.NotEqual(t, placement.RouteReadyTrusted, verdict, "出错时不能回零值 Trusted")
	})
	t.Run("bad_shape_refuses_start", func(t *testing.T) {
		reader := &fakeRouteReadyReader{reply: routeReadyReply(good, rrTestRunID, "master", rrTestReplID, nil)}
		fence, _ := newObservedRouteReadyFence(reader)
		_, err := fence.Probe(context.Background())
		require.ErrorIs(t, err, placement.ErrUnexpectedRouteReadyReadReply)
	})
	t.Run("nil_fence_refuses_start", func(t *testing.T) {
		var nilFence *RouteReadyMergeFence
		_, err := nilFence.Probe(context.Background())
		require.Error(t, err)
	})
}

// TestRouteReadyVerdictLabelsAreClosed:label 全集 = placement 的全部 verdict + script_error,无重复;
// 每个 verdict 的 String() 都在集合里(PrimeRouteReadyMetrics 按它预置)。读脚本失败的 label 用字面量钉住:
// 与 data_service、go/db 的 route_ready_verdict_total 同名,跨服务面板按同一个字符串过滤,改名先红。
func TestRouteReadyVerdictLabelsAreClosed(t *testing.T) {
	labels := routeReadyVerdictLabels()
	seen := map[string]bool{}
	for _, l := range labels {
		assert.False(t, seen[l], "label %q 重复", l)
		seen[l] = true
	}
	assert.Equal(t, "script_error", routeReadyVerdictScriptError)
	assert.True(t, seen[routeReadyVerdictScriptError])
	for _, v := range []placement.RouteReadyVerdict{
		placement.RouteReadyTrusted, placement.RouteReadyAbsent, placement.RouteReadyMalformed, placement.RouteReadyNewerVersion,
		placement.RouteReadyIdentityUnavailable, placement.RouteReadyNotMaster, placement.RouteReadyIdentityMismatch,
	} {
		assert.True(t, seen[v.String()], "verdict %s 不在 label 全集里", v)
	}
	assert.Len(t, labels, len(placement.RouteReadyVerdictNames())+1)
}

// prefixRouteReadyReader 给所有 KEYS 加一次性前缀:真 Redis 用例不碰真正的 mapping:route:ready 与闸门键。
type prefixRouteReadyReader struct {
	inner  RouteReadyReader
	prefix string
}

func (p prefixRouteReadyReader) ReadRouteReady(ctx context.Context, keys []string) (any, error) {
	out := make([]string, len(keys))
	for i, k := range keys {
		out[i] = p.prefix + k
	}
	return p.inner.ReadRouteReady(ctx, out)
}

func (p prefixRouteReadyReader) KeyExists(ctx context.Context, key string) (bool, error) {
	return p.inner.KeyExists(ctx, p.prefix+key)
}

// flipFirstHex 返回一个与 s 只差第一个字符的十六进制串(构造「别的实例」的身份)。
func flipFirstHex(s string) string {
	if s[0] == '0' {
		return "1" + s[1:]
	}
	return "0" + s[1:]
}

// TestRouteReadyMergeFence_RealRedis 在真 Redis 上执行 placement.RouteReadyReadScript
// (miniredis 不支持 INFO 的 server / replication 段):证明脚本里能调 INFO、回复形状与 ParseRouteReadyRead 一致、
// 身份绑定生效、非 string 闸门键仍封锁。键全部带一次性前缀,结束时删除;实例必须是主。
//
// 运行(Codex):GUILD_ROUTE_READY_IT_REDIS=127.0.0.1:6379;设 GUILD_ROUTE_READY_IT_REQUIRE=1 时缺地址判 FAIL 而不是 SKIP。
// 地址给了却连不上同样是 FAIL(不是 SKIP)。
func TestRouteReadyMergeFence_RealRedis(t *testing.T) {
	addr := os.Getenv("GUILD_ROUTE_READY_IT_REDIS")
	if addr == "" {
		if os.Getenv("GUILD_ROUTE_READY_IT_REQUIRE") == "1" {
			t.Fatal("GUILD_ROUTE_READY_IT_REQUIRE=1 but GUILD_ROUTE_READY_IT_REDIS is empty")
		}
		t.Skip("set GUILD_ROUTE_READY_IT_REDIS to run the route ready read script on a real Redis (miniredis lacks INFO server/replication)")
	}
	rdb := goredis.NewClient(&goredis.Options{Addr: addr, DialTimeout: 3 * time.Second, ReadTimeout: 3 * time.Second, WriteTimeout: 3 * time.Second})
	t.Cleanup(func() { _ = rdb.Close() })
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	infoServer, err := rdb.Info(ctx, "server").Result()
	require.NoError(t, err)
	infoRepl, err := rdb.Info(ctx, "replication").Result()
	require.NoError(t, err)
	id, err := placement.ParseRedisInstanceIdentity(infoServer, infoRepl)
	require.NoError(t, err)
	require.Equal(t, "master", id.Role, "GUILD_ROUTE_READY_IT_REDIS must point at a master")

	prefix := "codex:guild-route-ready:" + uuid.NewString() + ":"
	readyKey := prefix + placement.RouteReadyKey
	stringFence := prefix + MergeFenceKey(7)
	hashFence := prefix + MergeFenceKey(8)
	t.Cleanup(func() {
		cctx, ccancel := context.WithTimeout(context.Background(), 3*time.Second)
		defer ccancel()
		_ = rdb.Del(cctx, readyKey, stringFence, hashFence).Err()
	})
	fence, labels := newObservedRouteReadyFence(prefixRouteReadyReader{inner: redisRouteReadyReader{rdb: rdb}, prefix: prefix})

	// 1. 标记不在:不可信,探针不报错。
	_, err = fence.MergeInProgress(ctx, 7)
	require.ErrorIs(t, err, ErrMergeFenceRouteNotReady)
	verdict, err := fence.Probe(ctx)
	require.NoError(t, err, "INFO must be callable from a script on this Redis")
	assert.Equal(t, placement.RouteReadyAbsent, verdict)

	// 2. 挂上本实例身份的标记:可信,按闸门键判。
	require.NoError(t, rdb.Set(ctx, readyKey, routeReadyTestMarker(t, id.RunID, id.MasterReplID), 0).Err())
	verdict, err = fence.Probe(ctx)
	require.NoError(t, err)
	assert.Equal(t, placement.RouteReadyTrusted, verdict)
	merging, err := fence.MergeInProgress(ctx, 7)
	require.NoError(t, err)
	assert.False(t, merging)
	require.NoError(t, rdb.Set(ctx, stringFence, `{"fence_epoch":1}`, 0).Err())
	merging, err = fence.MergeInProgress(ctx, 7)
	require.NoError(t, err)
	assert.True(t, merging)
	require.NoError(t, rdb.HSet(ctx, hashFence, "f", "v").Err())
	merging, err = fence.MergeInProgress(ctx, 8)
	require.NoError(t, err)
	assert.True(t, merging, "非 string 的闸门键 MGET 回 nil,仍须经 EXISTS 封锁")

	// 3. 标记记的是别的实例:不可信。
	require.NoError(t, rdb.Set(ctx, readyKey, routeReadyTestMarker(t, flipFirstHex(id.RunID), id.MasterReplID), 0).Err())
	_, err = fence.MergeInProgress(ctx, 7)
	require.ErrorIs(t, err, ErrMergeFenceRouteNotReady)
	assert.Equal(t, "identity_mismatch", (*labels)[len(*labels)-1])

	// 4. 标记不是 string:按缺席处理,脚本不报错。
	require.NoError(t, rdb.Del(ctx, readyKey).Err())
	require.NoError(t, rdb.HSet(ctx, readyKey, "f", "v").Err())
	_, err = fence.MergeInProgress(ctx, 7)
	require.ErrorIs(t, err, ErrMergeFenceRouteNotReady)
	assert.Equal(t, "absent", (*labels)[len(*labels)-1])
}
