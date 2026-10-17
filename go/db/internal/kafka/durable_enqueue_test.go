package kafka

// durable_enqueue.go 的单测(S1 修订稿 §9)。
//
//   - WAITAOF 用 miniredis 上注册的假命令 fakeWaitAOF 模拟(miniredis 有 WAIT、没有 WAITAOF):按用例给定的顺序回
//     [numlocal, numreplicas]、错误回复或断链,并记下调用次数、参数与所在连接;不睡眠、不看 timeout,结果与墙钟无关。
//   - 入队是异步的:done 一律用容量为 1 的 channel 接,限时 2s;需要「请求已经在通道里」这一前提的用例
//     (攒批、背压)先把入队层标成已启动、不起写协程,排好请求再起写协程,批次的形成因此是确定的。
//   - 指标是进程级累计值,一律比较前后差值(metricCount 见 instrumentation_test.go)。
//   - 真 Redis 用例以环境变量 GODB_REDIS_ITEST_ADDR 门控,未设即 Skip。

import (
	"bytes"
	"context"
	db_config "db/internal/config"
	"db/internal/metrics"
	"fmt"
	"os"
	db_proto "proto/db"
	"strconv"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/IBM/sarama"
	"github.com/alicebob/miniredis/v2"
	"github.com/alicebob/miniredis/v2/server"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// saveToRetryQueue 是测试种子 helper:把一条任务按 v2 载荷(seq + 原始分区)直接 LPUSH 进重试队列,不经入队层、
// 不发 WAITAOF。生产代码不再用它(Kafka 源入队走 persistBeforeAck + durableEnqueue);保留同名同签名是为了
// placement_route_test.go、instrumentation_test.go 等既有用例不用改。
func saveToRetryQueue(ctx context.Context, redisClient redis.Cmdable, retryQueueKey string, task *db_proto.DBTask, seq uint64, partition int32) error {
	payload, err := encodeRetryPayload(task, seq, partition, true)
	if err != nil {
		return err
	}
	return redisClient.LPush(ctx, retryQueueKey, payload).Err()
}

// ---------------------------------------------------------------------------
// 测试替身与 helper
// ---------------------------------------------------------------------------

// fakeWaitAOFReply 是假 WAITAOF 的一次回复:dropConn 为 true 时不回复、关掉这条连接(模拟 WAITAOF 途中断链,
// 客户端读到 EOF,即网络错误);否则 errText 非空时回错误回复,再否则回 [numLocal, numReplicas]。
type fakeWaitAOFReply struct {
	numLocal    int
	numReplicas int
	errText     string
	dropConn    bool
}

var (
	waitAOFAcked      = fakeWaitAOFReply{numLocal: 1}
	waitAOFNotDurable = fakeWaitAOFReply{}
)

// fakeWaitAOF 按 replies 的顺序回复,用到最后一条后一直回它;replies 为空时回 waitAOFAcked。
// handler 跑在 miniredis 的连接 goroutine 上,所有字段由 mu 保护。
//
// peers 按调用顺序记下每次 WAITAOF 所在的连接(miniredis 每条 TCP 连接一个 *server.Peer):真实 Redis 的 WAITAOF
// 只等本连接自己的写偏移,在没写过的新连接上发会立即回 [1,0](等于假确认),所以「重发的 WAITAOF 还在原连接上」
// 「断链后换了新连接」必须按连接断言,只数调用次数证明不了。
type fakeWaitAOF struct {
	mu      sync.Mutex
	replies []fakeWaitAOFReply
	calls   int
	args    [][]string
	peers   []*server.Peer
}

func installFakeWaitAOF(t *testing.T, mr *miniredis.Miniredis, replies ...fakeWaitAOFReply) *fakeWaitAOF {
	t.Helper()
	f := &fakeWaitAOF{replies: replies}
	require.NoError(t, mr.Server().Register("WAITAOF", f.handle))
	return f
}

func (f *fakeWaitAOF) handle(c *server.Peer, _ string, args []string) {
	f.mu.Lock()
	f.calls++
	f.args = append(f.args, append([]string(nil), args...))
	f.peers = append(f.peers, c)
	reply := waitAOFAcked
	if len(f.replies) > 0 {
		reply = f.replies[0]
		if len(f.replies) > 1 {
			f.replies = f.replies[1:]
		}
	}
	f.mu.Unlock()
	if reply.dropConn {
		// miniredis 在本命令处理完、flush 之后关闭连接:同一 pipeline 里前面 LPUSH 的回复已经送出,WAITAOF 读到 EOF。
		c.Close()
		return
	}
	if reply.errText != "" {
		c.WriteError(reply.errText)
		return
	}
	c.Block(func(w *server.Writer) {
		w.WriteLen(2)
		w.WriteInt(reply.numLocal)
		w.WriteInt(reply.numReplicas)
	})
}

// setReplies 换一组回复并清零调用记录。
func (f *fakeWaitAOF) setReplies(replies ...fakeWaitAOFReply) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.replies = append([]fakeWaitAOFReply(nil), replies...)
	f.calls = 0
	f.args = nil
	f.peers = nil
}

// peersSeen 返回每次 WAITAOF 所在的连接,按调用顺序。
func (f *fakeWaitAOF) peersSeen() []*server.Peer {
	f.mu.Lock()
	defer f.mu.Unlock()
	return append([]*server.Peer(nil), f.peers...)
}

func (f *fakeWaitAOF) callCount() int {
	f.mu.Lock()
	defer f.mu.Unlock()
	return f.calls
}

func (f *fakeWaitAOF) lastArgs() []string {
	f.mu.Lock()
	defer f.mu.Unlock()
	if len(f.args) == 0 {
		return nil
	}
	return f.args[len(f.args)-1]
}

// newTestAOFGroup 用生产同款的专用客户端参数连 miniredis 建组提交入队层(未启动),t.Cleanup 里限时 close。
// cleanup 也必须限时:用例里的 close 卡住时 closeOnce.Do 会让 cleanup 的 close 跟着等,不限时就又挂回整个测试二进制。
func newTestAOFGroup(t *testing.T, mr *miniredis.Miniredis, ack aofAck) *aofGroupEnqueue {
	t.Helper()
	q := newAOFGroupEnqueue(redis.NewClient(durableClientOptions(mr.Addr(), "", 0, ack)), ack)
	t.Cleanup(func() {
		if !closeAOFGroupWithin(q, 5*time.Second) {
			t.Error("durable enqueue close did not return within 5s during cleanup")
		}
	})
	return q
}

// closeAOFGroupWithin 在 goroutine 里 close,限时等它返回:关闭协议回归(顺序改错、等写协程或 submitter 时死锁)
// 在本用例里快速失败、给出定位,而不是把整个测试二进制挂到 go test 默认的 -timeout。超时后那个 goroutine 泄漏,
// 用例已判失败。
func closeAOFGroupWithin(q *aofGroupEnqueue, d time.Duration) bool {
	closed := make(chan struct{})
	go func() {
		q.close()
		close(closed)
	}()
	return closedWithin(closed, d)
}

// startTestAOFGroup 让启动探针拿到确认后 start,再把假 WAITAOF 换成 then(并清零调用次数)。
func startTestAOFGroup(t *testing.T, q *aofGroupEnqueue, fake *fakeWaitAOF, then ...fakeWaitAOFReply) {
	t.Helper()
	fake.setReplies(waitAOFAcked)
	require.NoError(t, q.start(context.Background()))
	fake.setReplies(then...)
}

// markStartedWithoutRun 把入队层标成已启动、先不起写协程,让用例先把请求排进通道,再确定性地起写协程。
// 返回的 launch 只生效一次;t.Cleanup 兜底调用它(注册晚于 newTestAOFGroup 的 close,所以先于 close 执行),
// 否则 close 会一直等一个从没起过的写协程。
func markStartedWithoutRun(t *testing.T, q *aofGroupEnqueue) (launch func()) {
	t.Helper()
	q.mu.Lock()
	q.started = true
	q.mu.Unlock()
	var once sync.Once
	launch = func() { once.Do(func() { go q.run() }) }
	t.Cleanup(launch)
	return launch
}

// waitEnqueueResult 限时 2s 等一次 done。
func waitEnqueueResult(t *testing.T, results <-chan error) error {
	t.Helper()
	select {
	case err := <-results:
		return err
	case <-time.After(2 * time.Second):
		t.Fatal("durable enqueue did not complete within 2s")
		return nil
	}
}

// submitAndWait 提交一条并等它的 done。
func submitAndWait(t *testing.T, q durableEnqueue, ctx context.Context, target, queueKey string, payload []byte) error {
	t.Helper()
	results := make(chan error, 1)
	q.submit(ctx, target, queueKey, payload, func(err error) { results <- err })
	return waitEnqueueResult(t, results)
}

// waitAckerFailure 限时 2s 等 claimAcker 的失败通知(failKafkaTask → acker.fail)。
func waitAckerFailure(t *testing.T, acker *claimAcker) error {
	t.Helper()
	select {
	case err := <-acker.failureCh:
		return err
	case <-time.After(2 * time.Second):
		t.Fatal("the claim was not failed within 2s")
		return nil
	}
}

func waitGroupWithin(t *testing.T, wg *sync.WaitGroup, d time.Duration) {
	t.Helper()
	done := make(chan struct{})
	go func() {
		wg.Wait()
		close(done)
	}()
	require.True(t, closedWithin(done, d), "goroutines did not finish within %v", d)
}

// listOf 直接读 miniredis 里的列表(不经命令,不受 SetError 影响);键不存在返回 nil。
func listOf(t *testing.T, mr *miniredis.Miniredis, key string) []string {
	t.Helper()
	if !mr.Exists(key) {
		return nil
	}
	vals, err := mr.List(key)
	require.NoError(t, err)
	return vals
}

func durableResultCount(t *testing.T, target, result string) float64 {
	t.Helper()
	return metricCount(t, "db_durable_enqueue_total", map[string]string{"target": target, "result": result})
}

// durableSecondsCount 是 db_durable_enqueue_seconds{target} 的样本数。
func durableSecondsCount(t *testing.T, target string) float64 {
	t.Helper()
	return metricCount(t, "db_durable_enqueue_seconds", map[string]string{"target": target})
}

// heldEnqueue 是 holdingEnqueue 扣下的一次 submit;release 只生效一次。
type heldEnqueue struct {
	target   string
	queueKey string
	payload  []byte
	done     func(error)
	once     sync.Once
}

func (h *heldEnqueue) release(err error) {
	h.once.Do(func() { h.done(err) })
}

// holdingEnqueue 实现 durableEnqueue:submit 只记下请求、不调 done,由用例决定何时、以什么结果放行。
// close 把还没放行的以 canceled 放行(满足「close 返回时所有 done 都已调用过」)。
type holdingEnqueue struct {
	mu   sync.Mutex
	held []*heldEnqueue
}

func (f *holdingEnqueue) submit(_ context.Context, target, queueKey string, payload []byte, done func(error)) {
	f.mu.Lock()
	defer f.mu.Unlock()
	f.held = append(f.held, &heldEnqueue{
		target: target, queueKey: queueKey, payload: append([]byte(nil), payload...), done: done,
	})
}

func (f *holdingEnqueue) start(context.Context) error { return nil }

func (f *holdingEnqueue) close() {
	for _, h := range f.requests() {
		h.release(errDurableEnqueueClosed)
	}
}

func (f *holdingEnqueue) requests() []*heldEnqueue {
	f.mu.Lock()
	defer f.mu.Unlock()
	return append([]*heldEnqueue(nil), f.held...)
}

// attachSharedKafkaClaim 把几条任务挂到同一个 claimAcker 上(offset = seq-1,按给定顺序 track),
// 用来断言「低位未确认时,高位完成也不得越过它标记位点」。
func attachSharedKafkaClaim(t *testing.T, w *worker, tasks ...*workerTask) (*recordingSession, *claimAcker) {
	t.Helper()
	require.NotEmpty(t, tasks)
	claimCtx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)
	session := &recordingSession{ctx: claimCtx}
	acker := newClaimAcker(session, int64(tasks[0].seq)-1, cancel)
	for _, wt := range tasks {
		msg := &sarama.ConsumerMessage{Topic: w.topic, Partition: wt.originPartition, Offset: int64(wt.seq) - 1}
		acker.track(msg)
		wt.kafkaMsg = msg
		wt.acker = acker
		wt.claimCtx = claimCtx
	}
	return session, acker
}

// ---------------------------------------------------------------------------
// 纯函数
// ---------------------------------------------------------------------------

func TestCheckWaitAOFReply(t *testing.T) {
	cases := []struct {
		name           string
		vals           []any
		numLocal       int
		numReplicas    int
		wantErr        bool
		wantUnexpected bool
	}{
		{name: "one element is the wrong shape", vals: []any{int64(1)}, numLocal: 1, wantErr: true, wantUnexpected: true},
		{name: "three elements are the wrong shape", vals: []any{int64(1), int64(0), int64(0)}, numLocal: 1, wantErr: true, wantUnexpected: true},
		{name: "non-integer element is the wrong shape", vals: []any{"1", int64(0)}, numLocal: 1, wantErr: true, wantUnexpected: true},
		{name: "negative local count is the wrong shape", vals: []any{int64(-1), int64(0)}, numLocal: 1, wantErr: true, wantUnexpected: true},
		{name: "negative replica count is the wrong shape", vals: []any{int64(1), int64(-1)}, numLocal: 1, wantErr: true, wantUnexpected: true},
		{name: "local fsync missing", vals: []any{int64(0), int64(0)}, numLocal: 1, wantErr: true},
		{name: "replica missing", vals: []any{int64(1), int64(0)}, numLocal: 1, numReplicas: 1, wantErr: true},
		{name: "local satisfied", vals: []any{int64(1), int64(0)}, numLocal: 1},
		{name: "more replicas than required", vals: []any{int64(1), int64(2)}, numLocal: 1, numReplicas: 1},
		{name: "nothing required", vals: []any{int64(0), int64(0)}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			err := checkWaitAOFReply(tc.vals, tc.numLocal, tc.numReplicas)
			if !tc.wantErr {
				assert.NoError(t, err)
				return
			}
			require.Error(t, err)
			if tc.wantUnexpected {
				assert.ErrorIs(t, err, errUnexpectedWaitAOFReply)
			} else {
				assert.NotErrorIs(t, err, errUnexpectedWaitAOFReply, "too few copies is retryable, not a shape error")
			}
		})
	}
}

func TestWaitAOFBudgetMs(t *testing.T) {
	base := time.Unix(1_800_000_000, 0)
	const configured = 1500 * time.Millisecond
	const margin = 100 * time.Millisecond
	cases := []struct {
		name        string
		deadline    time.Time
		hasDeadline bool
		configured  time.Duration
		wantMs      int64
		wantOK      bool
	}{
		{name: "no deadline uses the configured timeout", configured: configured, wantMs: 1500, wantOK: true},
		{name: "far deadline uses the configured timeout", deadline: base.Add(10 * time.Second), hasDeadline: true,
			configured: configured, wantMs: 1500, wantOK: true},
		{name: "near deadline caps at remaining minus margin", deadline: base.Add(600 * time.Millisecond), hasDeadline: true,
			configured: configured, wantMs: 500, wantOK: true},
		{name: "margin is subtracted", deadline: base.Add(150 * time.Millisecond), hasDeadline: true,
			configured: configured, wantMs: 50, wantOK: true},
		{name: "truncated to whole milliseconds before the check", deadline: base.Add(margin + 1900*time.Microsecond),
			hasDeadline: true, configured: configured, wantMs: 1, wantOK: true},
		{name: "below one millisecond is never sent", deadline: base.Add(margin + 900*time.Microsecond),
			hasDeadline: true, configured: configured, wantOK: false},
		{name: "past deadline is never sent", deadline: base.Add(-time.Second), hasDeadline: true,
			configured: configured, wantOK: false},
		{name: "zero configured timeout means block forever and is never sent", configured: 0, wantOK: false},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			ms, ok := waitAOFBudgetMs(base, tc.deadline, tc.hasDeadline, tc.configured, margin)
			assert.Equal(t, tc.wantOK, ok)
			if tc.wantOK {
				assert.Equal(t, tc.wantMs, ms)
			} else {
				assert.Zero(t, ms)
			}
		})
	}
}

// ---------------------------------------------------------------------------
// 配置与探针
// ---------------------------------------------------------------------------

func TestNewDurableEnqueue_FollowsConfig(t *testing.T) {
	mr := miniredis.RunT(t)
	mainPool := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = mainPool.Close() })
	zero, one := 0, 1

	disabled := newDurableEnqueue(db_config.RedisConfig{
		Hosts: mr.Addr(), WaitAOFNumLocal: &zero, WaitAOFNumReplicas: &zero, WaitAOFTimeoutMs: 1500,
	}, mainPool)
	direct, ok := disabled.(directEnqueue)
	require.True(t, ok, "both counts written 0 falls back to LPUSH-only, got %T", disabled)
	assert.Equal(t, redis.Cmdable(mainPool), direct.rc, "the LPUSH-only fallback writes through the main pool")

	enabled := newDurableEnqueue(db_config.RedisConfig{
		Hosts: mr.Addr(), DB: 3, WaitAOFNumLocal: &one, WaitAOFNumReplicas: &zero, WaitAOFTimeoutMs: 1200,
	}, mainPool)
	q, ok := enabled.(*aofGroupEnqueue)
	require.True(t, ok, "WAITAOF enabled builds the group-commit enqueue, got %T", enabled)
	t.Cleanup(q.close)
	assert.Equal(t, aofAck{numLocal: 1, numReplicas: 0, timeout: 1200 * time.Millisecond}, q.ack)
	assert.NotSame(t, mainPool, q.rc, "WAITAOF must run on a dedicated client, never on the main pool")
	opt := q.rc.Options()
	assert.Equal(t, mr.Addr(), opt.Addr)
	assert.Equal(t, 3, opt.DB)
	assert.Equal(t, durableClientPoolSize, opt.PoolSize)
	assert.Equal(t, 2200*time.Millisecond, opt.ReadTimeout, "read timeout = WaitAOFTimeoutMs + 1s")
	assert.Equal(t, durableClientWriteTimeout, opt.WriteTimeout)
	assert.True(t, opt.DisableIndentity)

	// MaxRetries=-1 在 go-redis 里会被归一成 0(不重试),所以直接看参数构造函数的输出。
	assert.Equal(t, -1, durableClientOptions("127.0.0.1:1", "", 0, q.ack).MaxRetries,
		"go-redis must not resend a whole pipeline on its own")
}

func TestProbeAOFAck(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rc.Close() })
	ctx := context.Background()
	ack := aofAck{numLocal: 1, timeout: 1500 * time.Millisecond}

	require.NoError(t, probeAOFAck(ctx, rc, aofAck{timeout: 1500 * time.Millisecond}))
	assert.Zero(t, fake.callCount(), "a disabled probe sends no command")

	fake.setReplies(waitAOFAcked)
	require.NoError(t, probeAOFAck(ctx, rc, ack))
	assert.Equal(t, []string{"1", "0", "1500"}, fake.lastArgs(), "the probe waits with the configured timeout")

	fake.setReplies(waitAOFNotDurable)
	assert.Error(t, probeAOFAck(ctx, rc, ack), "no local fsync acknowledged must refuse to start")

	fake.setReplies(waitAOFAcked)
	assert.Error(t, probeAOFAck(ctx, rc, aofAck{numLocal: 1, numReplicas: 1, timeout: time.Second}),
		"a configured replica that never acknowledges must refuse to start")

	unknown := miniredis.RunT(t)
	rcUnknown := redis.NewClient(&redis.Options{Addr: unknown.Addr()})
	t.Cleanup(func() { _ = rcUnknown.Close() })
	err := probeAOFAck(ctx, rcUnknown, ack)
	require.Error(t, err, "a Redis without WAITAOF must refuse to start")
	assert.Contains(t, err.Error(), "appendonly", "the error tells the operator what to check")
}

// ---------------------------------------------------------------------------
// directEnqueue
// ---------------------------------------------------------------------------

func TestDirectEnqueue_LPushOnly(t *testing.T) {
	mr := miniredis.RunT(t)
	require.False(t, mr.Server().IsRegisteredCommand("WAITAOF"),
		"miniredis must not know WAITAOF, so a successful push proves none was sent")
	rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rc.Close() })
	q := directEnqueue{rc: rc}
	const key = "kafka:retry:queue:test-direct"

	var got error
	calls := 0
	record := func(err error) {
		calls++
		got = err
	}

	q.submit(context.Background(), metrics.DurableTargetRetry, key, []byte("first"), record)
	require.Equal(t, 1, calls, "directEnqueue completes before submit returns")
	require.NoError(t, got)
	assert.Equal(t, []string{"first"}, listOf(t, mr, key))

	canceled, cancel := context.WithCancel(context.Background())
	cancel()
	calls = 0
	q.submit(canceled, metrics.DurableTargetRetry, key, []byte("second"), record)
	require.Equal(t, 1, calls)
	assert.ErrorIs(t, got, errDurableEnqueueCanceled)
	assert.ErrorIs(t, got, context.Canceled)
	assert.Equal(t, []string{"first"}, listOf(t, mr, key), "a canceled ctx sends nothing")

	// LPUSH 失败后在退避等待中 ctx 到期:立即以 canceled 返回,不等满 0/100ms/500ms/1s。
	mr.SetError("ERR injected redis failure")
	expiring, cancelExpiring := context.WithTimeout(context.Background(), 50*time.Millisecond)
	defer cancelExpiring()
	calls = 0
	started := time.Now()
	q.submit(expiring, metrics.DurableTargetRetry, key, []byte("third"), record)
	elapsed := time.Since(started)
	mr.SetError("")
	require.Equal(t, 1, calls)
	assert.ErrorIs(t, got, errDurableEnqueueCanceled)
	assert.ErrorIs(t, got, context.DeadlineExceeded)
	assert.Less(t, elapsed, time.Second, "the backoff wait must honor ctx")
	assert.Equal(t, []string{"first"}, listOf(t, mr, key))
}

// 停机时的取消计 canceled、不计 error,也不进直方图:否则每次滚动都会误触 DbDurableEnqueueNotConfirmed。
func TestDurableEnqueue_CanceledCtxCountsCanceled(t *testing.T) {
	check := func(t *testing.T, q durableEnqueue, mr *miniredis.Miniredis, key string) {
		t.Helper()
		target := metrics.DurableTargetDead
		beforeCanceled := durableResultCount(t, target, metrics.DurableCanceled)
		beforeError := durableResultCount(t, target, metrics.DurableError)
		beforeSeconds := durableSecondsCount(t, target)

		ctx, cancel := context.WithCancel(context.Background())
		cancel()
		err := submitAndWait(t, q, ctx, target, key, []byte("canceled"))

		assert.ErrorIs(t, err, errDurableEnqueueCanceled)
		assert.ErrorIs(t, err, context.Canceled)
		assert.Equal(t, beforeCanceled+1, durableResultCount(t, target, metrics.DurableCanceled))
		assert.Equal(t, beforeError, durableResultCount(t, target, metrics.DurableError), "canceled is not an error")
		assert.Equal(t, beforeSeconds, durableSecondsCount(t, target), "canceled is not observed in the histogram")
		assert.Empty(t, listOf(t, mr, key))
	}

	t.Run("direct", func(t *testing.T) {
		mr := miniredis.RunT(t)
		rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})
		t.Cleanup(func() { _ = rc.Close() })
		check(t, directEnqueue{rc: rc}, mr, "kafka:dead:queue:test-canceled-direct")
	})
	t.Run("group", func(t *testing.T) {
		mr := miniredis.RunT(t)
		fake := installFakeWaitAOF(t, mr)
		q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 1500 * time.Millisecond})
		startTestAOFGroup(t, q, fake)
		check(t, q, mr, "kafka:dead:queue:test-canceled-group")
		assert.Zero(t, fake.callCount(), "a canceled submit never reaches the write goroutine")
	})
}

// ---------------------------------------------------------------------------
// aofGroupEnqueue
// ---------------------------------------------------------------------------

func TestAOFGroupEnqueue_ConfirmedBatch(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 1500 * time.Millisecond})
	startTestAOFGroup(t, q, fake)
	const key = "kafka:retry:queue:test-confirmed"
	target := metrics.DurableTargetRetry
	beforeOK := durableResultCount(t, target, metrics.DurableOK)
	beforeSeconds := durableSecondsCount(t, target)

	const n = 50
	results := make(chan error, n)
	want := make([]string, 0, n)
	var wg sync.WaitGroup
	for i := 0; i < n; i++ {
		payload := fmt.Sprintf("payload-%02d", i)
		want = append(want, payload)
		wg.Add(1)
		go func() {
			defer wg.Done()
			q.submit(context.Background(), target, key, []byte(payload), func(err error) { results <- err })
		}()
	}
	waitGroupWithin(t, &wg, 2*time.Second)
	for i := 0; i < n; i++ {
		require.NoError(t, waitEnqueueResult(t, results))
	}

	assert.ElementsMatch(t, want, listOf(t, mr, key), "every confirmed copy is in Redis exactly once")
	assert.Equal(t, beforeOK+n, durableResultCount(t, target, metrics.DurableOK))
	assert.Equal(t, beforeSeconds+n, durableSecondsCount(t, target))
	calls := fake.callCount()
	assert.GreaterOrEqual(t, calls, 1)
	assert.LessOrEqual(t, calls, n, "requests are confirmed in groups, never more than one WAITAOF each")
	args := fake.lastArgs()
	require.Len(t, args, 3)
	assert.Equal(t, "1", args[0], "numlocal")
	assert.Equal(t, "0", args[1], "numreplicas")
	waitMs, err := strconv.Atoi(args[2])
	require.NoError(t, err)
	assert.GreaterOrEqual(t, waitMs, 1, "WAITAOF timeout 0 would block forever")
	assert.LessOrEqual(t, waitMs, 1500)
}

func TestAOFGroupEnqueue_NotDurableWithinBudget(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 50 * time.Millisecond, budget: 300 * time.Millisecond})
	startTestAOFGroup(t, q, fake, waitAOFNotDurable)
	const key = "kafka:retry:queue:test-not-durable"
	target := metrics.DurableTargetRetry
	before := durableResultCount(t, target, metrics.DurableNotDurable)

	started := time.Now()
	err := submitAndWait(t, q, context.Background(), target, key, []byte("short-of-copies"))

	require.ErrorIs(t, err, errEnqueueNotDurable)
	assert.NotErrorIs(t, err, errDurableEnqueueCanceled, "an exhausted budget must never be counted as canceled")
	assert.Less(t, time.Since(started), time.Second, "fail-closed within the batch budget")
	assert.Equal(t, before+1, durableResultCount(t, target, metrics.DurableNotDurable))
	assert.GreaterOrEqual(t, fake.callCount(), 1)
	assert.Len(t, listOf(t, mr, key), 1, "only WAITAOF is resent while copies are short; the LPUSH is not repeated")
}

func TestAOFGroupEnqueue_WaitAOFErrorReplyNotRetried(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 1500 * time.Millisecond})
	startTestAOFGroup(t, q, fake,
		fakeWaitAOFReply{errText: "ERR WAITAOF cannot be used when numlocal is set but appendonly is disabled."})
	target := metrics.DurableTargetDead
	before := durableResultCount(t, target, metrics.DurableError)

	err := submitAndWait(t, q, context.Background(), target, "kafka:dead:queue:test-rejected", []byte("x"))

	require.ErrorIs(t, err, errWaitAOFRejected)
	assert.Equal(t, 1, fake.callCount(), "a deterministic WAITAOF rejection is not retried")
	assert.Equal(t, before+1, durableResultCount(t, target, metrics.DurableError))
}

func TestAOFGroupEnqueue_NotDurableResendsOnlyWaitAOF(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 1500 * time.Millisecond})
	startTestAOFGroup(t, q, fake, waitAOFNotDurable, waitAOFAcked)
	const key = "kafka:retry:queue:test-resend-wait-only"

	require.NoError(t, submitAndWait(t, q, context.Background(), metrics.DurableTargetRetry, key, []byte("only-once")))

	assert.Equal(t, 2, fake.callCount(), "the first WAITAOF was short, the second one confirmed")
	assert.Equal(t, []string{"only-once"}, listOf(t, mr, key),
		"same sticky connection, same write offset: the copy is pushed exactly once")
	peers := fake.peersSeen()
	require.Len(t, peers, 2)
	assert.Same(t, peers[0], peers[1],
		"WAITAOF must stay on the connection that ran this batch's LPUSH: on a fresh connection it acks [1,0] at once without covering the write")
}

// WAITAOF 途中断链(网络错误):粘连连接作废,未确认的请求在新连接上重推(LPUSH + WAITAOF),确认后以 nil 完成。
// 断链之前那次 LPUSH 已经写进列表,重推产生的重复按设计可以接受(游标与 seq 吸收)。
func TestAOFGroupEnqueue_NetworkErrorRepushesOnNewConnection(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 1500 * time.Millisecond})
	startTestAOFGroup(t, q, fake, fakeWaitAOFReply{dropConn: true}, waitAOFAcked)
	const key = "kafka:retry:queue:test-network-repush"

	require.NoError(t, submitAndWait(t, q, context.Background(), metrics.DurableTargetRetry, key, []byte("repushed")),
		"a network error within the budget is retried on a new connection, not failed")

	assert.Equal(t, 2, fake.callCount(), "the first WAITAOF lost its connection, the second one confirmed")
	peers := fake.peersSeen()
	require.Len(t, peers, 2)
	assert.NotSame(t, peers[0], peers[1],
		"after a network error the batch must move to a new connection: WAITAOF must run on the same connection as this batch's LPUSH")
	assert.Equal(t, []string{"repushed", "repushed"}, listOf(t, mr, key),
		"the new connection cannot cover the old connection's write, so the copy is pushed again there")
}

func TestAOFGroupEnqueue_SubmitAfterCloseCountsCanceled(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 1500 * time.Millisecond})
	startTestAOFGroup(t, q, fake)
	require.True(t, closeAOFGroupWithin(q, 2*time.Second), "closing an idle started enqueue must return")
	const key = "kafka:retry:queue:test-after-close"
	target := metrics.DurableTargetRetry
	beforeCanceled := durableResultCount(t, target, metrics.DurableCanceled)
	beforeError := durableResultCount(t, target, metrics.DurableError)

	err := submitAndWait(t, q, context.Background(), target, key, []byte("late"))

	assert.ErrorIs(t, err, errDurableEnqueueCanceled)
	assert.Equal(t, beforeCanceled+1, durableResultCount(t, target, metrics.DurableCanceled))
	assert.Equal(t, beforeError, durableResultCount(t, target, metrics.DurableError))
	assert.Empty(t, listOf(t, mr, key))
	assert.Zero(t, fake.callCount())
}

func TestAOFGroupEnqueue_SubmitBeforeStartFails(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 1500 * time.Millisecond})
	const key = "kafka:retry:queue:test-not-started"
	target := metrics.DurableTargetRetry
	before := durableResultCount(t, target, metrics.DurableError)

	err := submitAndWait(t, q, context.Background(), target, key, []byte("early"))

	assert.ErrorIs(t, err, errDurableEnqueueNotStarted)
	assert.NotErrorIs(t, err, errDurableEnqueueCanceled)
	assert.Equal(t, before+1, durableResultCount(t, target, metrics.DurableError))
	assert.Empty(t, listOf(t, mr, key))
	assert.Zero(t, fake.callCount())

	assert.True(t, closeAOFGroupWithin(q, 2*time.Second), "closing a never-started enqueue must not wait for a write goroutine")
}

// 32 个 goroutine 并发 submit 与 close 赛跑:每个 done 恰好一次;close 开始之前已经返回的 submit,
// 在 close 返回时 done 都已调用过。假 WAITAOF 一直回份数不足、待确认字节只放得下 4 条,所以 close 时
// 既有卡在 flush 重试里的批次,也有排在通道里的请求和卡在背压上的 submitter。
func TestAOFGroupEnqueue_CloseCompletesEveryRequestOnce(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	const payloadSize = 64
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 50 * time.Millisecond})
	q.maxPendingBytes = 4 * payloadSize
	startTestAOFGroup(t, q, fake, waitAOFNotDurable)
	const key = "kafka:retry:queue:test-close-race"

	const goroutines, perGoroutine = 32, 8
	const total = goroutines * perGoroutine
	doneCounts := make([]atomic.Int32, total)
	returnedBeforeClose := make([]atomic.Bool, total)
	var closeStarted atomic.Bool
	var returned atomic.Int32
	var wg sync.WaitGroup
	for g := 0; g < goroutines; g++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			for j := 0; j < perGoroutine; j++ {
				id := g*perGoroutine + j
				payload := bytes.Repeat([]byte{byte(id)}, payloadSize)
				q.submit(context.Background(), metrics.DurableTargetRetry, key, payload, func(error) {
					doneCounts[id].Add(1)
				})
				if !closeStarted.Load() {
					returnedBeforeClose[id].Store(true)
				}
				returned.Add(1)
			}
		}()
	}

	require.Eventually(t, func() bool { return returned.Load() >= 4 }, 2*time.Second, time.Millisecond,
		"the pending-byte budget admits four requests before backpressure")
	closeStarted.Store(true)
	require.True(t, closeAOFGroupWithin(q, 2*time.Second), "close must complete every pending request and return")
	// close 已经返回:close 开始之前返回的 submit,done 必须都已调用过(恰好一次)。
	for id := 0; id < total; id++ {
		if returnedBeforeClose[id].Load() {
			assert.Equalf(t, int32(1), doneCounts[id].Load(),
				"request %d was submitted before close; close must have completed it", id)
		}
	}

	waitGroupWithin(t, &wg, 2*time.Second)
	for id := 0; id < total; id++ {
		assert.Equalf(t, int32(1), doneCounts[id].Load(), "request %d: done must be called exactly once", id)
	}
}

// 字节上限 1KiB、三条各 600B:前两条凑满一批(600 < 1024 时再取一条,到 1200 停),第三条单独成批。
func TestAOFGroupEnqueue_BatchByteCap(t *testing.T) {
	mr := miniredis.RunT(t)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 1500 * time.Millisecond})
	q.maxBatchBytes = 1 << 10
	launch := markStartedWithoutRun(t, q)
	const key = "kafka:retry:queue:test-batch-bytes"

	const n = 3
	results := make(chan error, n)
	for i := 0; i < n; i++ {
		q.submit(context.Background(), metrics.DurableTargetRetry, key, bytes.Repeat([]byte{byte('a' + i)}, 600),
			func(err error) { results <- err })
	}
	launch()
	for i := 0; i < n; i++ {
		require.NoError(t, waitEnqueueResult(t, results))
	}

	assert.Equal(t, 2, fake.callCount(), "the byte cap splits three 600B copies into two groups")
	assert.Len(t, listOf(t, mr, key), n)
}

func TestAOFGroupEnqueue_PendingBytesBackpressure(t *testing.T) {
	mr := miniredis.RunT(t)
	installFakeWaitAOF(t, mr)
	ack := aofAck{numLocal: 1, timeout: 1500 * time.Millisecond}
	payload := make([]byte, 800)

	t.Run("second submit waits until the first copy completes", func(t *testing.T) {
		q := newTestAOFGroup(t, mr, ack)
		q.maxPendingBytes = 1000
		launch := markStartedWithoutRun(t, q)
		const key = "kafka:retry:queue:test-backpressure"

		first := make(chan error, 1)
		q.submit(context.Background(), metrics.DurableTargetRetry, key, payload, func(err error) { first <- err })
		second := make(chan error, 1)
		secondReturned := make(chan struct{})
		go func() {
			q.submit(context.Background(), metrics.DurableTargetRetry, key, payload, func(err error) { second <- err })
			close(secondReturned)
		}()

		// 写协程还没起:第一条一直未完成,第二条只能卡在背压上。(完成时先释放字节再调 done,所以第二条返回与
		// 第一条的 done 谁先谁后不确定,这里只断言「第一条完成之前第二条不返回」。)
		assert.False(t, closedWithin(secondReturned, 100*time.Millisecond),
			"800 of 1000 pending bytes are taken: the second 800B submit must block")
		select {
		case err := <-first:
			t.Fatalf("the first copy cannot complete before the write goroutine runs, got %v", err)
		default:
		}
		launch()
		require.NoError(t, waitEnqueueResult(t, first))
		require.True(t, closedWithin(secondReturned, 2*time.Second), "completing the first copy frees the budget")
		require.NoError(t, waitEnqueueResult(t, second))
		assert.Len(t, listOf(t, mr, key), 2)
	})

	t.Run("caller ctx canceled while waiting counts canceled", func(t *testing.T) {
		q := newTestAOFGroup(t, mr, ack)
		q.maxPendingBytes = 1000
		launch := markStartedWithoutRun(t, q)
		const key = "kafka:retry:queue:test-backpressure-cancel"
		target := metrics.DurableTargetRetry
		before := durableResultCount(t, target, metrics.DurableCanceled)

		first := make(chan error, 1)
		q.submit(context.Background(), target, key, payload, func(err error) { first <- err })
		ctx, cancel := context.WithCancel(context.Background())
		defer cancel()
		second := make(chan error, 1)
		go q.submit(ctx, target, key, payload, func(err error) { second <- err })
		select {
		case err := <-second:
			t.Fatalf("the second submit must wait for pending bytes, got %v", err)
		case <-time.After(50 * time.Millisecond):
		}
		cancel()

		err := waitEnqueueResult(t, second)
		assert.ErrorIs(t, err, errDurableEnqueueCanceled)
		assert.ErrorIs(t, err, context.Canceled)
		assert.Equal(t, before+1, durableResultCount(t, target, metrics.DurableCanceled))

		launch()
		require.NoError(t, waitEnqueueResult(t, first))
		assert.Len(t, listOf(t, mr, key), 1, "the canceled copy never reached Redis")
	})
}

// ---------------------------------------------------------------------------
// 与 KOC 的整合:Kafka 源入队的 ack 推迟到确认之后
// ---------------------------------------------------------------------------

// offset 0 失败、副本交给入队层后确认迟迟不来:handleTask 立即返回,offset 1 照常执行;位点停在 0 之前,
// 确认回来后 [0, 1] 一次推进。
func TestDeferFailedTask_AsyncEnqueueDoesNotBlockNextTask(t *testing.T) {
	w, _, h := newTestWorker(t, 0)
	held := &holdingEnqueue{}
	t.Cleanup(held.close)
	w.durable = held

	const failingKey, healthyKey uint64 = 9821, 9822
	failing := makeWriteTask(t, failingKey, placementTestMsgType, 1) // offset 0
	healthy := makeWriteTask(t, healthyKey, placementTestMsgType, 2) // offset 1
	session, _ := attachSharedKafkaClaim(t, w, failing, healthy)
	h.programFailure(failing.dbTask.TaskId, 1)

	w.handleTask(failing, true)
	reqs := held.requests()
	require.Len(t, reqs, 1, "the failed Kafka task hands exactly one copy to the enqueue layer")
	assert.Equal(t, metrics.DurableTargetRetry, reqs[0].target)
	assert.Equal(t, w.retryQueueKey, reqs[0].queueKey, "shared routing writes the shared ready list")
	seq, partition, hasPartition, _ := unwrapRetryPayload(reqs[0].payload)
	assert.Equal(t, uint64(1), seq)
	assert.True(t, hasPartition)
	assert.Equal(t, int32(0), partition)

	w.handleTask(healthy, true)
	require.Len(t, h.callsForKey(healthyKey), 1, "an unconfirmed enqueue must not block the next task")
	assert.Empty(t, session.marked(), "offset 1 must not be marked past the unconfirmed offset 0")
	assert.NoError(t, failing.claimCtx.Err(), "the claim stays alive while the copy is being confirmed")

	reqs[0].release(nil)
	assert.Equal(t, []int64{0, 1}, session.marked(), "the confirmation advances the contiguous prefix at once")
}

// 确认失败(errEnqueueNotDurable):claim 失败、claimCtx 取消,已完成的 offset 1 与之后的任务都不再标记。
func TestDeferFailedTask_ConfirmFailureFailsClaimAndBlocksLaterOffsets(t *testing.T) {
	w, _, h := newTestWorker(t, 0)
	held := &holdingEnqueue{}
	t.Cleanup(held.close)
	w.durable = held

	const failingKey, healthyKey, laterKey uint64 = 9831, 9832, 9833
	failing := makeWriteTask(t, failingKey, placementTestMsgType, 1) // offset 0
	healthy := makeWriteTask(t, healthyKey, placementTestMsgType, 2) // offset 1
	later := makeWriteTask(t, laterKey, placementTestMsgType, 3)     // offset 2
	session, acker := attachSharedKafkaClaim(t, w, failing, healthy, later)
	h.programFailure(failing.dbTask.TaskId, 1)

	w.handleTask(failing, true)
	w.handleTask(healthy, true)
	require.Len(t, h.callsForKey(healthyKey), 1)
	reqs := held.requests()
	require.Len(t, reqs, 1)

	reqs[0].release(fmt.Errorf("%w: injected", errEnqueueNotDurable))
	select {
	case err := <-acker.failureCh:
		assert.ErrorIs(t, err, errEnqueueNotDurable)
	default:
		t.Fatal("an unconfirmed copy must fail the claim")
	}
	assert.Error(t, failing.claimCtx.Err(), "the failed claim cancels all of its queued work")
	assert.Empty(t, session.marked(), "no offset may be marked once the hole at offset 0 failed")

	w.handleTask(later, true)
	assert.Empty(t, h.callsForKey(laterKey), "queued work of a failed claim must not reach the DB")
	assert.Empty(t, session.marked())
}

// 组提交确认不足:Kafka 源失败任务的 offset 不 ack,claim 失败。
func TestDeferFailedTask_KafkaNotAckedWhenEnqueueNotDurable(t *testing.T) {
	w, mr, h := newTestWorker(t, 0)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 50 * time.Millisecond, budget: 300 * time.Millisecond})
	startTestAOFGroup(t, q, fake, waitAOFNotDurable)
	w.durable = q

	const key uint64 = 9811
	wt := makeWriteTask(t, key, placementTestMsgType, 7)
	session := attachKafkaClaim(t, w, wt)
	h.programFailure(wt.dbTask.TaskId, 1)
	before := durableResultCount(t, metrics.DurableTargetRetry, metrics.DurableNotDurable)

	w.handleTask(wt, true)

	assert.ErrorIs(t, waitAckerFailure(t, wt.acker), errEnqueueNotDurable)
	assert.Error(t, wt.claimCtx.Err())
	assert.Empty(t, session.marked(), "the offset must stay uncommitted so Kafka redelivers it")
	assert.Empty(t, h.callsForKey(key))
	assert.Equal(t, before+1, durableResultCount(t, metrics.DurableTargetRetry, metrics.DurableNotDurable))
}

// 组提交确认不足:跨分区游标不可比的 Kafka 源写进死信未确认,offset 不 ack、死信计数不动。
func TestQuarantine_NotAckedWhenEnqueueNotDurable(t *testing.T) {
	w, mr, h := newTestWorker(t, 5)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 50 * time.Millisecond, budget: 300 * time.Millisecond})
	startTestAOFGroup(t, q, fake, waitAOFNotDurable)
	w.durable = q

	const key uint64 = 9812
	newP5 := makeWriteTask(t, key, placementTestMsgType, 5)
	newP5.originPartition = 5
	w.handleTask(newP5, true)
	require.Len(t, h.callsForKey(key), 1)

	claimCtx, cancel := context.WithCancel(context.Background())
	defer cancel()
	session := &recordingSession{ctx: claimCtx}
	acker := newClaimAcker(session, 100, cancel)
	msg := &sarama.ConsumerMessage{Topic: w.topic, Partition: 0, Offset: 100}
	acker.track(msg)
	delayedOldP0 := makeWriteTask(t, key, placementTestMsgType, 101)
	delayedOldP0.originPartition = 0
	delayedOldP0.kafkaMsg = msg
	delayedOldP0.acker = acker
	delayedOldP0.claimCtx = claimCtx
	beforeNotDurable := durableResultCount(t, metrics.DurableTargetDead, metrics.DurableNotDurable)
	beforeDeadLetter := metricCount(t, "db_task_dead_letter_total",
		map[string]string{"reason": metrics.DeadLetterOrderingIncomparable})

	w.handleTask(delayedOldP0, true)

	assert.ErrorIs(t, waitAckerFailure(t, acker), errEnqueueNotDurable)
	assert.Error(t, claimCtx.Err())
	assert.Empty(t, session.marked(), "Kafka may ACK only after the quarantine copy is confirmed")
	assert.Len(t, h.callsForKey(key), 1, "the quarantined delivery never reaches SQL")
	assert.Equal(t, beforeNotDurable+1, durableResultCount(t, metrics.DurableTargetDead, metrics.DurableNotDurable))
	assert.Equal(t, beforeDeadLetter, metricCount(t, "db_task_dead_letter_total",
		map[string]string{"reason": metrics.DeadLetterOrderingIncomparable}),
		"the dead-letter counter moves only once the copy is confirmed")
}

// 组提交确认不足:毒消息进死信未确认,offset 不 ack、死信计数不动(消息同 TestPoisonKafkaPayloadIsPersistedBeforeAck)。
func TestPoison_NotAckedWhenEnqueueNotDurable(t *testing.T) {
	w, mr, _ := newTestWorker(t, 0)
	fake := installFakeWaitAOF(t, mr)
	q := newTestAOFGroup(t, mr, aofAck{numLocal: 1, timeout: 50 * time.Millisecond, budget: 300 * time.Millisecond})
	startTestAOFGroup(t, q, fake, waitAOFNotDurable)
	w.durable = q

	claimCtx, cancel := context.WithCancel(context.Background())
	defer cancel()
	session := &recordingSession{ctx: claimCtx}
	acker := newClaimAcker(session, 60, cancel)
	msg := &sarama.ConsumerMessage{
		Topic: w.topic, Partition: 0, Offset: 60,
		Key: []byte("player-raw-key"), Value: []byte{0xff, 0x00, 0xff},
	}
	acker.track(msg)
	beforeNotDurable := durableResultCount(t, metrics.DurableTargetDead, metrics.DurableNotDurable)
	beforeDeadLetter := metricCount(t, "db_task_dead_letter_total", map[string]string{"reason": metrics.DeadLetterPoison})

	w.handleTask(&workerTask{kafkaMsg: msg, acker: acker, claimCtx: claimCtx}, true)

	assert.ErrorIs(t, waitAckerFailure(t, acker), errEnqueueNotDurable)
	assert.Error(t, claimCtx.Err())
	assert.Empty(t, session.marked(), "a poison offset may ACK only after its DLQ copy is confirmed")
	assert.Equal(t, beforeNotDurable+1, durableResultCount(t, metrics.DurableTargetDead, metrics.DurableNotDurable))
	assert.Equal(t, beforeDeadLetter,
		metricCount(t, "db_task_dead_letter_total", map[string]string{"reason": metrics.DeadLetterPoison}))
}

// ---------------------------------------------------------------------------
// 真 Redis(GODB_REDIS_ITEST_ADDR 门控)
// ---------------------------------------------------------------------------

// 钉住组提交依赖的三条 Redis 事实:一个 pipeline 落在同一条连接上(WAITAOF 等得到同一 pipeline 里的 LPUSH);
// appendonly yes 时 WAITAOF 1 0 回 [1,0];组提交端到端写进列表。appendonly 不是 yes 时 Fatal 而不是 Skip。
func TestDurableEnqueue_RealRedis(t *testing.T) {
	addr := os.Getenv("GODB_REDIS_ITEST_ADDR")
	if addr == "" {
		t.Skip("set GODB_REDIS_ITEST_ADDR (e.g. 127.0.0.1:6379, Redis >= 7.2 with appendonly yes) to run")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	ack := aofAck{numLocal: 1, timeout: 1500 * time.Millisecond}
	rc := redis.NewClient(durableClientOptions(addr, "", 0, ack))
	t.Cleanup(func() { _ = rc.Close() })
	prefix := fmt.Sprintf("godb:itest:durable:%d:", time.Now().UnixNano())
	pipelineKey, groupKey := prefix+"pipeline", prefix+"group"
	t.Cleanup(func() { _ = rc.Del(context.Background(), pipelineKey, groupKey).Err() })

	var idBefore, idAfter, push *redis.IntCmd
	var wait *redis.Cmd
	_, err := rc.Pipelined(ctx, func(p redis.Pipeliner) error {
		idBefore = p.ClientID(ctx)
		push = p.LPush(ctx, pipelineKey, "x")
		wait = p.Do(ctx, "WAITAOF", 0, 0, 100)
		idAfter = p.ClientID(ctx)
		return nil
	})
	require.NoError(t, err)
	require.NoError(t, push.Err())
	_, err = wait.Slice()
	require.NoError(t, err)
	assert.Equal(t, idBefore.Val(), idAfter.Val(), "one pipeline must run on one connection")

	conf, err := rc.ConfigGet(ctx, "appendonly").Result()
	require.NoError(t, err)
	if conf["appendonly"] != "yes" {
		t.Fatalf("appendonly=%q on %s: this test needs appendonly yes (deploy/docker-compose.yml runs redis with --appendonly yes)",
			conf["appendonly"], addr)
	}
	vals, err := rc.Do(ctx, "WAITAOF", 1, 0, 1500).Slice()
	require.NoError(t, err)
	require.NoError(t, checkWaitAOFReply(vals, 1, 0))

	q := newAOFGroupEnqueue(redis.NewClient(durableClientOptions(addr, "", 0, ack)), ack)
	t.Cleanup(q.close)
	require.NoError(t, q.start(ctx))
	payload := []byte(prefix + "payload")
	require.NoError(t, submitAndWait(t, q, ctx, metrics.DurableTargetRetry, groupKey, payload))
	got, err := rc.LRange(ctx, groupKey, 0, -1).Result()
	require.NoError(t, err)
	assert.Equal(t, []string{string(payload)}, got)
}
