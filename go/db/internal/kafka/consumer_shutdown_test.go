package kafka

// 停机顺序(KeyOrderedKafkaConsumer.Stop 的五步、ConsumeClaim 的 drainClaimForShutdown)的单测。
//
// 替身:fakeClaim(group_handoff_test.go)、recordingSession / recordingHarness(key_ordered_consumer_test.go);
// fakeConsumerGroup 按 sarama v1.43.1 的语义驱动一个 session:会话 ctx 派生自 Consume 的 ctx,任一 ConsumeClaim
// 返回即取消会话,全部返回后调 Cleanup、记一次「会话释放(最后一次提交)」再返回;Close 记事件与当时的状态。

import (
	"context"
	"strconv"
	"sync"
	"testing"
	"time"

	db_locker "db/internal/locker"
	db_proto "proto/db"

	"github.com/IBM/sarama"
	"github.com/alicebob/miniredis/v2"
	"github.com/luyuancpp/proto2mysql"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/proto"
)

// kafkaMessageFor 把一条 DBTask 包成指定分区 / offset 的 Kafka 消息(key 为玩家 id 的十进制串)。
func kafkaMessageFor(t *testing.T, topic string, partition int32, offset int64, task *db_proto.DBTask) *sarama.ConsumerMessage {
	t.Helper()
	body, err := proto.Marshal(task)
	require.NoError(t, err)
	return &sarama.ConsumerMessage{
		Topic: topic, Partition: partition, Offset: offset,
		Key: []byte(strconv.FormatUint(task.Key, 10)), Value: body,
	}
}

// shutdownFixture:一个 worker(由测试 goroutine 逐条 handleTask)、一个直接拼出来的消费者、一个 session 与一个 claim。
// gate 关闭之前,blocked 里的任务在 SQL 之后停住(harness 的 afterCall),并把 taskID 送进 entered。
type shutdownFixture struct {
	w       *worker
	mr      *miniredis.Miniredis
	h       *recordingHarness
	c       *KeyOrderedKafkaConsumer
	handler *consumerGroupHandler
	session *recordingSession
	claim   *fakeClaim

	cancelSession context.CancelFunc

	gate     chan struct{}
	gateOnce sync.Once
	entered  chan string
	blocked  map[string]bool

	stopOnce   sync.Once
	stopWorker context.CancelFunc
	loopDone   chan struct{}
}

func newShutdownFixture(t *testing.T) *shutdownFixture {
	t.Helper()
	w, mr, h := newTestWorker(t, 0)
	workerCtx, stopWorker := context.WithCancel(context.Background())
	w.ctx = workerCtx
	c := &KeyOrderedKafkaConsumer{
		topic:                w.topic,
		partitionCount:       1,
		workers:              map[int32]*worker{0: w},
		ctx:                  workerCtx,
		cancel:               stopWorker,
		redisClient:          w.redisClient,
		retryDeadQueueKey:    w.retryDeadQueueKey,
		pullStop:             make(chan struct{}),
		consumeDone:          make(chan struct{}),
		shutdownDrainTimeout: claimDrainTimeout,
	}
	sessionCtx, cancelSession := context.WithCancel(context.Background())
	f := &shutdownFixture{
		w: w, mr: mr, h: h, c: c,
		handler:       &consumerGroupHandler{consumer: c},
		session:       &recordingSession{ctx: sessionCtx, claims: map[string][]int32{w.topic: {0}}},
		claim:         &fakeClaim{topic: w.topic, partition: 0, messages: make(chan *sarama.ConsumerMessage, 16)},
		cancelSession: cancelSession,
		gate:          make(chan struct{}),
		entered:       make(chan string, 16),
		blocked:       map[string]bool{},
		stopWorker:    stopWorker,
		loopDone:      make(chan struct{}),
	}
	h.setAfterCall(func(call recordedCall) {
		if f.blocked[call.taskID] {
			f.entered <- call.taskID
			<-f.gate
		}
	})
	go func() {
		defer close(f.loopDone)
		for {
			select {
			case <-workerCtx.Done():
				return
			case task := <-w.taskCh:
				w.handleTask(task, true)
			}
		}
	}()
	t.Cleanup(func() {
		f.shutdownWorker()
		cancelSession()
	})
	return f
}

func (f *shutdownFixture) openGate() { f.gateOnce.Do(func() { close(f.gate) }) }

// shutdownWorker 放行被挡住的任务、停掉 worker 循环并等它退出(在途的那一条做完)。
func (f *shutdownFixture) shutdownWorker() {
	f.stopOnce.Do(func() {
		f.openGate()
		f.stopWorker()
		<-f.loopDone
	})
}

// addWrite 往 claim 里放一条写(每条一个玩家,避免合并);block 为 true 时它会在 SQL 之后停在 gate 上。
func (f *shutdownFixture) addWrite(t *testing.T, offset int64, block bool) *db_proto.DBTask {
	t.Helper()
	task := makeWriteTask(t, uint64(9300+offset), placementTestMsgType, uint64(offset)+1).dbTask
	if block {
		f.blocked[task.TaskId] = true
	}
	f.claim.messages <- kafkaMessageFor(t, f.w.topic, 0, offset, task)
	return task
}

func (f *shutdownFixture) startClaim() <-chan error {
	done := make(chan error, 1)
	go func() { done <- f.handler.ConsumeClaim(f.session, f.claim) }()
	return done
}

func waitClaimResult(t *testing.T, done <-chan error) error {
	t.Helper()
	select {
	case err := <-done:
		return err
	case <-time.After(5 * time.Second):
		t.Fatal("ConsumeClaim did not return")
		return nil
	}
}

func waitEntered(t *testing.T, f *shutdownFixture, taskID string) {
	t.Helper()
	select {
	case got := <-f.entered:
		require.Equal(t, taskID, got)
	case <-time.After(5 * time.Second):
		t.Fatalf("task %s never reached its SQL", taskID)
	}
}

// 已派发的 3 条(其中第 2 条卡在 SQL 之后、第 3 条在 worker 通道里排队)在撤拉取之后全部做完并标记,
// ConsumeClaim 返回 nil。
func TestConsumeClaim_ShutdownDrainsDispatchedTasks(t *testing.T) {
	f := newShutdownFixture(t)
	f.addWrite(t, 0, false)
	blocked := f.addWrite(t, 1, true)
	f.addWrite(t, 2, false)
	done := f.startClaim()

	waitEntered(t, f, blocked.TaskId)
	require.Eventually(t, func() bool { return len(f.w.taskCh) == 1 }, 5*time.Second, 5*time.Millisecond,
		"the third task must be dispatched before shutdown starts")
	f.c.stopPulling()
	f.openGate()

	require.NoError(t, waitClaimResult(t, done))
	assert.Equal(t, []int64{0, 1, 2}, f.session.marked(), "every dispatched task completes and is marked before the claim returns")
}

// 撤拉取之后 claim 里还有的消息不 track、不派发、不执行。
func TestConsumeClaim_ShutdownStopsTakingMessages(t *testing.T) {
	f := newShutdownFixture(t)
	f.c.stopPulling()
	f.addWrite(t, 0, false)

	require.NoError(t, waitClaimResult(t, f.startClaim()))
	assert.Empty(t, f.h.snapshot(), "nothing may run after pulling stopped")
	assert.Zero(t, len(f.w.taskCh))
	assert.Empty(t, f.session.marked())
	assert.Equal(t, 1, len(f.claim.messages), "the message is left for the next owner to replay")
}

// 排空期间会话被取消(兄弟 claim 先排空返回,sarama 取消整个会话):本 claim 的 claimCtx 不随之取消,
// 在途任务照常走完复核、缓存与游标,并被标记。
func TestConsumeClaim_ShutdownIgnoresSiblingSessionCancel(t *testing.T) {
	f := newShutdownFixture(t)
	task := f.addWrite(t, 0, true)
	done := f.startClaim()

	waitEntered(t, f, task.TaskId)
	f.c.stopPulling()
	f.cancelSession()
	f.openGate()

	require.NoError(t, waitClaimResult(t, done))
	assert.Equal(t, []int64{0}, f.session.marked())
	_, hasCursor := appliedCursorOf(t, f.w, task.Key)
	assert.True(t, hasCursor, "the in-flight write must finish normally, not be deferred by a canceled claim")
	assert.Zero(t, redisListLen(t, f.w, f.w.retryQueueKey), "nothing is pushed to the retry queue")
}

// 排空超时:按时返回 nil,acker 作废,超时之后才完成的任务与排队中的任务都不再标记(由新属主从已提交位点重放)。
func TestConsumeClaim_ShutdownDrainTimeoutCancelsRest(t *testing.T) {
	f := newShutdownFixture(t)
	f.c.shutdownDrainTimeout = 50 * time.Millisecond
	f.addWrite(t, 0, false)
	blocked := f.addWrite(t, 1, true)
	f.addWrite(t, 2, false)
	done := f.startClaim()

	waitEntered(t, f, blocked.TaskId)
	require.Eventually(t, func() bool { return len(f.w.taskCh) == 1 }, 5*time.Second, 5*time.Millisecond)
	f.c.stopPulling()

	require.NoError(t, waitClaimResult(t, done))
	f.shutdownWorker() // 放行卡住的任务并等 worker 停下:之后不会再有标记
	assert.Equal(t, []int64{0}, f.session.marked(), "offsets after the drain timeout must not be marked")
}

// 不撤拉取、只取消会话(再平衡):claimCtx 照旧随会话取消,原有行为不变。
func TestConsumeClaim_RebalanceStillCancelsClaim(t *testing.T) {
	f := newShutdownFixture(t)
	ctxSeen := make(chan context.Context, 1)
	dbOpHandlers["read"] = func(ctx context.Context, _ *proto2mysql.DB, _ *db_proto.DBTask, _ proto.Message) error {
		ctxSeen <- ctx
		<-f.gate
		return nil
	}
	read := makeReadTask(t, 9390, placementTestMsgType, "rebalance").dbTask
	f.claim.messages <- kafkaMessageFor(t, f.w.topic, 0, 0, read)
	done := f.startClaim()

	var claimCtx context.Context
	select {
	case claimCtx = <-ctxSeen:
	case <-time.After(5 * time.Second):
		t.Fatal("the read never reached its SQL")
	}
	f.cancelSession()
	waitDone(t, claimCtx.Done(), "claim context cancellation on rebalance")
	f.openGate()

	require.NoError(t, waitClaimResult(t, done))
	assert.Empty(t, f.session.marked(), "a revoked claim does not mark anything")
}

// ---------------------------------------------------------------------------
// Stop 的五步
// ---------------------------------------------------------------------------

// fakeConsumerGroup 实现 sarama.ConsumerGroup,只驱动一个 session(一个 claim)。
type fakeConsumerGroup struct {
	topic string
	claim *fakeClaim
	owner *KeyOrderedKafkaConsumer

	mu       sync.Mutex
	events   []string
	sessions int
	session  *recordingSession
	closed   chan struct{}

	closeCalls        int
	ctxErrAtClose     error
	processingAtClose int64
	markedAtClose     []int64
}

func newFakeConsumerGroup(topic string, claim *fakeClaim) *fakeConsumerGroup {
	return &fakeConsumerGroup{topic: topic, claim: claim, closed: make(chan struct{})}
}

func (g *fakeConsumerGroup) record(event string) {
	g.mu.Lock()
	defer g.mu.Unlock()
	g.events = append(g.events, event)
}

func (g *fakeConsumerGroup) snapshotEvents() []string {
	g.mu.Lock()
	defer g.mu.Unlock()
	return append([]string(nil), g.events...)
}

func (g *fakeConsumerGroup) sessionStarted() bool {
	g.mu.Lock()
	defer g.mu.Unlock()
	return g.session != nil
}

func (g *fakeConsumerGroup) Consume(ctx context.Context, _ []string, handler sarama.ConsumerGroupHandler) error {
	g.mu.Lock()
	if g.sessions > 0 {
		g.mu.Unlock()
		// 只跑一个 session;之后的 Consume 像没有分到分区的成员一样,等到 ctx 结束或 Close。
		select {
		case <-ctx.Done():
		case <-g.closed:
		}
		return nil
	}
	g.sessions++
	sessCtx, cancelSess := context.WithCancel(ctx)
	defer cancelSess()
	session := &recordingSession{ctx: sessCtx, claims: map[string][]int32{g.topic: {g.claim.partition}}}
	g.session = session
	g.mu.Unlock()

	if err := handler.Setup(session); err != nil {
		return err
	}
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		defer cancelSess() // sarama:任一 ConsumeClaim 返回即取消整个会话
		_ = handler.ConsumeClaim(session, g.claim)
	}()
	select {
	case <-sessCtx.Done():
	case <-g.closed:
	}
	wg.Wait()
	_ = handler.Cleanup(session)
	g.record("released") // sarama 在这里最后一次提交位点(offsets.Close)
	return nil
}

func (g *fakeConsumerGroup) Errors() <-chan error { return nil }

func (g *fakeConsumerGroup) Close() error {
	g.mu.Lock()
	defer g.mu.Unlock()
	g.events = append(g.events, "close")
	g.closeCalls++
	if g.closeCalls > 1 {
		return nil
	}
	if g.owner != nil {
		g.ctxErrAtClose = g.owner.ctx.Err()
		g.processingAtClose, _ = g.owner.redisClient.LLen(context.Background(), g.owner.retryProcessingKey).Result()
	}
	if g.session != nil {
		g.markedAtClose = g.session.marked()
	}
	close(g.closed)
	return nil
}

func (g *fakeConsumerGroup) Pause(map[string][]int32)  {}
func (g *fakeConsumerGroup) Resume(map[string][]int32) {}
func (g *fakeConsumerGroup) PauseAll()                 {}
func (g *fakeConsumerGroup) ResumeAll()                {}

var _ sarama.ConsumerGroup = (*fakeConsumerGroup)(nil)

// newStopTestConsumer 拼一个可以真跑 Start / Stop 的消费者:Kafka 换成 fakeConsumerGroup,其余(worker、排序锁、
// 重试归属、落点)都是生产代码,跑在 miniredis 上;重试循环间隔 1h,不会自己认领。
func newStopTestConsumer(t *testing.T) (*KeyOrderedKafkaConsumer, *fakeConsumerGroup, *redis.Client) {
	t.Helper()
	mr := miniredis.RunT(t)
	rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rc.Close() })
	const topic = "test-db-task-stop"
	readyKey := "kafka:retry:queue:" + topic
	owner := newRetryOwnership(rc, topic, readyKey, "db-stop-test:6000", 30*time.Second)
	ctx, cancel := context.WithCancel(context.Background())
	t.Cleanup(cancel)
	group := newFakeConsumerGroup(topic, &fakeClaim{topic: topic, partition: 0, messages: make(chan *sarama.ConsumerMessage, 16)})
	c := &KeyOrderedKafkaConsumer{
		consumer:             group,
		redisClient:          rc,
		topic:                topic,
		groupID:              "db_rpc_consumer_group_stop_test",
		partitionCount:       1,
		subShardCount:        1,
		isOfflineExpand:      true,
		workers:              map[int32]*worker{},
		wg:                   &sync.WaitGroup{},
		ctx:                  ctx,
		cancel:               cancel,
		locker:               db_locker.NewRedisLocker(rc),
		retryQueueKey:        readyKey,
		retryProcessingKey:   owner.processingKey,
		retryDeadQueueKey:    "kafka:dead:queue:" + topic,
		retryConsumeInterval: time.Hour,
		retryMaxTimes:        3,
		retryOwner:           owner,
		placement:            newTestPlacementRouter(rc),
		now:                  time.Now,
		pullStop:             make(chan struct{}),
		consumeDone:          make(chan struct{}),
		shutdownDrainTimeout: claimDrainTimeout,
	}
	c.workers[0] = c.buildWorker(0)
	group.owner = c
	return c, group, rc
}

func stopWithin(t *testing.T, c *KeyOrderedKafkaConsumer, d time.Duration) {
	t.Helper()
	done := make(chan struct{})
	go func() {
		c.Stop()
		close(done)
	}()
	select {
	case <-done:
	case <-time.After(d):
		t.Fatalf("Stop did not return within %v", d)
	}
}

// Stop 的顺序:撤拉取 → 在途任务做完、会话释放(最后一次提交)→ 离组(此时 worker 还没被取消、重试收据还在
// 本实例 processing 里)→ 取消 worker → 归还收据。
func TestStop_OrderPullDrainLeaveCancelRelease(t *testing.T) {
	h := newHarness(t)
	c, group, rc := newStopTestConsumer(t)
	gate := make(chan struct{})
	entered := make(chan struct{}, 1)
	h.setAfterCall(func(recordedCall) {
		entered <- struct{}{}
		<-gate
	})
	var gateOnce sync.Once
	openGate := func() { gateOnce.Do(func() { close(gate) }) }
	t.Cleanup(openGate)

	require.NoError(t, c.Start())
	require.Eventually(t, group.sessionStarted, 5*time.Second, 5*time.Millisecond)
	// 一条本实例认领了还没做完的重试收据(Start 之后放:Start 会先收回同名实例遗留的收据)。
	require.NoError(t, rc.LPush(context.Background(), c.retryProcessingKey, "receipt-in-flight").Err())
	task := makeWriteTask(t, 9395, placementTestMsgType, 1).dbTask
	group.claim.messages <- kafkaMessageFor(t, c.topic, 0, 0, task)
	select {
	case <-entered:
	case <-time.After(5 * time.Second):
		t.Fatal("the write never reached its SQL")
	}

	stopped := make(chan struct{})
	go func() {
		c.Stop()
		close(stopped)
	}()
	require.Eventually(t, c.pullStopped, 5*time.Second, 5*time.Millisecond)
	assert.NotContains(t, group.snapshotEvents(), "close", "the group must not be left while a dispatched task is in flight")
	openGate()
	select {
	case <-stopped:
	case <-time.After(20 * time.Second):
		t.Fatal("Stop did not return")
	}

	assert.Equal(t, []string{"released", "close"}, group.snapshotEvents(), "the session is released (final commit) before leaving the group")
	assert.NoError(t, group.ctxErrAtClose, "workers are still running when the group is left")
	assert.Equal(t, int64(1), group.processingAtClose, "retry receipts are returned only after leaving the group")
	assert.Equal(t, []int64{0}, group.markedAtClose, "the in-flight task is marked before leaving the group")
	assert.Error(t, c.ctx.Err(), "workers are canceled by the end of Stop")
	ready, err := rc.LRange(context.Background(), c.retryQueueKey, 0, -1).Result()
	require.NoError(t, err)
	assert.Contains(t, ready, "receipt-in-flight", "the unfinished receipt is returned to ready")
	processing, err := rc.LLen(context.Background(), c.retryProcessingKey).Result()
	require.NoError(t, err)
	assert.Zero(t, processing)
	select {
	case <-c.consumeDone:
	default:
		t.Fatal("consumeDone must be closed after Stop")
	}
}

func TestStop_Idempotent(t *testing.T) {
	newHarness(t)
	c, group, _ := newStopTestConsumer(t)
	require.NoError(t, c.Start())
	stopWithin(t, c, 20*time.Second)
	stopWithin(t, c, time.Second)
	group.mu.Lock()
	defer group.mu.Unlock()
	assert.Equal(t, 1, group.closeCalls, "the group is left exactly once")
}

func TestStop_BeforeStartDoesNotWait(t *testing.T) {
	c, group, _ := newStopTestConsumer(t)
	stopWithin(t, c, 2*time.Second) // 远小于 shutdownDrainTimeout + consumerShutdownGrace
	group.mu.Lock()
	defer group.mu.Unlock()
	assert.Equal(t, 1, group.closeCalls)
	assert.Error(t, c.ctx.Err())
}

// 撤拉取之后重试循环一次都不认领。
func TestRetryConsumer_StopsClaimingAfterPullStop(t *testing.T) {
	w, mr, _ := newTestWorker(t, 0)
	seedReadyWrites(t, w, 9396, 2)
	counter := &claimCounter{}
	c := newRetryTestConsumer(t, w, mr, "db-a:6000", counter)
	c.pullStop = make(chan struct{})
	c.stopPulling()

	c.consumeRetryQueue()

	assert.Zero(t, counter.count())
	assert.Equal(t, int64(2), redisListLen(t, w, w.retryQueueKey))
}
