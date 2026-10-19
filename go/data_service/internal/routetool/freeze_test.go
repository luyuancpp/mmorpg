package routetool

// freeze / unfreeze(第 1b-1 批)与就绪标记脚本的执行语义:miniredis + 假库,不连库。
// 解冻事务本身(先抬水位再翻 writable、锁序)由 store 包的契约用例与真库用例钉住;这里只钉
// 「标记怎么判、什么时候拒绝、交给库的最低水位是多少、退出码是几」。

import (
	"bytes"
	"context"
	"errors"
	"strings"
	"testing"
	"time"

	"data_service/internal/store"

	"shared/placement"

	"github.com/alicebob/miniredis/v2"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// 编译期断言:装配层直接把这两样传进来。
var (
	_ readyMarkerRedis = (*redis.Redis)(nil)
	_ freezeDB         = (*store.RouteStore)(nil)
	_ unfreezeDB       = (*store.RouteStore)(nil)
)

const testGeneration = "0123456789abcdef0123456789abcdef"

// testIdentity 是「当前」mapping Redis 的身份(Identity 由用例注入,miniredis 的 INFO 不支持 server / replication 段)。
var testIdentity = RedisIdentity{RunID: strings.Repeat("ab", 20), MasterReplID: strings.Repeat("cd", 20)}

// fakeUnfreezeDB 是 unfreezeDB / freezeDB 的内存替身。
type fakeUnfreezeDB struct {
	meta          store.RouteMeta
	unfreezeCalls []uint64
	freezeCalls   int
	freezeChanged bool
	freezeErr     error
	deadline      bool // 解冻 / 冻结调用时 ctx 带不带截止时间
}

func (f *fakeUnfreezeDB) ReadRouteMeta(context.Context) (store.RouteMeta, error) { return f.meta, nil }

func (f *fakeUnfreezeDB) UnfreezeRouteWriters(ctx context.Context, minWatermarkMs uint64) (bool, error) {
	_, f.deadline = ctx.Deadline()
	f.unfreezeCalls = append(f.unfreezeCalls, minWatermarkMs)
	f.meta.Writable = 1
	return true, nil
}

func (f *fakeUnfreezeDB) FreezeRouteWriters(ctx context.Context) (bool, error) {
	_, f.deadline = ctx.Deadline()
	f.freezeCalls++
	return f.freezeChanged, f.freezeErr
}

func frozenUnfreezeDB() *fakeUnfreezeDB {
	return &fakeUnfreezeDB{meta: store.RouteMeta{Generation: testGeneration, Writable: 0}}
}

// readyMarkerFor 编出一份身份等于 id、generation 等于 gen 的合规标记。
func readyMarkerFor(t *testing.T, id RedisIdentity, gen string, watermarkMs uint64) string {
	t.Helper()
	raw, err := placement.EncodeRouteReadyMarker(placement.RouteReadyMarker{
		Generation:   gen,
		RedisRunID:   id.RunID,
		MasterReplID: id.MasterReplID,
		SetAtMs:      watermarkMs + 1,
		WatermarkMs:  watermarkMs,
		Leader:       "route_leader@it:0a1b2c3d",
	})
	require.NoError(t, err)
	return raw
}

func runUnfreeze(t *testing.T, rd readyMarkerRedis, db unfreezeDB, ack bool) (UnfreezeReport, cliRun) {
	t.Helper()
	var out, errOut bytes.Buffer
	rep, err := Unfreeze(context.Background(), rd, db, UnfreezeOptions{
		CacheUnverifiedAcknowledged: ack,
		Timeout:                     store.RouteFreezeTimeoutMin,
		Identity:                    func(context.Context) (RedisIdentity, error) { return testIdentity, nil },
		Stdout:                      &out,
		Stderr:                      &errOut,
	})
	return rep, cliRun{code: UnfreezeExitCode(rep, err), stdout: out.String(), stderr: errOut.String()}
}

// TestUnfreeze_VerifiedMarkerUnfreezesWithItsWatermark:标记可解码、身份等于当前实例 → 解冻,最低水位取标记的 watermark_ms;
// 标记原样保留;解冻调用带截止时间。
func TestUnfreeze_VerifiedMarkerUnfreezesWithItsWatermark(t *testing.T) {
	mr, rd := newMappingRedis(t)
	raw := readyMarkerFor(t, testIdentity, testGeneration, 1_760_000_000_000)
	require.NoError(t, mr.Set(placement.RouteReadyKey, raw))
	db := frozenUnfreezeDB()

	rep, run := runUnfreeze(t, rd, db, false)

	assert.Equal(t, ExitClean, run.code, run.stderr)
	assert.Equal(t, []uint64{1_760_000_000_000}, db.unfreezeCalls)
	assert.True(t, db.deadline, "解冻事务必须带时限(store.RouteFreezeTimeoutMin)")
	assert.Equal(t, MarkerVerified, rep.Marker)
	assert.False(t, rep.GenerationRealigned)
	got, err := mr.Get(placement.RouteReadyKey)
	require.NoError(t, err)
	assert.Equal(t, raw, got, "可信标记不动")
	assert.Equal(t, "route_unfreeze changed=true already_writable=false marker=verified generation_realigned=false min_watermark_ms=1760000000000\n", run.stdout)
}

// TestUnfreeze_GenerationRealignedBeforeUnfreeze:可信标记的 generation 与库不等 → 先 compare-and-set 成库的 generation 再解冻;
// CAS 输了(读到之后被别人改过)→ 退出码 1、不解冻。
func TestUnfreeze_GenerationRealignedBeforeUnfreeze(t *testing.T) {
	mr, rd := newMappingRedis(t)
	require.NoError(t, mr.Set(placement.RouteReadyKey, readyMarkerFor(t, testIdentity, "ffff", 42)))
	db := frozenUnfreezeDB()

	rep, run := runUnfreeze(t, rd, db, false)

	assert.Equal(t, ExitClean, run.code, run.stderr)
	assert.True(t, rep.GenerationRealigned)
	got, err := mr.Get(placement.RouteReadyKey)
	require.NoError(t, err)
	m, err := placement.DecodeRouteReadyMarker(got)
	require.NoError(t, err)
	assert.Equal(t, testGeneration, m.Generation, "标记的 generation 对齐成库的")
	assert.Equal(t, uint64(42), m.WatermarkMs, "其余字段不变")
	assert.Equal(t, []uint64{42}, db.unfreezeCalls)

	t.Run("CAS输了_退出码1_不解冻", func(t *testing.T) {
		mr, rd := newMappingRedis(t)
		require.NoError(t, mr.Set(placement.RouteReadyKey, readyMarkerFor(t, testIdentity, "ffff", 42)))
		db := frozenUnfreezeDB()
		raced := casRacingRedis{readyMarkerRedis: rd, mr: mr}

		_, run := runUnfreeze(t, raced, db, false)

		assert.Equal(t, ExitAttention, run.code, run.stderr)
		assert.Empty(t, db.unfreezeCalls)
		assert.Empty(t, run.stdout)
	})
}

// casRacingRedis 在每次 EVAL 之前把标记换成另一份合规值,模拟「读到之后被别人改过」。
type casRacingRedis struct {
	readyMarkerRedis
	mr *miniredis.Miniredis
}

func (c casRacingRedis) EvalCtx(ctx context.Context, script string, keys []string, args ...any) (any, error) {
	if err := c.mr.Set(placement.RouteReadyKey, `{"v":1,"changed":"by-someone-else"}`); err != nil {
		return nil, err
	}
	return c.readyMarkerRedis.EvalCtx(ctx, script, keys, args...)
}

// TestUnfreeze_RefusesWithoutVerifiedMarker:确认不了缓存(标记缺席 / 畸形 / 版本更新 / 不是 string / 身份不符)一律退出码 2、不解冻;
// 畸形的按原值撤掉,版本更新的与不是 string 的不动,身份不符的(没加确认时)不动。
func TestUnfreeze_RefusesWithoutVerifiedMarker(t *testing.T) {
	other := RedisIdentity{RunID: strings.Repeat("01", 20), MasterReplID: testIdentity.MasterReplID}
	for _, tc := range []struct {
		name      string
		preset    func(t *testing.T, mr *miniredis.Miniredis)
		wantState string
		wantKept  bool // 标记键是否还在
	}{
		{"缺席", func(*testing.T, *miniredis.Miniredis) {}, MarkerAbsent, false},
		{"畸形_撤掉", func(t *testing.T, mr *miniredis.Miniredis) {
			require.NoError(t, mr.Set(placement.RouteReadyKey, "ready"))
		}, MarkerMalformedDropped, false},
		{"版本更新_不动", func(t *testing.T, mr *miniredis.Miniredis) {
			require.NoError(t, mr.Set(placement.RouteReadyKey, `{"v":2,"whatever":true}`))
		}, MarkerNewerVersion, true},
		// 2026-10-18 修复评审:新版本改了已知字段的类型也只认「版本更新」,不能当畸形删掉。
		{"版本更新且字段改了类型_不动", func(t *testing.T, mr *miniredis.Miniredis) {
			require.NoError(t, mr.Set(placement.RouteReadyKey, `{"v":2,"set_at_ms":"x"}`))
		}, MarkerNewerVersion, true},
		{"不是string_不动", func(t *testing.T, mr *miniredis.Miniredis) {
			_, err := mr.SAdd(placement.RouteReadyKey, "x")
			require.NoError(t, err)
		}, MarkerWrongType, true},
		{"身份不符_不动", func(t *testing.T, mr *miniredis.Miniredis) {
			require.NoError(t, mr.Set(placement.RouteReadyKey, readyMarkerFor(t, other, testGeneration, 42)))
		}, MarkerIdentityMismatch, true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			mr, rd := newMappingRedis(t)
			tc.preset(t, mr)
			db := frozenUnfreezeDB()

			rep, run := runUnfreeze(t, rd, db, false)

			assert.Equal(t, ExitFailed, run.code, run.stderr)
			assert.Equal(t, tc.wantState, rep.Marker)
			assert.Empty(t, db.unfreezeCalls, "确认不了缓存就不许解冻")
			assert.Empty(t, run.stdout)
			assert.Contains(t, run.stderr, "§9.2")
			assert.Contains(t, run.stderr, "-cache-was-not-verified-acknowledged")
			assert.Equal(t, tc.wantKept, mr.Exists(placement.RouteReadyKey))
		})
	}
}

// TestUnfreeze_AcknowledgedProceedsWithZeroWatermark:显式确认之后照常解冻,最低水位 0(没有可信的「缓存截至」时刻);
// 身份不符的标记按原值撤掉(读方只看「存在」,不可信的标记不能活过解冻);版本更新的标记仍不动。
func TestUnfreeze_AcknowledgedProceedsWithZeroWatermark(t *testing.T) {
	t.Run("缺席", func(t *testing.T) {
		_, rd := newMappingRedis(t)
		db := frozenUnfreezeDB()
		rep, run := runUnfreeze(t, rd, db, true)
		assert.Equal(t, ExitClean, run.code, run.stderr)
		assert.Equal(t, []uint64{0}, db.unfreezeCalls)
		assert.Equal(t, MarkerAbsent, rep.Marker)
		assert.Contains(t, run.stderr, "WARN: ")
	})
	t.Run("身份不符_撤掉", func(t *testing.T) {
		mr, rd := newMappingRedis(t)
		other := RedisIdentity{RunID: testIdentity.RunID, MasterReplID: strings.Repeat("ef", 20)}
		require.NoError(t, mr.Set(placement.RouteReadyKey, readyMarkerFor(t, other, testGeneration, 42)))
		db := frozenUnfreezeDB()
		_, run := runUnfreeze(t, rd, db, true)
		assert.Equal(t, ExitClean, run.code, run.stderr)
		assert.Equal(t, []uint64{0}, db.unfreezeCalls, "不可信标记的水位不用")
		assert.False(t, mr.Exists(placement.RouteReadyKey))
	})
	t.Run("版本更新_仍不动", func(t *testing.T) {
		mr, rd := newMappingRedis(t)
		require.NoError(t, mr.Set(placement.RouteReadyKey, `{"v":2}`))
		db := frozenUnfreezeDB()
		_, run := runUnfreeze(t, rd, db, true)
		assert.Equal(t, ExitClean, run.code, run.stderr)
		assert.True(t, mr.Exists(placement.RouteReadyKey))
	})
}

// TestUnfreeze_AlreadyWritableIsNoop:开始时已经可写 → 退出码 0、不读标记、不调解冻。
func TestUnfreeze_AlreadyWritableIsNoop(t *testing.T) {
	_, rd := newMappingRedis(t)
	db := frozenUnfreezeDB()
	db.meta.Writable = 1

	rep, run := runUnfreeze(t, rd, db, false)

	assert.Equal(t, ExitClean, run.code, run.stderr)
	assert.True(t, rep.AlreadyWritable)
	assert.Empty(t, db.unfreezeCalls)
	assert.Contains(t, run.stdout, "already_writable=true")
}

// TestUnfreeze_RedisModeNeedsAckAndSkipsMarker(2026-10-18 修复评审):redis 模式下 unfreeze 是撤销冻结的唯一工具路径,
// 不读标记(不给 mapping Redis 与身份读取也能跑)、最低水位 0;没有显式确认就拒绝(退出码 2),连库都不读。
func TestUnfreeze_RedisModeNeedsAckAndSkipsMarker(t *testing.T) {
	run := func(db unfreezeDB, ack bool) (UnfreezeReport, cliRun, error) {
		var out, errOut bytes.Buffer
		rep, err := Unfreeze(context.Background(), nil, db, UnfreezeOptions{
			CacheUnverifiedAcknowledged: ack,
			RedisMode:                   true,
			Timeout:                     store.RouteFreezeTimeoutMin,
			Stdout:                      &out,
			Stderr:                      &errOut,
		})
		return rep, cliRun{code: UnfreezeExitCode(rep, err), stdout: out.String(), stderr: errOut.String()}, err
	}

	db := frozenUnfreezeDB()
	_, r, err := run(db, false)
	assert.Equal(t, ExitFailed, r.code)
	assert.ErrorIs(t, err, ErrCacheNotVerified)
	assert.Empty(t, db.unfreezeCalls)
	assert.Empty(t, r.stdout)

	db = frozenUnfreezeDB()
	rep, r, err := run(db, true)
	require.NoError(t, err)
	assert.Equal(t, ExitClean, r.code, r.stderr)
	assert.Equal(t, []uint64{0}, db.unfreezeCalls, "redis 模式没有可信的「缓存截至」时刻")
	assert.Equal(t, MarkerNotReadRedisMode, rep.Marker)
	assert.True(t, db.deadline, "解冻写带时限")
	assert.Contains(t, r.stderr, "WARN: RouteStore.Mode=redis")
	assert.Equal(t, "route_unfreeze changed=true already_writable=false marker=not_read_redis_mode generation_realigned=false min_watermark_ms=0\n", r.stdout)

	_, err = Unfreeze(context.Background(), nil, frozenUnfreezeDB(), UnfreezeOptions{
		Timeout: store.RouteFreezeTimeoutMin, Stdout: &bytes.Buffer{}, Stderr: &bytes.Buffer{},
	})
	require.Error(t, err, "db_first 下没有 mapping Redis 与身份读取不许跑")
}

// TestFreeze_OutputAndRedisModeWarning:freeze 只碰库,带时限调用;stdout 一行统计;redis 模式打 WARN;
// 时限低于下限直接拒绝(不调库);库出错退出码 2、不出统计行。
func TestFreeze_OutputAndRedisModeWarning(t *testing.T) {
	run := func(db *fakeUnfreezeDB, redisMode bool, timeout time.Duration) cliRun {
		var out, errOut bytes.Buffer
		rep, err := Freeze(context.Background(), db, FreezeOptions{
			Reason: "PITR 2026-10-18", Timeout: timeout, RedisMode: redisMode, Stdout: &out, Stderr: &errOut,
		})
		return cliRun{code: FreezeExitCode(rep, err), stdout: out.String(), stderr: errOut.String()}
	}

	db := &fakeUnfreezeDB{freezeChanged: true}
	r := run(db, false, store.RouteFreezeTimeoutMin)
	assert.Equal(t, ExitClean, r.code, r.stderr)
	assert.Equal(t, "route_freeze changed=true writable=0\n", r.stdout)
	assert.Contains(t, r.stderr, `FREEZE reason="PITR 2026-10-18"`)
	assert.NotContains(t, r.stderr, "Mode=redis")
	assert.True(t, db.deadline)

	r = run(&fakeUnfreezeDB{}, true, store.RouteFreezeTimeoutMin)
	assert.Equal(t, ExitClean, r.code)
	assert.Equal(t, "route_freeze changed=false writable=0\n", r.stdout)
	assert.Contains(t, r.stderr, "WARN: RouteStore.Mode=redis")

	db = &fakeUnfreezeDB{}
	r = run(db, false, store.RouteFreezeTimeoutMin-time.Millisecond)
	assert.Equal(t, ExitFailed, r.code)
	assert.Zero(t, db.freezeCalls, "时限不够就不调库")

	r = run(&fakeUnfreezeDB{freezeErr: errors.New("Error 1205")}, false, store.RouteFreezeTimeoutMin)
	assert.Equal(t, ExitFailed, r.code)
	assert.Empty(t, r.stdout)
}

// TestParseRedisIdentity:按捕获的 Redis 7.2 INFO 文本(CRLF)取 run_id 与 master_replid;LF 也认;缺字段、长度不对、
// 大写一律报错(身份读不准时宁可确认不了)。
func TestParseRedisIdentity(t *testing.T) {
	server := "# Server\r\nredis_version:7.2.4\r\nredis_mode:standalone\r\nrun_id:" + testIdentity.RunID + "\r\ntcp_port:6379\r\n"
	replication := "# Replication\r\nrole:master\r\nconnected_slaves:0\r\nmaster_replid:" + testIdentity.MasterReplID +
		"\r\nmaster_replid2:0000000000000000000000000000000000000000\r\n"

	id, err := ParseRedisIdentity(server, replication)
	require.NoError(t, err)
	assert.Equal(t, testIdentity, id)

	id, err = ParseRedisIdentity(strings.ReplaceAll(server, "\r\n", "\n"), strings.ReplaceAll(replication, "\r\n", "\n"))
	require.NoError(t, err)
	assert.Equal(t, testIdentity, id, "LF 行尾也认")

	for _, tc := range []struct{ name, server, replication string }{
		{"缺run_id", "# Server\r\nredis_version:7.2.4\r\n", replication},
		{"缺master_replid", server, "# Replication\r\nrole:master\r\n"},
		{"run_id长度不对", strings.Replace(server, testIdentity.RunID, testIdentity.RunID[:39], 1), replication},
		{"run_id大写", strings.Replace(server, testIdentity.RunID, strings.ToUpper(testIdentity.RunID), 1), replication},
	} {
		t.Run(tc.name, func(t *testing.T) {
			_, err := ParseRedisIdentity(tc.server, tc.replication)
			assert.Error(t, err)
		})
	}
}

// TestReadyMarkerScriptsOnMiniredis:placement 的两个就绪标记脚本的执行语义 —— 当前值等于原值才改 / 删、回 1;
// 不等(含键不在)回 0、零写入。
func TestReadyMarkerScriptsOnMiniredis(t *testing.T) {
	mr, rd := newMappingRedis(t)
	ctx := context.Background()
	keys := []string{placement.RouteReadyKey}

	got, err := rd.EvalCtx(ctx, placement.RouteReadyCompareAndSetScript, keys, "a", "b")
	require.NoError(t, err)
	assert.Equal(t, int64(0), got, "键不在:CAS 落空")
	assert.False(t, mr.Exists(placement.RouteReadyKey))

	require.NoError(t, mr.Set(placement.RouteReadyKey, "a"))
	got, err = rd.EvalCtx(ctx, placement.RouteReadyCompareAndSetScript, keys, "x", "b")
	require.NoError(t, err)
	assert.Equal(t, int64(0), got, "原值不等:CAS 落空")
	got, err = rd.EvalCtx(ctx, placement.RouteReadyCompareAndSetScript, keys, "a", "b")
	require.NoError(t, err)
	assert.Equal(t, int64(1), got)
	v, _ := mr.Get(placement.RouteReadyKey)
	assert.Equal(t, "b", v)
	assert.Zero(t, mr.TTL(placement.RouteReadyKey), "CAS 写的值不带过期")

	got, err = rd.EvalCtx(ctx, placement.RouteReadyCompareAndDeleteScript, keys, "a")
	require.NoError(t, err)
	assert.Equal(t, int64(0), got, "原值不等:CAD 落空")
	assert.True(t, mr.Exists(placement.RouteReadyKey))
	got, err = rd.EvalCtx(ctx, placement.RouteReadyCompareAndDeleteScript, keys, "b")
	require.NoError(t, err)
	assert.Equal(t, int64(1), got)
	assert.False(t, mr.Exists(placement.RouteReadyKey))
}
