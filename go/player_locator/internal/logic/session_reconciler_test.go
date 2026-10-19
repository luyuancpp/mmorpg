package logic

import (
	"context"
	"errors"
	"fmt"
	"math/rand"
	"strconv"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/config"
	"player_locator/internal/svc"
	pb "proto/player_locator"
	"shared/leader"
)

// 会话对账(索引循环、补建 / 审计、就绪标记、选主让位)的行为测试。
// 约定:miniredis + 注入的固定时钟 + 不限速的 pacer + 受控的拍,不读墙钟;并发经 go-redis Hook 在指定命令
// 之后同步注入,结果确定。修复前(旧实现)这些用例依赖的函数不存在,编译即失败。

const testReconcileInterval = 60 * time.Second

// fakeClusterView 是可在测试中途切换返回值的 clusterView;并发安全(run 的用例在另一个 goroutine 里读它)。
type fakeClusterView struct {
	mu         sync.Mutex
	live       map[string]struct{}
	liveErr    error
	all        map[string]struct{}
	capable    map[string]struct{}
	writersErr error
	liveCalls  int
}

func newFakeClusterView(liveGates ...string) *fakeClusterView {
	f := &fakeClusterView{}
	f.setLive(liveGates...)
	f.setWriters([]string{"locator-1"}, []string{"locator-1"})
	return f
}

func toSet(items []string) map[string]struct{} {
	set := make(map[string]struct{}, len(items))
	for _, item := range items {
		set[item] = struct{}{}
	}
	return set
}

func (f *fakeClusterView) setLive(gates ...string) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.live, f.liveErr = toSet(gates), nil
}

func (f *fakeClusterView) setLiveErr(err error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.liveErr = err
}

func (f *fakeClusterView) setWriters(all, capable []string) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.all, f.capable, f.writersErr = toSet(all), toSet(capable), nil
}

func (f *fakeClusterView) setWritersErr(err error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.writersErr = err
}

func (f *fakeClusterView) LiveGateInstances(context.Context) (map[string]struct{}, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.liveCalls++
	if f.liveErr != nil {
		return nil, f.liveErr
	}
	return f.live, nil
}

func (f *fakeClusterView) SessionWriters(context.Context) (map[string]struct{}, map[string]struct{}, error) {
	f.mu.Lock()
	defer f.mu.Unlock()
	if f.writersErr != nil {
		return nil, nil, f.writersErr
	}
	return f.all, f.capable, nil
}

func (f *fakeClusterView) liveCallCount() int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.liveCalls
}

// testClock 是注入的时钟;只有用例显式 Advance 时才走。
type testClock struct {
	mu  sync.Mutex
	now time.Time
}

func newTestClock() *testClock { return &testClock{now: time.Unix(1_800_000_000, 0)} }

func (c *testClock) Now() time.Time {
	c.mu.Lock()
	defer c.mu.Unlock()
	return c.now
}

func (c *testClock) Advance(d time.Duration) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.now = c.now.Add(d)
}

func testReconcileConf() config.ReconcileConf {
	return config.ReconcileConf{
		AuditIntervalSeconds:    86400,
		ScanCount:               1000,
		ScanPagesPerSecond:      -1,
		MaxTransitionsPerSecond: -1,
		DeadGateMembersPerRound: 30000,
	}
}

// testReconcilerDeps:不限速、拍立即到、补建循环的间隔与重试退避立即到、Redis run_id 恒定。用例按需替换。
// (miniredis 的 INFO 不支持 server 段,run_id 必须注入;解析逻辑由 TestParseRedisRunID 单测。)
func testReconcilerDeps(view clusterView, clock *testClock) reconcilerDeps {
	return reconcilerDeps{
		view:     view,
		now:      clock.Now,
		newPacer: func(int) pacer { return noopPacer{} },
		newTicker: func(time.Duration) (func(context.Context) error, func()) {
			return func(ctx context.Context) error { return ctx.Err() }, func() {}
		},
		sleep:      func(ctx context.Context, _ time.Duration) error { return ctx.Err() },
		redisRunID: func(context.Context, *redis.Client) (string, error) { return "test-run-id", nil },
	}
}

// injectedRedisError 模拟服务端回复的错误(实现 redis.Error,与 go-redis 解析出的 "-ERR ..." 回复同类)。
type injectedRedisError string

func (e injectedRedisError) Error() string { return string(e) }

func (injectedRedisError) RedisError() {}

// cmdArg 返回命令的第 i 个参数的字符串形式(越界为 "")。
func cmdArg(cmd redis.Cmder, i int) string {
	args := cmd.Args()
	if i >= len(args) {
		return ""
	}
	return fmt.Sprint(args[i])
}

// isScriptOn 判断 cmd 是不是 KEYS 恰为 keys 的 EVAL / EVALSHA(go-redis 的 Script.Run 先发 EVALSHA,NOSCRIPT 时再发 EVAL)。
func isScriptOn(cmd redis.Cmder, keys ...string) bool {
	if name := cmd.Name(); name != "evalsha" && name != "eval" {
		return false
	}
	if cmdArg(cmd, 2) != strconv.Itoa(len(keys)) {
		return false
	}
	for i, key := range keys {
		if cmdArg(cmd, 3+i) != key {
			return false
		}
	}
	return true
}

func newTestReconciler(t *testing.T, sc *svc.ServiceContext, view clusterView) *sessionReconciler {
	t.Helper()
	return newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, testReconcilerDeps(view, newTestClock()))
}

// blockUntilDone 是「等到 ctx 结束」的 sleep / tick:run 的用例用它让补建循环或索引循环停在等待处。
func blockUntilDone(ctx context.Context, _ time.Duration) error {
	<-ctx.Done()
	return ctx.Err()
}

// secondClient 是同一个 miniredis 上的另一个客户端,不挂 Hook:用来在 Hook 里注入并发写。
func secondClient(t *testing.T, sc *svc.ServiceContext, mr *miniredis.Miniredis) *svc.ServiceContext {
	t.Helper()
	rdb := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { rdb.Close() })
	return &svc.ServiceContext{Config: sc.Config, RedisClient: rdb}
}

// commandRecorder 计数客户端发出的命令(含 pipeline),after 在每条普通命令返回之后同步调用。
// fail 返回非 nil 时这条普通命令不发出,直接以该错误返回(模拟传输错误或服务端错误回复;仍计数)。
type commandRecorder struct {
	mu       sync.Mutex
	counts   map[string]int
	mgetKeys int
	scans    []string // 每次 SCAN 的游标参数,按发出顺序(含失败的)
	sscans   []string // 每次 SSCAN 的游标参数,按发出顺序(含失败的)
	before   func(ctx context.Context, cmd redis.Cmder)
	after    func(cmd redis.Cmder)
	fail     func(cmd redis.Cmder) error
}

func newCommandRecorder() *commandRecorder { return &commandRecorder{counts: map[string]int{}} }

func (h *commandRecorder) DialHook(next redis.DialHook) redis.DialHook { return next }

func (h *commandRecorder) ProcessHook(next redis.ProcessHook) redis.ProcessHook {
	return func(ctx context.Context, cmd redis.Cmder) error {
		if h.before != nil {
			h.before(ctx, cmd)
		}
		if h.fail != nil {
			if err := h.fail(cmd); err != nil {
				cmd.SetErr(err)
				h.record(cmd)
				return err
			}
		}
		err := next(ctx, cmd)
		h.record(cmd)
		if h.after != nil {
			h.after(cmd)
		}
		return err
	}
}

func (h *commandRecorder) ProcessPipelineHook(next redis.ProcessPipelineHook) redis.ProcessPipelineHook {
	return func(ctx context.Context, cmds []redis.Cmder) error {
		err := next(ctx, cmds)
		for _, cmd := range cmds {
			h.record(cmd)
		}
		return err
	}
}

func (h *commandRecorder) record(cmd redis.Cmder) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.counts[cmd.Name()]++
	switch cmd.Name() {
	case "mget":
		h.mgetKeys += len(cmd.Args()) - 1
	case "scan":
		h.scans = append(h.scans, cmdArg(cmd, 1))
	case "sscan":
		h.sscans = append(h.sscans, cmdArg(cmd, 2))
	}
}

func (h *commandRecorder) scanCursors() []string {
	h.mu.Lock()
	defer h.mu.Unlock()
	return append([]string(nil), h.scans...)
}

func (h *commandRecorder) sscanCursors() []string {
	h.mu.Lock()
	defer h.mu.Unlock()
	return append([]string(nil), h.sscans...)
}

func (h *commandRecorder) count(name string) int {
	h.mu.Lock()
	defer h.mu.Unlock()
	return h.counts[name]
}

func (h *commandRecorder) mgetKeyCount() int {
	h.mu.Lock()
	defer h.mu.Unlock()
	return h.mgetKeys
}

func mustIndexRoundOK(t *testing.T, r *sessionReconciler) indexRoundReport {
	t.Helper()
	out := r.indexRound(context.Background())
	require.NoError(t, out.err)
	return out.report
}

// writeRawSession 直接写会话字节、不维护索引:模拟不维护索引的旧版写者或外部篡改。
func writeRawSession(t *testing.T, mr *miniredis.Miniredis, s *pb.PlayerSession) []byte {
	t.Helper()
	data, err := proto.Marshal(s)
	require.NoError(t, err)
	require.NoError(t, mr.Set(sessionKey(s.PlayerId), string(data)))
	return data
}

func leaseMembers(t *testing.T, mr *miniredis.Miniredis) []string {
	t.Helper()
	if !mr.Exists(LeaseZSetKey) {
		return nil
	}
	members, err := mr.ZMembers(LeaseZSetKey)
	require.NoError(t, err)
	return members
}

func setReadyMarker(t *testing.T, mr *miniredis.Miniredis, ttl time.Duration) {
	t.Helper()
	require.NoError(t, mr.Set(gateIndexReadyKey, gateIndexReadyValuePrefix+"1"))
	mr.SetTTL(gateIndexReadyKey, ttl)
}

// ---------------------------------------------------------------------------
// 索引循环
// ---------------------------------------------------------------------------

func TestIndexRound_DeadGateNeedsTwoConsecutiveRounds(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9001, 100, idxGateA, 1)))
	before := readSessionBytes(t, sc, 9001)
	r := newTestReconciler(t, sc, newFakeClusterView()) // A 不在 etcd

	rep := mustIndexRoundOK(t, r)
	assert.Empty(t, rep.deadGates, "第一拍缺席只挂嫌疑")
	assert.Equal(t, before, readSessionBytes(t, sc, 9001))

	rep = mustIndexRoundOK(t, r)
	assert.Equal(t, []string{idxGateA}, rep.deadGates)
	after := readSession(t, sc, 9001)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, after.State)
	assert.Equal(t, uint32(2), after.SessionVersion)
	assert.Contains(t, leaseMembers(t, mr), "9001")
	assert.False(t, mr.Exists(gateSessionIndexKey(idxGateA)), "死 gate 处理完后索引键被删")
	assert.NotContains(t, registryMembers(t, mr), idxGateA, "并移出登记集")
	assert.Equal(t, 1, rep.actions[reconcileActionToDisconnecting])
}

func TestIndexRound_LiveGateUntouched(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9002, 100, idxGateA, 1)))
	before := readSessionBytes(t, sc, 9002)
	r := newTestReconciler(t, sc, newFakeClusterView(idxGateA))

	for i := 0; i < 4; i++ {
		rep := mustIndexRoundOK(t, r)
		assert.Equal(t, int64(1), rep.liveMembers)
	}
	assert.Equal(t, before, readSessionBytes(t, sc, 9002))
	assert.Equal(t, []string{"9002"}, indexMembers(t, mr, idxGateA))
	assert.Empty(t, leaseMembers(t, mr))
}

// 缺席、存活、缺席:必须**连续**两拍缺席才动手;去抖在存活那一拍清零。
func TestIndexRound_ReappearanceResetsSuspicion(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9003, 100, idxGateA, 1)))
	before := readSessionBytes(t, sc, 9003)
	view := newFakeClusterView()
	r := newTestReconciler(t, sc, view)

	mustIndexRoundOK(t, r) // 缺席
	view.setLive(idxGateA)
	mustIndexRoundOK(t, r) // 存活
	view.setLive()
	mustIndexRoundOK(t, r) // 缺席(重新计 1)
	assert.Equal(t, before, readSessionBytes(t, sc, 9003))

	mustIndexRoundOK(t, r) // 连续第二拍缺席
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9003).State)
}

// 本项的核心性质:对账不再扫全键空间,MGET 的只是死 gate 的成员。旧实现每拍必然 SCAN。
func TestIndexRound_DoesNotScanKeyspace(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	for i := 0; i < 5000; i++ {
		require.NoError(t, mr.Set("noise:"+strconv.Itoa(i), "x"))
	}
	for i := uint64(0); i < 10; i++ {
		require.NoError(t, callSetSession(sc, indexTestSession(9100+i, 100, idxGateA, 1)))
	}
	for i := uint64(0); i < 3; i++ {
		require.NoError(t, callSetSession(sc, indexTestSession(9200+i, 100, idxGateB, 1)))
	}
	rec := newCommandRecorder()
	sc.RedisClient.AddHook(rec)
	r := newTestReconciler(t, sc, newFakeClusterView(idxGateA)) // B 死了

	mustIndexRoundOK(t, r)
	rep := mustIndexRoundOK(t, r)

	assert.Equal(t, 0, rec.count("scan"), "索引循环不得 SCAN 全键空间")
	assert.Equal(t, 3, rec.mgetKeyCount(), "只 MGET 死 gate 上的 3 个成员")
	assert.Equal(t, 3, rep.actions[reconcileActionToDisconnecting])
	assert.Equal(t, int64(10), rep.liveMembers)
}

// 死 gate 索引里的三类脏成员(会话已在别的 gate 上 ONLINE、会话键不存在、成员不是 player_id)都被摘除,
// 别的 gate 上的会话一个字节都不动。
func TestIndexRound_PrunesStaleMembers(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9301, 100, idxGateB, 1)))
	beforeB := readSessionBytes(t, sc, 9301)
	_, err := mr.SetAdd(gateSessionIndexKey(idxGateA), "9301", "9302", "junk")
	require.NoError(t, err)
	_, err = mr.SetAdd(GateIndexRegistryKey, idxGateA)
	require.NoError(t, err)
	r := newTestReconciler(t, sc, newFakeClusterView(idxGateB))

	mustIndexRoundOK(t, r)
	rep := mustIndexRoundOK(t, r)

	assert.False(t, mr.Exists(gateSessionIndexKey(idxGateA)))
	assert.Equal(t, beforeB, readSessionBytes(t, sc, 9301))
	assert.Equal(t, []string{"9301"}, indexMembers(t, mr, idxGateB))
	assert.Equal(t, 1, rep.actions[reconcileActionPrunedStale])
	assert.Equal(t, 1, rep.actions[reconcileActionPrunedMissing])
	assert.Equal(t, 1, rep.actions[reconcileActionPrunedBadMember])
	assert.Empty(t, leaseMembers(t, mr))
}

// 会话字节解不开,或载荷里的 player_id 与成员不符:只摘成员,会话原样保留、不入租约。
func TestIndexRound_MalformedSessionPrunedNotModified(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	garbage := string([]byte{0xff, 0xff, 0xff})
	require.NoError(t, mr.Set(sessionKey(9401), garbage))
	wrongOwner, err := proto.Marshal(indexTestSession(9999, 100, idxGateA, 1))
	require.NoError(t, err)
	require.NoError(t, mr.Set(sessionKey(9402), string(wrongOwner)))
	_, err = mr.SetAdd(gateSessionIndexKey(idxGateA), "9401", "9402")
	require.NoError(t, err)
	_, err = mr.SetAdd(GateIndexRegistryKey, idxGateA)
	require.NoError(t, err)
	r := newTestReconciler(t, sc, newFakeClusterView())

	mustIndexRoundOK(t, r)
	rep := mustIndexRoundOK(t, r)

	assert.Equal(t, 2, rep.actions[reconcileActionPrunedMalformed])
	assert.Equal(t, 0, rep.actions[reconcileActionToDisconnecting])
	got1, _ := mr.Get(sessionKey(9401))
	got2, _ := mr.Get(sessionKey(9402))
	assert.Equal(t, garbage, got1)
	assert.Equal(t, string(wrongOwner), got2)
	assert.Empty(t, leaseMembers(t, mr))
}

// etcd 存活集读不出来:整拍不推进去抖、不转换;恢复后仍要连续两拍缺席才动手。
func TestIndexRound_EtcdErrorSkipsWithoutProgress(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9501, 100, idxGateA, 1)))
	before := readSessionBytes(t, sc, 9501)
	view := newFakeClusterView()
	view.setLiveErr(errors.New("etcd unavailable"))
	r := newTestReconciler(t, sc, view)

	for i := 0; i < 5; i++ {
		out := r.indexRound(context.Background())
		require.Error(t, out.err)
		assert.Equal(t, stageEtcd, out.stage)
	}
	assert.Equal(t, before, readSessionBytes(t, sc, 9501))

	view.setLive() // 恢复,A 缺席
	mustIndexRoundOK(t, r)
	assert.Equal(t, before, readSessionBytes(t, sc, 9501), "恢复后的第一拍只挂嫌疑")
	mustIndexRoundOK(t, r)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9501).State)
}

// 判定与提交之间玩家顶号换到 B:转换 CAS 失败成 no-op,会话保持在 B 上 ONLINE,成员在 B 不在 A,不入租约。
func TestIndexRound_ConcurrentReplaceDuringRoundIsNoop(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9601, 100, idxGateA, 1)))
	r := newTestReconciler(t, sc, newFakeClusterView(idxGateB))
	mustIndexRoundOK(t, r)

	other := secondClient(t, sc, mr)
	rec := newCommandRecorder()
	var once sync.Once
	rec.after = func(cmd redis.Cmder) {
		if cmd.Name() != "mget" {
			return
		}
		once.Do(func() {
			require.NoError(t, callSetSession(other, indexTestSession(9601, 200, idxGateB, 2)))
		})
	}
	sc.RedisClient.AddHook(rec)

	rep := mustIndexRoundOK(t, r)

	s := readSession(t, sc, 9601)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_ONLINE, s.State)
	assert.Equal(t, idxGateB, s.GateInstanceId)
	assert.Equal(t, []string{"9601"}, indexMembers(t, mr, idxGateB))
	assert.Empty(t, indexMembers(t, mr, idxGateA))
	assert.Empty(t, leaseMembers(t, mr))
	assert.Equal(t, 1, rep.actions[reconcileActionCASConflict])
	assert.Equal(t, 0, rep.actions[reconcileActionToDisconnecting])
}

// 每拍预算:单个死 gate 的成员数超过一拍预算时分几拍处理完,这一拍照常完成;
// 第二个死 gate 在第一拍之后仍被识别(计入 deadGates),预算轮到它时处理。
func TestIndexRound_BudgetSpreadsDeadGateAcrossRounds(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	for i := uint64(0); i < 6; i++ {
		require.NoError(t, callSetSession(sc, indexTestSession(9700+i, 100, idxGateA, 1)))
	}
	require.NoError(t, callSetSession(sc, indexTestSession(9800, 100, idxGateB, 1)))
	cfg := testReconcileConf()
	cfg.DeadGateMembersPerRound = 4
	cfg.ScanCount = 4
	r := newSessionReconciler(sc, cfg, testReconcileInterval, testReconcilerDeps(newFakeClusterView(), newTestClock()))

	mustIndexRoundOK(t, r)
	rep := mustIndexRoundOK(t, r)
	assert.Equal(t, []string{idxGateA, idxGateB}, rep.deadGates, "两个死 gate 都已确认")
	assert.Equal(t, 4, rep.actions[reconcileActionToDisconnecting], "本拍只用掉 4 个成员的预算")
	assert.Equal(t, int64(3), rep.deadGateMembers, "进度:A 剩 2、B 剩 1")
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_ONLINE, readSession(t, sc, 9800).State)

	for i := 0; i < 2; i++ {
		mustIndexRoundOK(t, r)
	}
	for i := uint64(0); i < 6; i++ {
		assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9700+i).State)
	}
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9800).State)
	assert.Empty(t, registryMembers(t, mr), "两个死 gate 处理完后都已释放")
}

// 一拍内多页续扫:死 gate 的成员数超过 SSCAN 的 COUNT、预算充足时,这一拍内按游标连续扫多页(processDeadGates 里
// 「next != 0 且预算还有余」的续扫分支),不把剩下的页都拖到下一拍。
//
// miniredis 与真实 Redis 的差异:miniredis 的 SSCAN 游标是排序后成员列表的偏移,转换会 SREM 掉已扫过的成员,
// 后面的成员前移,游标就跳过了一部分;真实 Redis 的游标按哈希桶推进,删除不影响覆盖。所以这里只断言「一拍内转换数
// 大于一页、SSCAN 至少两次」,释放留到下一拍再断言(真实 Redis 上第一拍就会释放,下一拍的断言同样成立)。
func TestIndexRound_MultiPageDeadGateSweepInOneRound(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	for i := uint64(0); i < 6; i++ {
		require.NoError(t, callSetSession(sc, indexTestSession(9870+i, 100, idxGateA, 1)))
	}
	cfg := testReconcileConf()
	cfg.ScanCount = 2
	r := newSessionReconciler(sc, cfg, testReconcileInterval, testReconcilerDeps(newFakeClusterView(), newTestClock()))
	mustIndexRoundOK(t, r) // 第一拍只挂嫌疑

	rec := newCommandRecorder()
	sc.RedisClient.AddHook(rec)
	rep := mustIndexRoundOK(t, r)

	assert.Greater(t, rep.actions[reconcileActionToDisconnecting], int(cfg.ScanCount), "一拍内处理了不止一页")
	assert.GreaterOrEqual(t, rec.count("sscan"), 2)

	mustIndexRoundOK(t, r)
	for i := uint64(0); i < 6; i++ {
		assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9870+i).State)
	}
	assert.False(t, mr.Exists(gateSessionIndexKey(idxGateA)))
	assert.NotContains(t, registryMembers(t, mr), idxGateA, "扫完一遍、索引空了才释放")
}

// 死 gate(实为分区存活,或 etcd 视图误判)的索引释放与并发 SetSession 竞态:释放之前有玩家经 SetSession 回到这个 gate。
// I2 的唯一保障是两件事都在 Redis 单线程上原子执行:SetSession 同时 SADD 成员与登记集;释放脚本内 SCARD==0 才 DEL。
//
// Hook 在释放脚本之前注入 SetSession。另外,凡是单独发出的、针对这个索引键的 SCARD 之后 / DEL 之前也注入 ——
// 现行实现没有这种命令,不会触发;有人把释放改成 Go 侧「先 SCARD 再 DEL」或无条件 DEL 时,注入就落在两步之间,
// 新成员被删,I1 断言变红。
func TestIndexRound_ReleaseRacesConcurrentSetSessionOnDeadGate(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9850, 100, idxGateA, 1)))
	// A 不在 etcd;第一拍只挂嫌疑。
	r := newTestReconciler(t, sc, newFakeClusterView())
	mustIndexRoundOK(t, r)

	other := secondClient(t, sc, mr)
	indexKeyA := gateSessionIndexKey(idxGateA)
	var injected []byte
	var once sync.Once
	inject := func() {
		once.Do(func() {
			require.NoError(t, callSetSession(other, indexTestSession(9851, 200, idxGateA, 1)))
			injected = readSessionBytes(t, other, 9851)
		})
	}
	rec := newCommandRecorder()
	rec.before = func(_ context.Context, cmd redis.Cmder) {
		switch {
		case isScriptOn(cmd, indexKeyA, GateIndexRegistryKey): // 释放脚本:KEYS = 索引键、登记集
			inject()
		case cmd.Name() == "del" && cmdArg(cmd, 1) == indexKeyA:
			inject()
		}
	}
	rec.after = func(cmd redis.Cmder) {
		if cmd.Name() == "scard" && cmdArg(cmd, 1) == indexKeyA {
			inject()
		}
	}
	sc.RedisClient.AddHook(rec)

	rep := mustIndexRoundOK(t, r)

	require.NotNil(t, injected, "释放之前必须注入过并发 SetSession")
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9850).State)
	assert.NotContains(t, rep.released, idxGateA, "索引里有新成员,不得释放")
	assert.Equal(t, []string{"9851"}, indexMembers(t, mr, idxGateA))
	assert.Contains(t, registryMembers(t, mr), idxGateA)
	assert.Equal(t, injected, readSessionBytes(t, sc, 9851), "新会话一个字节都没动")
	assertGateIndexInvariant(t, sc, mr, []uint64{9850, 9851}, []string{idxGateA}, "after the race")

	// A 仍不在 etcd、仍在登记集里:保持确认死亡,下一拍直接处理新成员并释放。
	rep = mustIndexRoundOK(t, r)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9851).State)
	assert.Contains(t, rep.released, idxGateA)
	assert.NotContains(t, registryMembers(t, mr), idxGateA)
	assertGateIndexInvariant(t, sc, mr, []uint64{9850, 9851}, []string{idxGateA}, "after the next round")
}

// 毒键:死 gate A 的索引里有一个成员,它的会话键被写成了 hash(外部篡改 / 误写)。MGET 对非 string 回 nil,判成
// missing,守卫脚本里的 GET 报 WRONGTYPE。只跳过这一个成员(member_error),每一拍照常完成、不让位:A 的其余成员与
// 死 gate B 照常转换、B 照常释放;毒成员留在 A 的索引里,A 不释放(由 DeadGatesPending 暴露)。
// 修复前每一拍都死在这个键上:连续 3 拍让位,换主后结果一样,全服死 gate 处理永久停摆。
func TestIndexRound_WrongTypeSessionKeySkipsMemberNotRound(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9881, 100, idxGateA, 1)))
	require.NoError(t, callSetSession(sc, indexTestSession(9882, 100, idxGateB, 1)))
	mr.HSet(sessionKey(9880), "field", "not a session")
	_, err := mr.SetAdd(gateSessionIndexKey(idxGateA), "9880")
	require.NoError(t, err)
	r := newTestReconciler(t, sc, newFakeClusterView())
	mustIndexRoundOK(t, r)

	for round := 0; round < maxConsecutiveRoundFailures; round++ {
		out := r.indexRound(context.Background())
		require.NoErrorf(t, out.err, "round %d: 一个毒键不得让整拍失败", round)
		assert.Equal(t, 1, out.report.actions[reconcileActionMemberError])
	}
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9881).State)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9882).State)
	assert.NotContains(t, registryMembers(t, mr), idxGateB, "B 照常释放")
	assert.Contains(t, registryMembers(t, mr), idxGateA, "A 留着毒成员,不释放")
	assert.Equal(t, []string{"9880"}, indexMembers(t, mr, idxGateA))
	assert.Equal(t, "hash", mr.Type(sessionKey(9880)), "毒键原样保留,留给排查")
}

// 某个 gate 的索引键被写成了别的类型:死 gate A 的 SSCAN、A 与存活 gate C 的 SCARD 都报 WRONGTYPE。只跳过这两个 gate
// (gate_error),每一拍照常完成,死 gate B 照常处理并释放。
func TestIndexRound_WrongTypeIndexKeySkipsGateNotRound(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9891, 100, idxGateB, 1)))
	require.NoError(t, mr.Set(gateSessionIndexKey(idxGateA), "not a set"))
	require.NoError(t, mr.Set(gateSessionIndexKey(idxGateC), "not a set"))
	_, err := mr.SetAdd(GateIndexRegistryKey, idxGateA, idxGateC)
	require.NoError(t, err)
	// A、B 死,C 活。第一拍:A、B 挂嫌疑;C 的 SCARD 已经报错,也不得让这一拍失败。
	r := newTestReconciler(t, sc, newFakeClusterView(idxGateC))
	mustIndexRoundOK(t, r)

	out := r.indexRound(context.Background())

	require.NoError(t, out.err)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9891).State)
	assert.Contains(t, out.report.released, idxGateB)
	assert.Equal(t, 3, out.report.actions[reconcileActionGateError], "A 的 SSCAN 与 SCARD、C 的 SCARD 各一次")
	assert.ElementsMatch(t, []string{idxGateA, idxGateC}, registryMembers(t, mr))
}

// 一拍里的单键错误超过上限:不再当作「个别键坏了」,整拍失败(计入让位门槛),错误链里保留原始的 WRONGTYPE。
func TestIndexRound_TooManyKeyTypeErrorsFailsRound(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	for i := 0; i <= maxKeyTypeErrorsPerRound; i++ {
		pid := uint64(20000 + i)
		mr.HSet(sessionKey(pid), "field", "x")
		_, err := mr.SetAdd(gateSessionIndexKey(idxGateA), strconv.FormatUint(pid, 10))
		require.NoError(t, err)
	}
	_, err := mr.SetAdd(GateIndexRegistryKey, idxGateA)
	require.NoError(t, err)
	r := newTestReconciler(t, sc, newFakeClusterView())
	mustIndexRoundOK(t, r)

	out := r.indexRound(context.Background())

	require.Error(t, out.err)
	assert.Equal(t, stageRedis, out.stage)
	assert.True(t, isKeyTypeError(out.err))
}

// 全局租约键(player:leases)被写成了别的类型:每个转换都报 WRONGTYPE。这不是哪个成员的问题,不能逐个跳过后报 ok
// (那样 last_success 照常推进、不让位,对账实际上什么都没做):同一页的转换全部失败 → 整拍失败。
func TestIndexRound_SharedLeaseKeyWrongTypeFailsRound(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9895, 100, idxGateA, 1)))
	require.NoError(t, mr.Set(LeaseZSetKey, "not a zset")) // SetSession 之后再坏掉,否则 SetSession 自己就失败
	r := newTestReconciler(t, sc, newFakeClusterView())
	mustIndexRoundOK(t, r)

	out := r.indexRound(context.Background())

	require.Error(t, out.err)
	assert.Equal(t, stageRedis, out.stage)
	assert.Equal(t, 1, out.report.actions[reconcileActionMemberError])
}

// etcd 读写者列表出错:只跳过本拍写者检查,不删就绪标记、不动代次,标记租约不被续(照常走时);
// 补建也不会因此启动。整拍记为 etcd 失败(计入让位门槛)。
func TestIndexRound_SessionWritersErrorKeepsMarkerAndGen(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	setReadyMarker(t, mr, 100*time.Second)
	require.NoError(t, mr.Set(gateIndexGenKey, "5"))
	view := newFakeClusterView()
	view.setWritersErr(errors.New("etcd timeout"))
	clock := newTestClock()
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, testReconcilerDeps(view, clock))
	require.NoError(t, mr.Set(gateIndexLastAuditKey, strconv.FormatInt(clock.Now().Unix(), 10)))

	for i := 0; i < 2; i++ {
		out := r.indexRound(context.Background())
		require.Error(t, out.err)
		assert.Equal(t, stageEtcd, out.stage)
	}
	assert.True(t, mr.Exists(gateIndexReadyKey))
	assert.Equal(t, 100*time.Second, mr.TTL(gateIndexReadyKey), "出错的拍不续租")
	gen, _ := mr.Get(gateIndexGenKey)
	assert.Equal(t, "5", gen)

	rec := newCommandRecorder()
	sc.RedisClient.AddHook(rec)
	step := r.repairStep(context.Background())
	assert.Equal(t, "", step.loop, "标记还在、审计未到期:不跑补建")
	assert.Equal(t, 0, rec.count("scan"))
}

// 只有一次成功的 etcd 读里确实看到「已注册却没有能力键」的 locator,才删标记并递增代次。
func TestIndexRound_LegacyWriterClearsMarkerAndBumpsGen(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	setReadyMarker(t, mr, 100*time.Second)
	require.NoError(t, mr.Set(gateIndexGenKey, "5"))
	view := newFakeClusterView()
	view.setWriters([]string{"locator-new", "locator-old"}, []string{"locator-new"})
	r := newTestReconciler(t, sc, view)

	rep := mustIndexRoundOK(t, r)

	assert.Equal(t, 1, rep.legacyWriters)
	assert.False(t, mr.Exists(gateIndexReadyKey))
	gen, _ := mr.Get(gateIndexGenKey)
	assert.Equal(t, "6", gen)
}

// 零旧写者的成功拍给已存在的标记续租到 readyLease;别的版本前缀的标记不续;不存在的标记不新建。
func TestIndexRound_RenewsExistingReadyMarkerOnly(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	r := newTestReconciler(t, sc, newFakeClusterView())

	rep := mustIndexRoundOK(t, r)
	assert.False(t, rep.readyRenewed)
	assert.False(t, mr.Exists(gateIndexReadyKey), "续期不得新建标记")

	setReadyMarker(t, mr, 5*time.Second)
	rep = mustIndexRoundOK(t, r)
	assert.True(t, rep.readyRenewed)
	assert.Equal(t, r.readyLease(), mr.TTL(gateIndexReadyKey))

	require.NoError(t, mr.Set(gateIndexReadyKey, "v2:1"))
	mr.SetTTL(gateIndexReadyKey, 5*time.Second)
	rep = mustIndexRoundOK(t, r)
	assert.False(t, rep.readyRenewed)
	assert.Equal(t, 5*time.Second, mr.TTL(gateIndexReadyKey))
}

// ---------------------------------------------------------------------------
// 补建 / 审计
// ---------------------------------------------------------------------------

// 旧写者写的 ONLINE 会话不在索引里:补建补齐成员与登记集并设就绪标记(带租约),随后索引循环两拍内完成转换。
func TestRepairPass_BackfillsLegacySessionAndSetsReady(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	writeRawSession(t, mr, indexTestSession(9901, 100, idxGateA, 1))
	r := newTestReconciler(t, sc, newFakeClusterView()) // A 已死

	step := r.repairStep(context.Background())
	assert.Equal(t, reconcileLoopRepair, step.loop)
	assert.Equal(t, reconcileResultOK, step.result)
	assert.Equal(t, 1, step.report.repaired)
	assert.Equal(t, []string{"9901"}, indexMembers(t, mr, idxGateA))
	assert.Contains(t, registryMembers(t, mr), idxGateA)
	assert.True(t, mr.Exists(gateIndexReadyKey))
	assert.Equal(t, r.readyLease(), mr.TTL(gateIndexReadyKey))
	assert.True(t, mr.Exists(gateIndexLastAuditKey), "完整补建同时记下审计时刻")

	mustIndexRoundOK(t, r)
	mustIndexRoundOK(t, r)
	assert.Equal(t, pb.PlayerSessionState_SESSION_STATE_DISCONNECTING, readSession(t, sc, 9901).State)
}

// 补建开始时就有旧写者:照样补,但代次已被这次检查递增,结束时不设标记。
func TestRepairPass_LegacyWriterBlocksReady(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	writeRawSession(t, mr, indexTestSession(9902, 100, idxGateA, 1))
	view := newFakeClusterView(idxGateA)
	view.setWriters([]string{"locator-new", "locator-old"}, []string{"locator-new"})
	r := newTestReconciler(t, sc, view)

	step := r.repairStep(context.Background())

	assert.Equal(t, reconcileResultGenChanged, step.result)
	assert.False(t, mr.Exists(gateIndexReadyKey))
	assert.Equal(t, []string{"9902"}, indexMembers(t, mr, idxGateA), "补进来的成员照样有效")
}

// 补建开始前 etcd 读写者失败:这一遍不开始,也不设标记。
func TestRepairPass_WriterCheckErrorDoesNotStart(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	view := newFakeClusterView()
	view.setWritersErr(errors.New("etcd timeout"))
	rec := newCommandRecorder()
	sc.RedisClient.AddHook(rec)
	r := newTestReconciler(t, sc, view)

	step := r.repairStep(context.Background())

	assert.Equal(t, reconcileResultEtcdError, step.result)
	assert.Equal(t, 0, rec.count("scan"))
	assert.False(t, mr.Exists(gateIndexReadyKey))
}

// 补建进行中(第一页之后)别的拍看到了旧写者、递增了代次:这一遍结束时「代次未变才设」失败,不设标记。
func TestRepairPass_GenChangeMidPassBlocksReady(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	for i := uint64(0); i < 3; i++ {
		writeRawSession(t, mr, indexTestSession(9910+i, 100, idxGateA, 1))
	}
	cfg := testReconcileConf()
	cfg.ScanCount = 1
	other := secondClient(t, sc, mr)
	rec := newCommandRecorder()
	var once sync.Once
	rec.after = func(cmd redis.Cmder) {
		if cmd.Name() != "scan" {
			return
		}
		once.Do(func() {
			require.NoError(t, markLegacyWritersScript.Run(context.Background(), other.RedisClient,
				[]string{gateIndexGenKey, gateIndexReadyKey}).Err())
		})
	}
	sc.RedisClient.AddHook(rec)
	r := newSessionReconciler(sc, cfg, testReconcileInterval, testReconcilerDeps(newFakeClusterView(idxGateA), newTestClock()))

	step := r.repairStep(context.Background())

	assert.Equal(t, reconcileResultGenChanged, step.result)
	assert.False(t, mr.Exists(gateIndexReadyKey))
}

// 不续期时标记自然过期(没有新版 leader 在跑:一次性回滚、对账关闭),下一任 leader 从补建开始。
func TestReadyMarker_ExpiresWithoutRenewalAndNextLeaderBackfills(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9920, 100, idxGateA, 1)))
	first := newTestReconciler(t, sc, newFakeClusterView(idxGateA))
	require.Equal(t, reconcileResultOK, first.repairStep(context.Background()).result)
	require.True(t, mr.Exists(gateIndexReadyKey))

	mr.FastForward(first.readyLease() + time.Second)
	require.False(t, mr.Exists(gateIndexReadyKey), "没人续期,标记按租约过期")

	rec := newCommandRecorder()
	sc.RedisClient.AddHook(rec)
	next := newTestReconciler(t, sc, newFakeClusterView(idxGateA))
	step := next.repairStep(context.Background())
	assert.Equal(t, reconcileLoopRepair, step.loop)
	assert.Equal(t, reconcileResultOK, step.result)
	assert.Positive(t, rec.count("scan"), "下一任 leader 重新全键补建")
	assert.True(t, mr.Exists(gateIndexReadyKey))
}

// DISCONNECTING 会话不补;MGET 之后会话被改写(脚本按字节守卫返回 -1)也不补。
func TestRepairPass_SkipsNonOnlineAndChangedSessions(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	disconnecting := indexTestSession(9930, 100, idxGateA, 2)
	disconnecting.State = pb.PlayerSessionState_SESSION_STATE_DISCONNECTING
	writeRawSession(t, mr, disconnecting)
	writeRawSession(t, mr, indexTestSession(9931, 100, idxGateA, 1))

	other := secondClient(t, sc, mr)
	rec := newCommandRecorder()
	var once sync.Once
	rec.after = func(cmd redis.Cmder) {
		if cmd.Name() != "mget" {
			return
		}
		once.Do(func() {
			changed, err := proto.Marshal(indexTestSession(9931, 200, idxGateA, 2))
			require.NoError(t, err)
			require.NoError(t, other.RedisClient.Set(context.Background(), sessionKey(9931), changed, 0).Err())
		})
	}
	sc.RedisClient.AddHook(rec)
	r := newTestReconciler(t, sc, newFakeClusterView(idxGateA))

	step := r.repairStep(context.Background())

	assert.Equal(t, 0, step.report.repaired)
	assert.Nil(t, indexMembers(t, mr, idxGateA))
}

// 标记存在时人为删掉一个成员:审计补回来,并计为 audit_repaired(不变量被破坏)。
func TestAuditPass_CountsRepairedWhenReady(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9940, 100, idxGateA, 1)))
	_, err := mr.SRem(gateSessionIndexKey(idxGateA), "9940")
	require.NoError(t, err)
	setReadyMarker(t, mr, 100*time.Second)
	clock := newTestClock()
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, testReconcilerDeps(newFakeClusterView(idxGateA), clock))

	step := r.repairStep(context.Background()) // last_audit 缺失 = 到期

	assert.Equal(t, reconcileLoopAudit, step.loop)
	assert.Equal(t, reconcileResultOK, step.result)
	assert.Equal(t, 1, step.report.repaired)
	assert.Equal(t, 1, step.report.actions[reconcileActionAuditRepaired])
	assert.Equal(t, []string{"9940"}, indexMembers(t, mr, idxGateA))
	last, _ := mr.Get(gateIndexLastAuditKey)
	assert.Equal(t, strconv.FormatInt(clock.Now().Unix(), 10), last)
}

// 审计时机以 Redis 里的上次审计时刻为准:换一个新的 reconciler(换主)不重置计时,到期才跑。
func TestAudit_TimingPersistedAcrossLeaders(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	setReadyMarker(t, mr, 100*time.Second)
	clock := newTestClock()
	require.NoError(t, mr.Set(gateIndexLastAuditKey, strconv.FormatInt(clock.Now().Unix()-100, 10)))
	rec := newCommandRecorder()
	sc.RedisClient.AddHook(rec)

	first := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, testReconcilerDeps(newFakeClusterView(), clock))
	assert.Equal(t, "", first.repairStep(context.Background()).loop)

	clock.Advance(time.Duration(86400-100-1) * time.Second)
	next := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, testReconcilerDeps(newFakeClusterView(), clock))
	assert.Equal(t, "", next.repairStep(context.Background()).loop, "差 1 秒未到期")
	assert.Equal(t, 0, rec.count("scan"))

	clock.Advance(time.Second)
	step := next.repairStep(context.Background())
	assert.Equal(t, reconcileLoopAudit, step.loop)
	assert.Positive(t, rec.count("scan"))
	last, _ := mr.Get(gateIndexLastAuditKey)
	assert.Equal(t, strconv.FormatInt(clock.Now().Unix(), 10), last)
}

// 审计对**存活** gate 的索引也按字节守卫清脏成员;真成员不动。
func TestAuditPass_PrunesDirtyMembersOnLiveGates(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9950, 100, idxGateA, 1)))
	require.NoError(t, callSetSession(sc, indexTestSession(9951, 100, idxGateB, 1)))
	before := readSessionBytes(t, sc, 9950)
	_, err := mr.SetAdd(gateSessionIndexKey(idxGateA), "9951", "9952", "junk")
	require.NoError(t, err)
	setReadyMarker(t, mr, 100*time.Second)
	r := newTestReconciler(t, sc, newFakeClusterView(idxGateA, idxGateB))

	step := r.repairStep(context.Background())

	assert.Equal(t, reconcileLoopAudit, step.loop)
	assert.Equal(t, []string{"9950"}, indexMembers(t, mr, idxGateA))
	assert.Equal(t, []string{"9951"}, indexMembers(t, mr, idxGateB))
	assert.Equal(t, before, readSessionBytes(t, sc, 9950))
	assert.Equal(t, 1, step.report.actions[reconcileActionPrunedStale])
	assert.Equal(t, 1, step.report.actions[reconcileActionPrunedMissing])
	assert.Equal(t, 1, step.report.actions[reconcileActionPrunedBadMember])
	assert.Empty(t, leaseMembers(t, mr), "审计不转换会话")
}

// last_audit 比本 leader 的时钟超前一个审计周期以上:只可能是写入者的时钟坏了(例如 NTP 故障快了几天)。当坏值处理、
// 照常审计;否则此后所有 leader 都算出负数,审计停摆到各自时钟追上为止。偏差在一个周期以内的照常比较。
func TestAudit_LastAuditFarInFutureTreatedAsDue(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	setReadyMarker(t, mr, 100*time.Second)
	clock := newTestClock()
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, testReconcilerDeps(newFakeClusterView(), clock))
	now := clock.Now().Unix()

	require.NoError(t, mr.Set(gateIndexLastAuditKey, strconv.FormatInt(now+3600, 10)))
	assert.Equal(t, "", r.repairStep(context.Background()).loop, "偏差小于审计周期:照常比较,未到期")

	require.NoError(t, mr.Set(gateIndexLastAuditKey, strconv.FormatInt(now+86400+3600, 10)))
	step := r.repairStep(context.Background())
	assert.Equal(t, reconcileLoopAudit, step.loop)
	assert.Equal(t, reconcileResultOK, step.result)
	last, _ := mr.Get(gateIndexLastAuditKey)
	assert.Equal(t, strconv.FormatInt(now, 10), last, "审计完成后改写成本 leader 的时刻")
}

// 补建一遍内单页失败(一次没被 go-redis 重试吸收的抖动):对同一游标退避重试,这一遍照常完成并设上就绪标记,
// 不作废几个小时的进度。miniredis 的 SCAN 一次返回全部键(只有游标 0 这一页),非零游标的重试见审计的 SSCAN 用例。
func TestRepairPass_RetriesFailedScanPageWithSameCursor(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	writeRawSession(t, mr, indexTestSession(9970, 100, idxGateA, 1))
	rec := newCommandRecorder()
	var scans atomic.Int32
	rec.fail = func(cmd redis.Cmder) error {
		if cmd.Name() == "scan" && scans.Add(1) == 1 {
			return errors.New("i/o timeout")
		}
		return nil
	}
	sc.RedisClient.AddHook(rec)
	var sleeps []time.Duration
	deps := testReconcilerDeps(newFakeClusterView(idxGateA), newTestClock())
	deps.sleep = func(ctx context.Context, d time.Duration) error {
		sleeps = append(sleeps, d)
		return ctx.Err()
	}
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, deps)

	step := r.repairStep(context.Background())

	assert.Equal(t, reconcileResultOK, step.result)
	assert.True(t, mr.Exists(gateIndexReadyKey))
	assert.Equal(t, []string{"0", "0"}, rec.scanCursors(), "失败的页按同一游标重试")
	assert.Equal(t, []time.Duration{passPageRetryBase}, sleeps)
	assert.Equal(t, 1, step.report.pageRetries)
	assert.Equal(t, []string{"9970"}, indexMembers(t, mr, idxGateA))
}

// 审计(或补建)第二遍扫描阶段的第二页 SSCAN 失败一次:按同一个非零游标重试,这一遍照常完成并记下审计时刻。
func TestAuditPass_RetriesFailedIndexPageWithSameCursor(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	for i := uint64(0); i < 5; i++ {
		require.NoError(t, callSetSession(sc, indexTestSession(9975+i, 100, idxGateA, 1)))
	}
	setReadyMarker(t, mr, 100*time.Second)
	cfg := testReconcileConf()
	cfg.ScanCount = 2
	rec := newCommandRecorder()
	var sscans atomic.Int32
	rec.fail = func(cmd redis.Cmder) error {
		if cmd.Name() == "sscan" && sscans.Add(1) == 2 {
			return errors.New("read: connection reset by peer")
		}
		return nil
	}
	sc.RedisClient.AddHook(rec)
	clock := newTestClock()
	r := newSessionReconciler(sc, cfg, testReconcileInterval, testReconcilerDeps(newFakeClusterView(idxGateA), clock))

	step := r.repairStep(context.Background()) // last_audit 缺失 = 到期

	assert.Equal(t, reconcileLoopAudit, step.loop)
	assert.Equal(t, reconcileResultOK, step.result)
	cursors := rec.sscanCursors()
	require.GreaterOrEqual(t, len(cursors), 3)
	assert.Equal(t, []string{"0", "2", "2"}, cursors[:3], "第二页失败后按同一个非零游标重试")
	assert.Equal(t, 1, step.report.pageRetries)
	last, _ := mr.Get(gateIndexLastAuditKey)
	assert.Equal(t, strconv.FormatInt(clock.Now().Unix(), 10), last)
	assert.Len(t, indexMembers(t, mr, idxGateA), 5, "审计不动真成员")
}

// 持续失败:同一页按指数退避重试 passPageMaxAttempts 次后放弃这一遍(redis_error),不设就绪标记;
// 补建循环一拍后再来一遍。
func TestRepairPass_GivesUpAfterMaxAttempts(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	writeRawSession(t, mr, indexTestSession(9985, 100, idxGateA, 1))
	rec := newCommandRecorder()
	rec.fail = func(cmd redis.Cmder) error {
		if cmd.Name() == "scan" {
			return errors.New("i/o timeout")
		}
		return nil
	}
	sc.RedisClient.AddHook(rec)
	var sleeps []time.Duration
	deps := testReconcilerDeps(newFakeClusterView(idxGateA), newTestClock())
	deps.sleep = func(ctx context.Context, d time.Duration) error {
		sleeps = append(sleeps, d)
		return ctx.Err()
	}
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, deps)

	step := r.repairStep(context.Background())

	assert.Equal(t, reconcileResultRedisError, step.result)
	assert.False(t, mr.Exists(gateIndexReadyKey))
	assert.Equal(t, passPageMaxAttempts, rec.count("scan"))
	var want []time.Duration
	for backoff, i := passPageRetryBase, 1; i < passPageMaxAttempts; i++ {
		want = append(want, backoff)
		backoff = min(backoff*2, passPageRetryMax)
	}
	assert.Equal(t, want, sleeps, "指数退避,次数有上限")
}

// 审计失败后设冷却键(Redis 侧 EX):冷却期内审计不到期,不让持续的故障每拍重开一遍全键扫描;失败的审计不记完成时刻;
// 冷却过后照常重试。
func TestAuditPass_FailureSetsCooldownKey(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	setReadyMarker(t, mr, 100*time.Hour)
	var failing atomic.Bool
	failing.Store(true)
	rec := newCommandRecorder()
	rec.fail = func(cmd redis.Cmder) error {
		if failing.Load() && cmd.Name() == "scan" {
			return errors.New("i/o timeout")
		}
		return nil
	}
	sc.RedisClient.AddHook(rec)
	r := newTestReconciler(t, sc, newFakeClusterView())

	step := r.repairStep(context.Background())
	assert.Equal(t, reconcileLoopAudit, step.loop)
	assert.Equal(t, reconcileResultRedisError, step.result)
	assert.False(t, mr.Exists(gateIndexLastAuditKey), "失败的审计不记完成时刻")
	assert.True(t, mr.Exists(gateIndexAuditCooldownKey))
	assert.Equal(t, auditFailureCooldown, mr.TTL(gateIndexAuditCooldownKey))

	failing.Store(false)
	assert.Equal(t, "", r.repairStep(context.Background()).loop, "冷却期内不审计")

	mr.FastForward(auditFailureCooldown)
	step = r.repairStep(context.Background())
	assert.Equal(t, reconcileLoopAudit, step.loop)
	assert.Equal(t, reconcileResultOK, step.result)
	assert.True(t, mr.Exists(gateIndexLastAuditKey))
}

// 扫描期间 Redis 换了进程(主从切换 / 重启,run_id 变了):旧游标在新进程上没有覆盖保证,这一遍作废、不设就绪标记;
// 补进来的成员照样有效。下一遍(run_id 不再变)正常设上标记。
func TestRepairPass_RedisRunIDChangeDiscardsPass(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	writeRawSession(t, mr, indexTestSession(9990, 100, idxGateA, 1))
	var reads atomic.Int32
	deps := testReconcilerDeps(newFakeClusterView(idxGateA), newTestClock())
	deps.redisRunID = func(context.Context, *redis.Client) (string, error) {
		if reads.Add(1) == 1 {
			return "run-before-failover", nil
		}
		return "run-after-failover", nil
	}
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, deps)

	step := r.repairStep(context.Background())
	assert.Equal(t, reconcileResultRedisError, step.result)
	assert.False(t, mr.Exists(gateIndexReadyKey))
	assert.Equal(t, []string{"9990"}, indexMembers(t, mr, idxGateA), "补进来的成员照样有效")

	step = r.repairStep(context.Background())
	assert.Equal(t, reconcileResultOK, step.result)
	assert.True(t, mr.Exists(gateIndexReadyKey))
}

// 补建遍里有 ONLINE 会话因为它所在 gate 的索引键类型不对而没能核对 / 补进索引:跳过它、计 member_error,这一遍照常
// 扫完(别的 gate 上漏的会话照样补),但结束时不设就绪标记(incomplete)—— 否则就是在仍有会话缺成员时宣布 I1 成立。
func TestRepairPass_SkippedCandidateBlocksReady(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	writeRawSession(t, mr, indexTestSession(9993, 100, idxGateA, 1))
	writeRawSession(t, mr, indexTestSession(9994, 100, idxGateB, 1))
	require.NoError(t, mr.Set(gateSessionIndexKey(idxGateA), "not a set"))
	r := newTestReconciler(t, sc, newFakeClusterView(idxGateA, idxGateB))

	step := r.repairStep(context.Background())

	assert.Equal(t, reconcileLoopRepair, step.loop)
	assert.Equal(t, reconcileResultIncomplete, step.result)
	assert.Equal(t, 1, step.report.skippedCandidates)
	assert.Equal(t, 1, step.report.actions[reconcileActionMemberError])
	assert.False(t, mr.Exists(gateIndexReadyKey))
	assert.Equal(t, []string{"9994"}, indexMembers(t, mr, idxGateB), "别的 gate 上漏的会话照样补")
}

// ---------------------------------------------------------------------------
// 任期:让位与生命周期
// ---------------------------------------------------------------------------

// etcd 连续 3 拍读不出来,leader 主动让位(run 返回 true)。
func TestRun_YieldsAfterConsecutiveFailures(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	view := newFakeClusterView()
	view.setLiveErr(errors.New("etcd unreachable from this pod"))
	deps := testReconcilerDeps(view, newTestClock())
	deps.sleep = blockUntilDone
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, deps)

	assert.True(t, r.run(context.Background()))
	assert.Equal(t, maxConsecutiveRoundFailures, view.liveCallCount())
}

// 失败不满 3 拍、但距上次成功已超过 3 拍(每拍拖得很长):同样让位。
func TestRun_YieldsWhenNoSuccessForThreeIntervals(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	view := newFakeClusterView()
	view.setLiveErr(errors.New("etcd slow"))
	clock := newTestClock()
	deps := testReconcilerDeps(view, clock)
	deps.sleep = blockUntilDone
	deps.newTicker = func(time.Duration) (func(context.Context) error, func()) {
		return func(ctx context.Context) error {
			clock.Advance(4 * testReconcileInterval)
			return ctx.Err()
		}, func() {}
	}
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, deps)

	assert.True(t, r.run(context.Background()))
	assert.Equal(t, 2, view.liveCallCount())
}

// 实例级的服务端错误(READONLY:主从切换中连到了只读副本;OOM、LOADING、BUSY 同理)也是 redis.Error,但不是单键问题,
// 不得按「跳过成员」吸收:整拍失败,连续 3 拍让位,让别的副本(可能连着新主)接手。会话一个字节都不动。
func TestRun_YieldsOnInstanceLevelRedisErrorOnTransition(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9897, 100, idxGateA, 1)))
	before := readSessionBytes(t, sc, 9897)
	// 让补建 / 审计循环什么都不做:标记在、审计刚做过。
	clock := newTestClock()
	setReadyMarker(t, mr, 100*time.Second)
	require.NoError(t, mr.Set(gateIndexLastAuditKey, strconv.FormatInt(clock.Now().Unix(), 10)))
	rec := newCommandRecorder()
	rec.fail = func(cmd redis.Cmder) error {
		if (cmd.Name() == "evalsha" || cmd.Name() == "eval") && cmdArg(cmd, 3) == sessionKey(9897) {
			return injectedRedisError("READONLY You can't write against a read only replica.")
		}
		return nil
	}
	sc.RedisClient.AddHook(rec)
	view := newFakeClusterView()
	deps := testReconcilerDeps(view, clock)
	deps.sleep = blockUntilDone
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, deps)

	assert.True(t, r.run(context.Background()), "实例级错误必须计入让位门槛")
	assert.Equal(t, 1+maxConsecutiveRoundFailures, view.liveCallCount(), "第一拍挂嫌疑,之后连续 3 拍转换失败")
	assert.Equal(t, before, readSessionBytes(t, sc, 9897))
}

// 让位后:选主锁已被释放(健康副本立刻能抢到),本进程先等一个 TTL 再竞选。用真实选主器 + miniredis。
func TestCampaign_YieldReleasesLeaderKeyAndBacksOff(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	view := newFakeClusterView()
	view.setLiveErr(errors.New("etcd unreachable from this pod"))
	deps := testReconcilerDeps(view, newTestClock())
	deps.sleep = blockUntilDone
	elector := leader.New(leader.NewGoRedisStore(sc.RedisClient), reconcilerLeaderKey,
		leader.Options{TTL: reconcilerLeaderTTL, ID: "test-replica"})

	var heldDuringTerm atomic.Bool
	backoff := make(chan time.Duration, 1)
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})
	go func() {
		defer close(done)
		campaignSessionReconciler(ctx, elector.Run, reconcilerLeaderTTL,
			func(ctx context.Context, d time.Duration) error {
				backoff <- d
				<-ctx.Done()
				return ctx.Err()
			},
			func(lctx context.Context) bool {
				heldDuringTerm.Store(mr.Exists(reconcilerLeaderKey))
				return newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, deps).run(lctx)
			})
	}()

	select {
	case d := <-backoff:
		assert.Equal(t, reconcilerLeaderTTL, d)
	case <-time.After(10 * time.Second): // 只是防挂死的护栏,正确性不依赖它
		t.Fatal("reconciler never yielded")
	}
	assert.True(t, heldDuringTerm.Load(), "任期内持有选主锁")
	assert.False(t, mr.Exists(reconcilerLeaderKey), "让位后选主锁已释放,等待期间不再持有")

	cancel()
	<-done
}

// run 返回时补建循环已经停下:取消前补建正卡在一条 SCAN 上,run 等它返回后才返回,之后不再有在途命令。
func TestRun_WaitsForRepairLoopBeforeReturning(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	require.NoError(t, callSetSession(sc, indexTestSession(9960, 100, idxGateA, 1))) // 补建要有键可扫

	var inflight atomic.Int32
	scanStarted := make(chan struct{})
	var once sync.Once
	rec := newCommandRecorder()
	rec.before = func(ctx context.Context, cmd redis.Cmder) {
		if cmd.Name() != "scan" {
			return
		}
		inflight.Add(1)
		once.Do(func() { close(scanStarted) })
		<-ctx.Done() // 模拟一页很慢的 SCAN:直到任期取消
	}
	rec.after = func(cmd redis.Cmder) {
		if cmd.Name() == "scan" {
			inflight.Add(-1)
		}
	}
	sc.RedisClient.AddHook(rec)

	deps := testReconcilerDeps(newFakeClusterView(idxGateA), newTestClock())
	deps.sleep = blockUntilDone
	deps.newTicker = func(time.Duration) (func(context.Context) error, func()) {
		return func(ctx context.Context) error { <-ctx.Done(); return ctx.Err() }, func() {}
	}
	r := newSessionReconciler(sc, testReconcileConf(), testReconcileInterval, deps)

	lctx, cancel := context.WithCancel(context.Background())
	returned := make(chan bool, 1)
	go func() { returned <- r.run(lctx) }()

	select {
	case <-scanStarted:
	case <-time.After(10 * time.Second): // 防挂死护栏
		t.Fatal("repair loop never issued SCAN")
	}
	cancel()
	assert.False(t, <-returned, "任期取消不是让位")
	assert.Equal(t, int32(0), inflight.Load(), "run 返回时补建的 SCAN 已经返回")
}

// 两个限速器分开:转换限速给索引循环,扫描限速给补建 / 审计;转换限速调低时每拍成员预算被截到半拍的量。
func TestNewSessionReconciler_SeparatePacersAndBudget(t *testing.T) {
	sc, _ := newTestSvcCtx(t)
	var rates []int
	deps := testReconcilerDeps(newFakeClusterView(), newTestClock())
	deps.newPacer = func(perSecond int) pacer {
		rates = append(rates, perSecond)
		return &intervalPacer{}
	}
	cfg := testReconcileConf()
	cfg.MaxTransitionsPerSecond = 100
	cfg.ScanPagesPerSecond = 7

	r := newSessionReconciler(sc, cfg, testReconcileInterval, deps)

	assert.Equal(t, []int{100, 7}, rates)
	assert.NotSame(t, r.transitionPacer, r.scanPacer)
	assert.Equal(t, 3000, r.memberBudget, "100/s × 60s / 2")
}

// ---------------------------------------------------------------------------
// 确定性序列测试:固定种子在多个 gate 间随机交错 SetSession / Reconnect / SetDisconnecting / MarkOffline /
// 对账转换,每一步之后对全部玩家与 gate 断言 I1(以及 I2:索引非空 ⇒ gate 在登记集),且不落空串键。
// (2026-10-18 从 gate_session_index_test.go 挪来:它要驱动对账,那个文件只用修复前就有的符号。)
// ---------------------------------------------------------------------------

func TestGateIndex_InvariantHoldsOverRandomSequence(t *testing.T) {
	sc, mr := newTestSvcCtx(t)
	ctx := context.Background()
	rng := rand.New(rand.NewSource(20261018))
	pids := []uint64{8001, 8002, 8003, 8004, 8005, 8006}
	gates := []string{idxGateA, idxGateB, idxGateC, ""} // "" = 旧版 login 写的无实例号会话
	r := newTestReconciler(t, sc, newFakeClusterView())

	readCurrent := func(pid uint64) *pb.PlayerSession {
		data, err := sc.RedisClient.Get(ctx, sessionKey(pid)).Bytes()
		if err != nil {
			return nil
		}
		s := &pb.PlayerSession{}
		require.NoError(t, proto.Unmarshal(data, s))
		return s
	}
	var nextSessionID uint32 = 1000

	for step := 0; step < 600; step++ {
		pid := pids[rng.Intn(len(pids))]
		gate := gates[rng.Intn(len(gates))]
		current := readCurrent(pid)
		nextSessionID++
		op := rng.Intn(6)
		switch op {
		case 0, 1: // SetSession:首登或顶号(偶尔故意给错版本,验证被拒时索引不动)
			version := uint32(1)
			if current != nil {
				version = current.SessionVersion + 1
				if rng.Intn(5) == 0 {
					version += 3
				}
			}
			_ = callSetSession(sc, indexTestSession(pid, nextSessionID, gate, version))
		case 2: // Reconnect(只有 DISCONNECTING 才会成功,其余被拒)
			_, err := NewReconnectLogic(ctx, sc).Reconnect(&pb.ReconnectRequest{
				PlayerId: pid, NewSessionId: nextSessionID, GateId: "2", GateInstanceId: gate,
				RequestId: fmt.Sprintf("rc-%d", step),
			})
			require.NoError(t, err)
		case 3: // SetDisconnecting:当前会话号或错号;实例号取记录值、空或别的 gate
			if current == nil {
				continue
			}
			sessionID := current.SessionId
			if rng.Intn(4) == 0 {
				sessionID++
			}
			reqGate := []string{current.GateInstanceId, "", gate}[rng.Intn(3)]
			_, err := NewSetDisconnectingLogic(ctx, sc).SetDisconnecting(&pb.SetDisconnectingRequest{
				PlayerId: pid, SessionId: sessionID, GateInstanceId: reqGate,
			})
			require.NoError(t, err)
		case 4: // MarkOffline:精确或陈旧的 expected
			if current == nil {
				continue
			}
			version := current.SessionVersion
			if rng.Intn(4) == 0 {
				version++
			}
			_, err := NewMarkOfflineLogic(ctx, sc).MarkOffline(&pb.MarkOfflineRequest{
				PlayerId: pid, ExpectedSessionId: current.SessionId, ExpectedSessionVersion: version,
				ExpectedGateInstanceId: current.GateInstanceId,
			})
			require.NoError(t, err)
		case 5: // 对账把某个 gate 当作已确认死亡处理一遍(转换 + 摘除 + 空了就释放)
			if gate == "" {
				continue
			}
			rep := indexRoundReport{actions: map[string]int{}}
			require.NoError(t, r.processDeadGates(ctx, []string{gate}, &rep))
		}
		assertGateIndexInvariant(t, sc, mr, pids, gates, fmt.Sprintf("step=%d op=%d pid=%d gate=%q", step, op, pid, gate))
	}
}

// assertGateIndexInvariant 断言 I1 / I2,并确认没有名为 "" 的键。
func assertGateIndexInvariant(t *testing.T, sc *svc.ServiceContext, mr *miniredis.Miniredis, pids []uint64, gates []string, where string) {
	t.Helper()
	ctx := context.Background()
	registry := map[string]bool{}
	for _, g := range registryMembers(t, mr) {
		registry[g] = true
	}
	for _, pid := range pids {
		want := ""
		if data, err := sc.RedisClient.Get(ctx, sessionKey(pid)).Bytes(); err == nil {
			s := &pb.PlayerSession{}
			require.NoError(t, proto.Unmarshal(data, s), where)
			want = indexedGate(s)
		}
		for _, gate := range gates {
			if gate == "" {
				continue
			}
			member, err := mr.IsMember(gateSessionIndexKey(gate), strconv.FormatUint(pid, 10))
			if err != nil {
				member = false // 索引键不存在
			}
			require.Equalf(t, want == gate, member, "%s: I1 broken for player %d on gate %s (session indexed gate %q)",
				where, pid, gate, want)
		}
	}
	for _, gate := range gates {
		if gate != "" && len(indexMembers(t, mr, gate)) > 0 {
			require.Truef(t, registry[gate], "%s: I2 broken: gate %s has members but is not registered", where, gate)
		}
	}
	require.False(t, mr.Exists(""), where)
}

// ---------------------------------------------------------------------------
// 纯函数
// ---------------------------------------------------------------------------

func TestIsKeyTypeError(t *testing.T) {
	wrongType := "WRONGTYPE Operation against a key holding the wrong kind of value"
	assert.True(t, isKeyTypeError(injectedRedisError(wrongType)), "Redis 7:错误码前缀保留")
	assert.True(t, isKeyTypeError(fmt.Errorf("prune member: %w",
		injectedRedisError("ERR Error running script (call to f_x): @user_script:2: "+wrongType))), "Redis 6:包在脚本错误里")
	for _, msg := range []string{
		"READONLY You can't write against a read only replica.",
		"LOADING Redis is loading the dataset in memory",
		"OOM command not allowed when used memory > 'maxmemory'.",
		"BUSY Redis is busy running a script.",
		"MASTERDOWN Link with MASTER is down",
	} {
		assert.False(t, isKeyTypeError(injectedRedisError(msg)), "实例级故障不得当成单键问题:%s", msg)
	}
	assert.False(t, isKeyTypeError(errors.New("dial tcp: i/o timeout "+wrongType)), "不是服务端回复的错误,文本里有这个词也不算")
	assert.False(t, isKeyTypeError(context.DeadlineExceeded))
	assert.False(t, isKeyTypeError(nil))
}

func TestParseRedisRunID(t *testing.T) {
	info := "# Server\r\nredis_version:7.2.4\r\nrun_id:8f3c2a1b0e9d8c7b6a5f4e3d2c1b0a9f8e7d6c5b\r\ntcp_port:6379\r\n"
	id, ok := parseRedisRunID(info)
	assert.True(t, ok)
	assert.Equal(t, "8f3c2a1b0e9d8c7b6a5f4e3d2c1b0a9f8e7d6c5b", id)

	_, ok = parseRedisRunID("# Server\r\nredis_version:7.2.4\r\n")
	assert.False(t, ok, "没有 run_id 字段")
	_, ok = parseRedisRunID("run_id:\r\n")
	assert.False(t, ok, "run_id 为空")
}

func TestClassifyIndexedMember(t *testing.T) {
	onlineA, err := proto.Marshal(indexTestSession(10, 1, idxGateA, 1))
	require.NoError(t, err)
	onlineB, err := proto.Marshal(indexTestSession(10, 1, idxGateB, 1))
	require.NoError(t, err)
	disc := indexTestSession(10, 1, idxGateA, 2)
	disc.State = pb.PlayerSessionState_SESSION_STATE_DISCONNECTING
	discA, err := proto.Marshal(disc)
	require.NoError(t, err)
	otherOwner, err := proto.Marshal(indexTestSession(11, 1, idxGateA, 1))
	require.NoError(t, err)

	cases := []struct {
		name   string
		member string
		raw    []byte
		found  bool
		want   indexedAction
	}{
		{"online on this gate", "10", onlineA, true, indexedOnlineHere},
		{"online elsewhere", "10", onlineB, true, indexedStale},
		{"disconnecting", "10", discA, true, indexedStale},
		{"session missing", "10", nil, false, indexedMissing},
		{"garbage bytes", "10", []byte{0xff, 0xff, 0xff}, true, indexedMalformed},
		{"payload player mismatch", "10", otherOwner, true, indexedMalformed},
		{"non numeric", "junk", nil, false, indexedBadMember},
		{"zero", "0", nil, false, indexedBadMember},
		{"non canonical", "010", onlineA, true, indexedBadMember},
		{"signed", "+10", onlineA, true, indexedBadMember},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			_, _, got := classifyIndexedMember(tc.member, tc.raw, tc.found, idxGateA)
			assert.Equal(t, tc.want, got)
		})
	}
}

func TestCollectNodeUUIDs(t *testing.T) {
	values := [][]byte{
		gateNodeInfoWithFutureFields(reconcileGateUUIDA),
		[]byte(reconcileGateUUIDA), // allocated/ 占位
		[]byte(reconcileGateUUIDB),
	}
	assert.Equal(t, toSet([]string{reconcileGateUUIDA, reconcileGateUUIDB}), collectNodeUUIDs(values))
}

func TestCapabilityUUIDs(t *testing.T) {
	keys := []string{
		GateSessionIndexCapabilityPrefix + "loc-1",
		GateSessionIndexCapabilityPrefix, // 键尾为空
		"/mmorpg/capability/other/v1/loc-2",
	}
	assert.Equal(t, toSet([]string{"loc-1"}), capabilityUUIDs(keys))
}

func TestLegacyWriters(t *testing.T) {
	assert.Empty(t, legacyWriters(toSet([]string{"a", "b"}), toSet([]string{"a", "b", "c"})),
		"能力键多于注册(正在退出的实例)不算旧写者")
	assert.Equal(t, []string{"b", "d"}, legacyWriters(toSet([]string{"a", "b", "d"}), toSet([]string{"a"})))
}

func TestIndexedGate(t *testing.T) {
	assert.Equal(t, "", indexedGate(nil))
	assert.Equal(t, idxGateA, indexedGate(indexTestSession(1, 1, idxGateA, 1)))
	s := indexTestSession(1, 1, idxGateA, 1)
	s.State = pb.PlayerSessionState_SESSION_STATE_DISCONNECTING
	assert.Equal(t, "", indexedGate(s))
	assert.Equal(t, "", indexedGate(indexTestSession(1, 1, "", 1)))
}
