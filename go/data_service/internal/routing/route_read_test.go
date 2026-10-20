package routing

// 第 3 批读方门禁(route_read.go)的编排:miniredis + 假 RouteReader + 假脚本执行器,不连库。
//
// miniredis 不支持 INFO 的 server / replication 段(读脚本在它上面必然报错),所以「可信」路径要绕开脚本执行:
// fakeReadyRedis 按两段读脚本的语义在 miniredis 的数据上拼回复(身份由用例给),注入 Router.routeReadEval。
// 脚本本身在真 Redis 上的语义由 router_integration_test.go 的 TestRouteReadyRead_RealRedis* 验;miniredis 上真跑脚本
// 必然失败这件事本身也有用 —— 它就是「脚本执行出错 → 回源 SQL」与启动探针拒启的用例(TestProbeRouteReadyRead_*)。

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"testing"

	"data_service/internal/config"
	"data_service/internal/store"

	"shared/placement"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// 假实例的身份(40 个小写十六进制字符)与标记里的世代。
const (
	readyRunID      = "0123456789abcdef0123456789abcdef01234567"
	readyReplID     = "89abcdef0123456789abcdef0123456789abcdef"
	readyGeneration = "00112233445566778899aabbccddeeff"
)

var errReadBoom = errors.New("player_route: connection reset by peer")

// readyMarkerFor 编码一个记下 (runID, replID) 的合法标记。
func readyMarkerFor(t *testing.T, runID, replID string) string {
	t.Helper()
	raw, err := placement.EncodeRouteReadyMarker(placement.RouteReadyMarker{
		Generation:   readyGeneration,
		RedisRunID:   runID,
		MasterReplID: replID,
		SetAtMs:      1,
		WatermarkMs:  1,
		Leader:       "data_service@unit-test",
	})
	require.NoError(t, err)
	return raw
}

// fakeReadyRedis 在 miniredis 的数据上模拟两段读脚本。runID / role / replID 是「本实例」此刻的身份;
// err 非 nil 时模拟脚本执行出错,reply 非 nil 时原样回它(造形状不对的回复);mismatchOnCall 让第 n 次调用(从 1 起)
// 回一个不同的 run_id(造「这一块不可信」)。calls 记下每次的 KEYS,args 记下每次的 ARGV。
type fakeReadyRedis struct {
	mr             *miniredis.Miniredis
	runID          string
	role           string
	replID         string
	err            error
	reply          any
	mismatchOnCall int
	scripts        []string
	calls          [][]string
	args           [][]any
}

func (f *fakeReadyRedis) eval(_ context.Context, script string, keys []string, args ...any) (any, error) {
	f.scripts = append(f.scripts, script)
	f.calls = append(f.calls, append([]string(nil), keys...))
	f.args = append(f.args, append([]any(nil), args...))
	if f.err != nil {
		return nil, f.err
	}
	if f.reply != nil {
		return f.reply, nil
	}
	get := func(k string) any {
		if v, err := f.mr.Get(k); err == nil {
			return v
		}
		return nil
	}
	runID := f.runID
	if f.mismatchOnCall == len(f.calls) {
		runID = strings.Repeat("f", 40)
	}
	out := []any{get(keys[0]), runID, f.role, f.replID}
	switch script {
	case placement.RouteReadyReadScript:
		for _, k := range keys[1:] {
			out = append(out, get(k))
		}
	case getHomeZoneFenceReadyScript:
		home := get(keys[1])
		if home == nil {
			return append(out, nil, nil), nil
		}
		fence := "0"
		if f.mr.Exists(args[0].(string) + home.(string)) {
			fence = "1"
		}
		out = append(out, home, fence)
	default:
		return nil, fmt.Errorf("fakeReadyRedis: unexpected script %q", script)
	}
	return out, nil
}

// readFakeReader 是 RouteReader 的假实现:按预置数据回答,记下每次入参。err 非 nil 时三个方法都回它;
// failGetRoutesOn 让第 n 次 GetRoutes(从 1 起)失败。pages 按调用顺序作为 ScanRoutesByHome 的每一页。
type readFakeReader struct {
	homeFence       map[uint64]store.RouteHomeFence
	rows            map[uint64]store.RouteRow
	err             error
	failGetRoutesOn int
	pages           [][]store.RouteRow
	homeFenceCalls  []uint64
	getRoutesCalls  [][]uint64
	scanCalls       []store.RouteScanCursor
	scanLimits      []int
}

var _ RouteReader = (*readFakeReader)(nil)

// 接口一变,编译期先炸:*store.RouteStore 必须逐字满足 RouteReader(svc 把同一个实例注入两边)。
var _ RouteReader = (*store.RouteStore)(nil)

func (f *readFakeReader) GetHomeAndFence(_ context.Context, playerID uint64) (store.RouteHomeFence, error) {
	f.homeFenceCalls = append(f.homeFenceCalls, playerID)
	if f.err != nil {
		return store.RouteHomeFence{}, f.err
	}
	return f.homeFence[playerID], nil
}

func (f *readFakeReader) GetRoutes(_ context.Context, ids []uint64) (map[uint64]store.RouteRow, error) {
	f.getRoutesCalls = append(f.getRoutesCalls, append([]uint64(nil), ids...))
	if f.err != nil {
		return nil, f.err
	}
	if f.failGetRoutesOn != 0 && len(f.getRoutesCalls) == f.failGetRoutesOn {
		return nil, errReadBoom
	}
	out := make(map[uint64]store.RouteRow, len(ids))
	for _, id := range ids {
		if row, ok := f.rows[id]; ok {
			out[id] = row
		}
	}
	return out, nil
}

func (f *readFakeReader) ScanRoutesByHome(_ context.Context, _ uint32, after store.RouteScanCursor, limit int) ([]store.RouteRow, error) {
	f.scanCalls = append(f.scanCalls, after)
	f.scanLimits = append(f.scanLimits, limit)
	if f.err != nil {
		return nil, f.err
	}
	n := len(f.scanCalls) - 1
	if n >= len(f.pages) {
		return nil, nil
	}
	return f.pages[n], nil
}

// newReadGateRouter 建一个 db_first 的 Router(mapping Redis 指向 miniredis),开关按 on 取值;reader 非 nil 时注入
// (不能把 nil 的 *readFakeReader 传给 WithRouteReader,那是「非 nil 接口包着 nil 指针」)。读脚本的执行换成 fakeReadyRedis,
// 身份与标记匹配(调用方再改字段造各种 verdict),标记默认已挂。
func newReadGateRouter(t *testing.T, on bool, reader *readFakeReader) (*Router, *miniredis.Miniredis, *fakeReadyRedis) {
	t.Helper()
	mr := miniredis.RunT(t)
	c := config.Config{
		MappingRedis:     redis.RedisConf{Host: mr.Addr(), Type: "node"},
		DevRedis:         config.DevRedisConfig{Host: mr.Addr()},
		PlayerLockTTLSec: 3,
		RouteStore: config.RouteStoreConfig{
			Mode:               config.RouteStoreModeDBFirst,
			WaitAOFNumLocal:    intPtr(0),
			WaitAOFNumReplicas: intPtr(0),
			ReadRequiresReady:  on,
		},
	}
	var opts []RouterOption
	if reader != nil {
		opts = append(opts, WithRouteReader(reader))
	}
	r := NewRouter(c, opts...)
	t.Cleanup(r.Close)
	require.Equal(t, on, r.readRequiresReady, "NewRouter must take RouteStore.ReadRequiresReady")
	fake := &fakeReadyRedis{mr: mr, runID: readyRunID, role: "master", replID: readyReplID}
	require.NoError(t, mr.Set(placement.RouteReadyKey, readyMarkerFor(t, readyRunID, readyReplID)))
	r.routeReadEval = fake.eval
	return r, mr, fake
}

// TestHomeFenceReadyScriptSharesIdentityLines:R2 的读脚本取标记与身份的几行必须与 placement.RouteReadyReadScript 逐字相同
// (身份口径只有一份;共享脚本一改,这里先红,提醒同步改 getHomeZoneFenceReadyScript)。另钉住 home 与围栏在同一段脚本里读。
func TestHomeFenceReadyScriptSharesIdentityLines(t *testing.T) {
	for _, line := range []string{
		"local m = redis.pcall('GET', KEYS[1])",
		"if type(m) ~= 'string' then m = false end",
		"local srv = redis.call('INFO', 'server')",
		"local repl = redis.call('INFO', 'replication')",
		"string.match(srv, 'run_id:(%x+)') or ''",
		"string.match(repl, 'role:(%a+)') or ''",
		"string.match(repl, 'master_replid:(%x+)') or ''",
	} {
		require.Contains(t, placement.RouteReadyReadScript, line, "the shared read script changed; resync getHomeZoneFenceReadyScript")
		assert.Contains(t, getHomeZoneFenceReadyScript, line)
	}
	for _, line := range []string{
		"local home = redis.call('GET', KEYS[2])",
		"local fenced = redis.pcall('EXISTS', ARGV[1] .. home)",
	} {
		assert.Contains(t, getHomeZoneFenceReadyScript, line, "home and fence must be read in the same script as the identity")
	}
}

// TestReadGateOffKeepsTodaysPaths:开关为 false 时 R2 / R3 / R6 与今天逐字相同 —— 一次读脚本都不跑、回源读者一次都不碰,
// 结果与缺席 / 故障的口径照旧(缺席 NotFound,不是 ErrRouteReadUnavailable)。
func TestReadGateOffKeepsTodaysPaths(t *testing.T) {
	reader := &readFakeReader{homeFence: map[uint64]store.RouteHomeFence{8201: {HomeZone: 9}}}
	r, mr, fake := newReadGateRouter(t, false, reader)
	ctx := context.Background()
	require.NoError(t, mr.Set(mappingKey(8201), "5"))
	require.NoError(t, mr.Set(MergeFenceKey(5), `{"started_at":1757000000}`))

	zone, merging, err := r.GetPlayerHomeZoneAndMergeFence(ctx, 8201)
	require.NoError(t, err)
	assert.Equal(t, uint32(5), zone)
	assert.True(t, merging)
	zone, err = r.GetPlayerHomeZone(ctx, 8201)
	require.NoError(t, err)
	assert.Equal(t, uint32(5), zone)
	got, err := r.BatchGetPlayerHomeZone(ctx, []uint64{8201, 8202})
	require.NoError(t, err)
	assert.Equal(t, map[uint64]uint32{8201: 5}, got)

	_, err = r.GetPlayerHomeZone(ctx, 8202)
	require.ErrorIs(t, err, ErrHomeZoneNotMapped)
	assert.NotErrorIs(t, err, ErrRouteReadUnavailable)

	assert.Empty(t, fake.calls, "the ready read script must not run while ReadRequiresReady=false")
	assert.Empty(t, reader.homeFenceCalls)
	assert.Empty(t, reader.getRoutesCalls)
}

// TestReadyHomeFence_TrustedUsesCache:标记身份与本实例相符 → 用缓存(含合服围栏),缺键是权威缺席,不碰库;
// 脚本的 KEYS / ARGV 是 [ready, player:zone:{id}] / [围栏前缀]。
func TestReadyHomeFence_TrustedUsesCache(t *testing.T) {
	reader := &readFakeReader{}
	r, mr, fake := newReadGateRouter(t, true, reader)
	ctx := context.Background()
	require.NoError(t, mr.Set(mappingKey(8301), "12"))

	zone, merging, err := r.GetPlayerHomeZoneAndMergeFence(ctx, 8301)
	require.NoError(t, err)
	assert.Equal(t, uint32(12), zone)
	assert.False(t, merging)
	require.Len(t, fake.calls, 1)
	assert.Equal(t, getHomeZoneFenceReadyScript, fake.scripts[0])
	assert.Equal(t, []string{placement.RouteReadyKey, mappingKey(8301)}, fake.calls[0])
	assert.Equal(t, []any{mergeFenceKeyPrefix}, fake.args[0])

	require.NoError(t, mr.Set(MergeFenceKey(12), `{"started_at":1757000000}`))
	zone, merging, err = r.GetPlayerHomeZoneAndMergeFence(ctx, 8301)
	require.NoError(t, err)
	assert.Equal(t, uint32(12), zone)
	assert.True(t, merging, "the cached merge fence is honoured when the replica is trusted")

	_, _, err = r.GetPlayerHomeZoneAndMergeFence(ctx, 8399)
	require.ErrorIs(t, err, ErrHomeZoneNotMapped, "a trusted replica's missing key is authoritative absence")
	assert.NotErrorIs(t, err, ErrRouteReadUnavailable)

	require.NoError(t, mr.Set(mappingKey(8302), "abc"))
	_, _, err = r.GetPlayerHomeZoneAndMergeFence(ctx, 8302)
	require.Error(t, err)
	assert.NotErrorIs(t, err, ErrHomeZoneNotMapped, "a malformed cached value is a fault, not absence")

	assert.Empty(t, reader.homeFenceCalls, "a trusted replica must not fall back to player_route")
}

// readyUntrustedCases 是「副本不可信」的每一种来源:六个非 Trusted 的 verdict、脚本执行出错、回复形状不对。
func readyUntrustedCases() []struct {
	name  string
	setup func(mr *miniredis.Miniredis, f *fakeReadyRedis)
} {
	return []struct {
		name  string
		setup func(mr *miniredis.Miniredis, f *fakeReadyRedis)
	}{
		{"absent", func(mr *miniredis.Miniredis, _ *fakeReadyRedis) { mr.Del(placement.RouteReadyKey) }},
		{"malformed", func(mr *miniredis.Miniredis, _ *fakeReadyRedis) { _ = mr.Set(placement.RouteReadyKey, "not-json") }},
		{"newer_version", func(mr *miniredis.Miniredis, _ *fakeReadyRedis) { _ = mr.Set(placement.RouteReadyKey, `{"v":2}`) }},
		{"identity_unavailable", func(_ *miniredis.Miniredis, f *fakeReadyRedis) { f.runID = "" }},
		{"not_master", func(_ *miniredis.Miniredis, f *fakeReadyRedis) { f.role = "slave" }},
		{"identity_mismatch (restart)", func(_ *miniredis.Miniredis, f *fakeReadyRedis) { f.runID = strings.Repeat("a", 40) }},
		{"identity_mismatch (failover)", func(_ *miniredis.Miniredis, f *fakeReadyRedis) { f.replID = strings.Repeat("b", 40) }},
		{"script error", func(_ *miniredis.Miniredis, f *fakeReadyRedis) {
			f.err = errors.New("ERR This Redis command is not allowed from script")
		}},
		{"unexpected reply shape", func(_ *miniredis.Miniredis, f *fakeReadyRedis) { f.reply = []any{"x"} }},
	}
}

// TestReadyHomeFence_UntrustedFallsBackToSQL:副本不可信时值与缺键都不用 —— 缓存里是 5、库里是 9(封锁),回的是库;
// 缓存缺键、库里有行,回的也是库;库里没有,才 NotFound。回源**不回填缓存**。
func TestReadyHomeFence_UntrustedFallsBackToSQL(t *testing.T) {
	for _, tc := range readyUntrustedCases() {
		t.Run(tc.name, func(t *testing.T) {
			reader := &readFakeReader{homeFence: map[uint64]store.RouteHomeFence{
				8401: {HomeZone: 9, Fenced: true},
				8402: {HomeZone: 11},
			}}
			r, mr, fake := newReadGateRouter(t, true, reader)
			ctx := context.Background()
			require.NoError(t, mr.Set(mappingKey(8401), "5"))
			tc.setup(mr, fake)

			zone, merging, err := r.GetPlayerHomeZoneAndMergeFence(ctx, 8401)
			require.NoError(t, err)
			assert.Equal(t, uint32(9), zone, "an untrusted cached value must not be used")
			assert.True(t, merging, "the fence comes from the same SQL snapshot as the home")

			zone, merging, err = r.GetPlayerHomeZoneAndMergeFence(ctx, 8402)
			require.NoError(t, err, "an untrusted missing key is not absence")
			assert.Equal(t, uint32(11), zone)
			assert.False(t, merging)
			assert.False(t, mr.Exists(mappingKey(8402)), "readers never backfill the cache")

			_, _, err = r.GetPlayerHomeZoneAndMergeFence(ctx, 8403)
			require.ErrorIs(t, err, ErrHomeZoneNotMapped, "no row in player_route is authoritative absence")
			assert.NotErrorIs(t, err, ErrRouteReadUnavailable)
			assert.Contains(t, err.Error(), "no home zone mapping for player 8403", "the NotFound text is a login contract")

			assert.Equal(t, []uint64{8401, 8402, 8403}, reader.homeFenceCalls)
		})
	}
}

// TestReadyHomeFence_SQLFailureIsUnavailableNotNotFound:不可信 + 库故障 → ErrRouteReadUnavailable(Unavailable),
// 绝不包 ErrHomeZoneNotMapped;回源读者没装配同样 fail-closed。
func TestReadyHomeFence_SQLFailureIsUnavailableNotNotFound(t *testing.T) {
	reader := &readFakeReader{err: errReadBoom}
	r, mr, _ := newReadGateRouter(t, true, reader)
	mr.Del(placement.RouteReadyKey)
	_, _, err := r.GetPlayerHomeZoneAndMergeFence(context.Background(), 8501)
	require.ErrorIs(t, err, ErrRouteReadUnavailable)
	require.ErrorIs(t, err, errReadBoom)
	assert.NotErrorIs(t, err, ErrHomeZoneNotMapped)

	r, mr, _ = newReadGateRouter(t, true, nil)
	mr.Del(placement.RouteReadyKey)
	_, _, err = r.GetPlayerHomeZoneAndMergeFence(context.Background(), 8501)
	require.ErrorIs(t, err, ErrRouteReadUnavailable, "no route reader assembled must fail closed")
	assert.NotErrorIs(t, err, ErrHomeZoneNotMapped)

	// 可信时没有回源,读者缺席也不影响。
	require.NoError(t, mr.Set(placement.RouteReadyKey, readyMarkerFor(t, readyRunID, readyReplID)))
	require.NoError(t, mr.Set(mappingKey(8501), "4"))
	zone, _, err := r.GetPlayerHomeZoneAndMergeFence(context.Background(), 8501)
	require.NoError(t, err)
	assert.Equal(t, uint32(4), zone)
}

// TestReadyHome_TrustedAndFallback:R6(GetPlayerHomeZone,ClientForPlayer 也走它)同 R2 去掉围栏:可信用缓存;
// 不可信按单 id 的 GetRoutes 回源;库无行 / home 为 NULL → NotFound;库故障 → ErrRouteReadUnavailable。
func TestReadyHome_TrustedAndFallback(t *testing.T) {
	reader := &readFakeReader{rows: map[uint64]store.RouteRow{
		8601: {PlayerID: 8601, HomeZone: 9, Rev: 2},
		8602: {PlayerID: 8602, StorageID: placement.DefaultGlobalStorageID, PlacementVersion: 1, Rev: 1},
	}}
	r, mr, fake := newReadGateRouter(t, true, reader)
	ctx := context.Background()
	require.NoError(t, mr.Set(mappingKey(8601), "5"))

	zone, err := r.GetPlayerHomeZone(ctx, 8601)
	require.NoError(t, err)
	assert.Equal(t, uint32(5), zone, "trusted: cache")
	assert.Equal(t, placement.RouteReadyReadScript, fake.scripts[0])
	assert.Equal(t, []string{placement.RouteReadyKey, mappingKey(8601)}, fake.calls[0])
	_, err = r.GetPlayerHomeZone(ctx, 8602)
	require.ErrorIs(t, err, ErrHomeZoneNotMapped, "trusted: a missing key is authoritative")
	assert.Empty(t, reader.getRoutesCalls)

	fake.role = "slave"
	zone, err = r.GetPlayerHomeZone(ctx, 8601)
	require.NoError(t, err)
	assert.Equal(t, uint32(9), zone, "untrusted: player_route")
	_, err = r.GetPlayerHomeZone(ctx, 8602)
	require.ErrorIs(t, err, ErrHomeZoneNotMapped, "a row with only a placement has no home")
	_, err = r.GetPlayerHomeZone(ctx, 8603)
	require.ErrorIs(t, err, ErrHomeZoneNotMapped)
	assert.Equal(t, [][]uint64{{8601}, {8602}, {8603}}, reader.getRoutesCalls)

	reader.err = errReadBoom
	_, err = r.GetPlayerHomeZone(ctx, 8601)
	require.ErrorIs(t, err, ErrRouteReadUnavailable)
	assert.NotErrorIs(t, err, ErrHomeZoneNotMapped)
}

// TestReadyBatch_ChunksTrustedAndUntrusted:R3 按 store.RouteBatchLimit 分块,每块一段脚本(KEYS = ready + 至多 1000 个);
// 可信的块用缓存,不可信的块整块回源(GetRoutes 的 IN 也是至多 1000 个),结果合并。
func TestReadyBatch_ChunksTrustedAndUntrusted(t *testing.T) {
	const n = 2*store.RouteBatchLimit + 500
	ids := make([]uint64, n)
	rows := map[uint64]store.RouteRow{}
	for i := range ids {
		ids[i] = uint64(900001 + i)
		if i%3 == 0 {
			rows[ids[i]] = store.RouteRow{PlayerID: ids[i], HomeZone: 9, Rev: 1}
		}
	}
	reader := &readFakeReader{rows: rows}
	r, mr, fake := newReadGateRouter(t, true, reader)
	ctx := context.Background()
	for i, id := range ids {
		if i%3 == 0 {
			require.NoError(t, mr.Set(mappingKey(id), "5"))
		}
	}

	// 第 2 块不可信:它那 1000 个 id 走库(zone 9),另外两块走缓存(zone 5)。
	fake.mismatchOnCall = 2
	got, err := r.BatchGetPlayerHomeZone(ctx, ids)
	require.NoError(t, err)
	require.Len(t, fake.calls, 3)
	assert.Len(t, fake.calls[0], 1+store.RouteBatchLimit)
	assert.Len(t, fake.calls[1], 1+store.RouteBatchLimit)
	assert.Len(t, fake.calls[2], 1+500)
	for _, keys := range fake.calls {
		assert.Equal(t, placement.RouteReadyKey, keys[0])
	}
	require.Len(t, reader.getRoutesCalls, 1)
	assert.Equal(t, ids[store.RouteBatchLimit:2*store.RouteBatchLimit], reader.getRoutesCalls[0])
	for i, id := range ids {
		want, present := uint32(0), i%3 == 0
		if present {
			want = 5
			if i >= store.RouteBatchLimit && i < 2*store.RouteBatchLimit {
				want = 9
			}
		}
		zone, ok := got[id]
		require.Equal(t, present, ok, "id %d", id)
		require.Equal(t, want, zone, "id %d", id)
	}
}

// TestReadyBatch_AnyChunkFailureFailsWholeBatch:不可信时任一块回源失败整批报错、不回半份结果;可信块里的畸形值同今天整批报错。
func TestReadyBatch_AnyChunkFailureFailsWholeBatch(t *testing.T) {
	ids := make([]uint64, store.RouteBatchLimit+10)
	for i := range ids {
		ids[i] = uint64(910001 + i)
	}
	reader := &readFakeReader{failGetRoutesOn: 2}
	r, mr, _ := newReadGateRouter(t, true, reader)
	mr.Del(placement.RouteReadyKey)
	got, err := r.BatchGetPlayerHomeZone(context.Background(), ids)
	require.ErrorIs(t, err, ErrRouteReadUnavailable)
	assert.Nil(t, got, "no partial result")
	assert.Len(t, reader.getRoutesCalls, 2)

	r, mr, _ = newReadGateRouter(t, true, nil)
	mr.Del(placement.RouteReadyKey)
	_, err = r.BatchGetPlayerHomeZone(context.Background(), ids[:3])
	require.ErrorIs(t, err, ErrRouteReadUnavailable, "no route reader assembled must fail closed")

	r, mr, _ = newReadGateRouter(t, true, &readFakeReader{})
	require.NoError(t, mr.Set(mappingKey(ids[1]), "not-a-zone"))
	got, err = r.BatchGetPlayerHomeZone(context.Background(), ids[:3])
	require.Error(t, err)
	assert.Nil(t, got)
	assert.NotErrorIs(t, err, ErrRouteReadUnavailable, "a malformed trusted value is a cache fault, not a fallback failure")
	assert.Contains(t, err.Error(), "invalid zone mapping")
}

// TestReadyBatch_EmptyIssuesNothing:空列表回空 map,不跑脚本、不碰库。
func TestReadyBatch_EmptyIssuesNothing(t *testing.T) {
	reader := &readFakeReader{}
	r, _, fake := newReadGateRouter(t, true, reader)
	got, err := r.BatchGetPlayerHomeZone(context.Background(), nil)
	require.NoError(t, err)
	assert.Empty(t, got)
	assert.NotNil(t, got)
	assert.Empty(t, fake.calls)
	assert.Empty(t, reader.getRoutesCalls)
}

// TestDecodeReadyHomeFence:就绪读回的 [home, fence] 与 decodeHomeZoneAndMergeFenceReply 同一口径,含「围栏读失败 → 封锁」。
func TestDecodeReadyHomeFence(t *testing.T) {
	for _, tc := range []struct {
		name        string
		vals        []any
		zone        uint32
		merging     bool
		notMapped   bool
		wantFailure bool
	}{
		{name: "absent", vals: []any{nil, nil}, notMapped: true},
		{name: "empty string is absent", vals: []any{"", nil}, notMapped: true},
		{name: "not fenced", vals: []any{"12", "0"}, zone: 12},
		{name: "fenced", vals: []any{"12", "1"}, zone: 12, merging: true},
		{name: "fence unreadable", vals: []any{"12", "-1"}, zone: 12, merging: true},
		{name: "fence missing", vals: []any{"12", nil}, zone: 12, merging: true},
		{name: "fence garbage", vals: []any{"12", "yes"}, zone: 12, merging: true},
		{name: "malformed home", vals: []any{"abc", "0"}, wantFailure: true},
		{name: "wrong length", vals: []any{"12"}, wantFailure: true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			zone, merging, err := decodeReadyHomeFence(1, tc.vals)
			switch {
			case tc.notMapped:
				require.ErrorIs(t, err, ErrHomeZoneNotMapped)
			case tc.wantFailure:
				require.Error(t, err)
				assert.NotErrorIs(t, err, ErrHomeZoneNotMapped)
			default:
				require.NoError(t, err)
				assert.Equal(t, tc.zone, zone)
				assert.Equal(t, tc.merging, merging)
			}
		})
	}
}

// TestAllPlayerIDsInZone_DBFirstReadsDB:db_first 下 R7 一律读库(开关关着也一样),沿键集游标翻页直到一页不足;
// 读库失败、游标不前进、单页超过上限、回源读者没装配都整体报错(包 ErrRouteReadUnavailable),不回半份名单。
func TestAllPlayerIDsInZone_DBFirstReadsDB(t *testing.T) {
	fullPage := func(base uint64) []store.RouteRow {
		page := make([]store.RouteRow, store.RouteBatchLimit)
		for i := range page {
			// 同一页内按聚簇键升序:行片 = id % 64,这里取 id = base + i*64,片恒为 base % 64、id 递增。
			page[i] = store.RouteRow{PlayerID: base + uint64(i)*64, HomeZone: 7, Rev: 1}
		}
		return page
	}
	page1 := fullPage(1)
	page2 := []store.RouteRow{{PlayerID: 2, HomeZone: 7, Rev: 1}, {PlayerID: 66, HomeZone: 7, Rev: 1}}
	reader := &readFakeReader{pages: [][]store.RouteRow{page1, page2}}
	r, mr, fake := newReadGateRouter(t, false, reader)
	require.NoError(t, mr.Set(mappingKey(424242), "7"), "a cache-only player must not be listed: the DB is the source of truth")

	ids, err := r.GetAllPlayerIDsInZone(context.Background(), 7)
	require.NoError(t, err)
	require.Len(t, ids, store.RouteBatchLimit+2)
	assert.Equal(t, uint64(2), ids[store.RouteBatchLimit])
	assert.NotContains(t, ids, uint64(424242))
	require.Len(t, reader.scanCalls, 2)
	assert.Equal(t, store.RouteScanCursor{}, reader.scanCalls[0], "the first page starts at the zero cursor")
	assert.Equal(t, store.RouteScanCursorAt(page1[len(page1)-1]), reader.scanCalls[1], "the next page starts after the last row")
	assert.Equal(t, []int{store.RouteBatchLimit, store.RouteBatchLimit}, reader.scanLimits)
	assert.Empty(t, fake.calls, "R7 does not run the ready read script")

	for _, tc := range []struct {
		name   string
		reader *readFakeReader
	}{
		{"read error", &readFakeReader{err: errReadBoom}},
		{"cursor does not advance", &readFakeReader{pages: [][]store.RouteRow{page1, page1}}},
		{"oversized page", &readFakeReader{pages: [][]store.RouteRow{append(fullPage(1), store.RouteRow{PlayerID: 999999, HomeZone: 7})}}},
		{"no reader", nil},
	} {
		t.Run(tc.name, func(t *testing.T) {
			r, _, _ := newReadGateRouter(t, false, tc.reader)
			ids, err := r.GetAllPlayerIDsInZone(context.Background(), 7)
			require.ErrorIs(t, err, ErrRouteReadUnavailable)
			assert.Nil(t, ids)
		})
	}
}

// TestProbeRouteReadyRead_FailsWhenScriptCannotRun:启动探针在读脚本跑不起来时报错(调用方据此拒启)。
// miniredis 不支持 INFO server,正好是「INFO 在脚本里不可用」的情形;连不上的地址是另一种。
func TestProbeRouteReadyRead_FailsWhenScriptCannotRun(t *testing.T) {
	mr := miniredis.RunT(t)
	_, err := ProbeRouteReadyRead(context.Background(), redis.RedisConf{Host: mr.Addr(), Type: "node"})
	require.Error(t, err, "the ready read script calls INFO server, which miniredis rejects")

	addr := mr.Addr()
	mr.Close()
	_, err = ProbeRouteReadyRead(context.Background(), redis.RedisConf{Host: addr, Type: "node"})
	require.Error(t, err, "an unreachable mapping redis must fail the probe")
}

// TestReadyReadScriptFailsOnMiniredisFallsBackToSQL:不注入假执行器、直接在 miniredis 上跑真脚本(INFO 报错)——
// 证明「脚本执行出错 → 回源库」走的是生产的那条执行路径(mappingRedis.EvalCtx),不是假执行器的模拟。
func TestReadyReadScriptFailsOnMiniredisFallsBackToSQL(t *testing.T) {
	reader := &readFakeReader{homeFence: map[uint64]store.RouteHomeFence{8701: {HomeZone: 9}},
		rows: map[uint64]store.RouteRow{8701: {PlayerID: 8701, HomeZone: 9, Rev: 1}}}
	r, mr, _ := newReadGateRouter(t, true, reader)
	r.routeReadEval = r.mappingRedis.EvalCtx
	require.NoError(t, mr.Set(mappingKey(8701), "5"))

	zone, _, err := r.GetPlayerHomeZoneAndMergeFence(context.Background(), 8701)
	require.NoError(t, err)
	assert.Equal(t, uint32(9), zone)
	zone, err = r.GetPlayerHomeZone(context.Background(), 8701)
	require.NoError(t, err)
	assert.Equal(t, uint32(9), zone)
	assert.Equal(t, []uint64{8701}, reader.homeFenceCalls)
	assert.Equal(t, [][]uint64{{8701}}, reader.getRoutesCalls)
}
