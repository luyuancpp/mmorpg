package kafka

// 读方就绪闸(route_ready_gate.go,D1 第 3 批)与停机、重新入组退避之间的接缝回归。
//
// go/db 管线一线提醒过两条,这里逐条钉住:
//   - 开关打开(Placement.RequireRouteReady=true)后 Stop 不能被闸卡住:停在闸前的 claim、停在闸前的重试循环、没分到
//     分区的成员,都要能被撤拉取(pullStop)或会话 ctx 打断;生产里 c.ctx 取消经 consumeCtx 传到会话 ctx,打断的是同一路;
//   - 保留 R7 的重新入组退避(rejoin_backoff.go):闸关期间 claim 不返回,不制造入组抖动;闸关之前已交给入队层的请求,
//     落盘确认失败仍让 claim 带错返回、记入退避;开关打开时消费循环照常退避,退避等待照样被撤拉取打断。
//
// 另有两组同一接缝上的用例:闸由开转关时,空闲分区上等消息的 claim 被那次刷新唤醒、停在闸前,不再漏取一条(gatedMessages
// 的修复);闸关时再平衡 / 交接让出 / 停机排空在途任务都按原有收尾走;owner 模式的重试循环在闸关时连分拣也停。
//
// consumer_shutdown_test.go 的 TestStop_* / TestConsumeClaim_Shutdown* 都不带闸(routeGate 为 nil),覆盖不到开关打开的
// 情形,所以单列在这里。替身与夹具沿用那边的 newShutdownFixture / newStopTestConsumer / fakeConsumerGroup / probedClaim、
// route_ready_gate_test.go 的 fakeRouteReadyReader / refreshGate、retry_routing_test.go 的 routingFixture,以及
// group_handoff_test.go 的 handoffHarness。
//
// 闸只由用例调 refreshGate 刷新;Start 起的刷新 goroutine 用 1h 间隔,用例期间不会自己刷新。等待都有上限,不靠墙钟推进剧情。

import (
	"context"
	"fmt"
	"sync/atomic"
	"testing"
	"time"

	db_config "db/internal/config"

	"github.com/IBM/sarama"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// untrustedRouteGate 返回一个读方报「标记不在」的闸(间隔 1h)与它的读方;闸的初始状态就是关着,Start 的启动探针或用例
// 自己的 probe 给出第一个结论(不可信,闸照样关着)。
func untrustedRouteGate(t *testing.T, rc redis.Cmdable) (*routeReadyGate, *fakeRouteReadyReader) {
	t.Helper()
	reader := newFakeRouteReadyReader(t, rc)
	reader.setAbsent()
	return newRouteReadyGate(reader, time.Hour), reader
}

// waitGateWaitsAbove 等闸的等待计数超过 n(消费循环或重试循环又一次停在闸前)。
func waitGateWaitsAbove(t *testing.T, g *routeReadyGate, n int64, what string) {
	t.Helper()
	require.Eventually(t, func() bool { return g.waits.Load() > n }, 5*time.Second, time.Millisecond, what)
}

// backoffStreak 读重新入组退避的连续次数(带锁)。
func backoffStreak(b *durableFailureBackoff) int {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.streak
}

// countingConsumerGroup 给 fakeConsumerGroup 数 Consume(入组)的次数,其余原样转发。
type countingConsumerGroup struct {
	*fakeConsumerGroup
	consumes atomic.Int32
}

func (g *countingConsumerGroup) Consume(ctx context.Context, topics []string, handler sarama.ConsumerGroupHandler) error {
	g.consumes.Add(1)
	return g.fakeConsumerGroup.Consume(ctx, topics, handler)
}

// gateClosingEnqueue 先关闸(代替一次刷新)、再扣下请求(holdingEnqueue)。persistPoisonMessage 在 claim 的 goroutine 里
// 同步调 submit,所以 claim 回到主循环顶部时闸一定已经关了:之后到达的确认结果,落在 claim 停在闸前的时候。
// 先关闸后记请求:用例看到请求时闸一定已经关了(反过来的顺序,用例可能在两步之间读到闸还开着)。
type gateClosingEnqueue struct {
	*holdingEnqueue
	closeGate func()
}

func (e gateClosingEnqueue) submit(ctx context.Context, target, queueKey string, payload []byte, done func(error)) {
	e.closeGate()
	e.holdingEnqueue.submit(ctx, target, queueKey, payload, done)
}

var _ durableEnqueue = gateClosingEnqueue{}

// 开关打开、闸关着(对账 leader 还没挂标记)时停机:停在闸前的 claim 被撤拉取打断,走停机收尾;Stop 远在第②步上限
// (shutdownDrainTimeout + consumerShutdownGrace = 15s)之内返回,仍是先释放会话、后离组,离组时 worker 还没被取消。
// 消息一条没取:不执行、不标记,留给下一任属主重放。
// 红证据:把 ConsumeClaim 里的闸改成「select 之前阻塞等 recheck、直到放行」(不进 select、不看 pullStop / 会话 ctx),
// 本用例报 Stop did not return within 2s。
func TestStop_RouteReadyGateClosedClaimParkedReturnsPromptly(t *testing.T) {
	h := newHarness(t)
	c, group, _ := newStopTestConsumer(t)
	gate, _ := untrustedRouteGate(t, c.redisClient)
	c.routeGate = gate

	require.NoError(t, c.Start(), "an untrusted replica is not a startup error")
	require.True(t, c.isStarted())
	require.False(t, gate.opened(), "the startup probe found no trusted marker: the gate stays closed")
	require.Eventually(t, group.sessionStarted, 5*time.Second, 5*time.Millisecond)
	task := makeWriteTask(t, 9701, placementTestMsgType, 1).dbTask
	group.claim.messages <- kafkaMessageFor(t, c.topic, 0, 0, task)
	// 重试循环间隔 1h(newStopTestConsumer),闸的等待计数只来自 claim。
	waitGateWaitsAbove(t, gate, 0, "the claim must park on the closed gate")

	stopWithin(t, c, 2*time.Second)

	assert.Equal(t, []string{"released", "close"}, group.snapshotEvents(), "the session is released (final commit) before leaving the group")
	assert.NoError(t, group.ctxErrAtClose, "step ② ended on its own: workers are still running when the group is left")
	assert.Empty(t, group.markedAtClose, "nothing was taken, so nothing is marked")
	assert.Len(t, group.claim.messages, 1, "the message stays in Kafka for the next owner")
	assert.Empty(t, h.callsForKey(task.Key), "nothing runs while the gate is closed")
	assert.False(t, gate.opened())
	assert.Error(t, c.ctx.Err(), "workers are canceled by the end of Stop")
	waitDone(t, c.consumeDone, "consume loop exit")
}

// 开关打开、闸关着,本实例没分到分区(副本数多于分区数的热备成员),只有重试循环在跑:每一拍都停在闸前,收据留在 ready
// (不认领、不进死信);Stop 很快返回,重试协程随撤拉取退出,第④步的 wg.Wait 不被卡住。
// 红证据:删掉 consumeRetryQueue 里的 retryClaimsPaused 判断,等待计数不再增长,本用例在「the retry loop must pause on the
// closed gate on every tick」处失败(Condition never satisfied)。
func TestStop_RouteReadyGateClosedRetryLoopPausedReturnsPromptly(t *testing.T) {
	c, group, rc := newStopTestConsumer(t)
	group.noClaims = true
	c.retryConsumeInterval = 5 * time.Millisecond
	gate, _ := untrustedRouteGate(t, rc)
	c.routeGate = gate

	require.NoError(t, c.Start())
	require.Eventually(t, group.sessionStarted, 5*time.Second, 5*time.Millisecond)
	// 畸形收据:闸放行的话本拍就会被认领、按畸形载荷进死信。Start 之后再放(Start 会先收回同名实例遗留的收据)。
	require.NoError(t, rc.LPush(context.Background(), c.retryQueueKey, "receipt-ready").Err())
	// 多等一拍:第一次计数可能来自 LPUSH 之前已经开始的那一拍。
	waitGateWaitsAbove(t, gate, gate.waits.Load()+1, "the retry loop must pause on the closed gate on every tick")

	stopWithin(t, c, 2*time.Second)

	assert.Equal(t, []string{"released", "close"}, group.snapshotEvents())
	ready, err := rc.LRange(context.Background(), c.retryQueueKey, 0, -1).Result()
	require.NoError(t, err)
	assert.Equal(t, []string{"receipt-ready"}, ready, "a closed gate claims nothing: the receipt stays in ready")
	dead, err := rc.LLen(context.Background(), c.retryDeadQueueKey).Result()
	require.NoError(t, err)
	assert.Zero(t, dead)
	processing, err := rc.LLen(context.Background(), c.retryProcessingKey).Result()
	require.NoError(t, err)
	assert.Zero(t, processing)
	waitDone(t, c.consumeDone, "consume loop exit")
}

// 闸关着时再平衡(只取消会话、不撤拉取):停在闸前的 claim 按再平衡收尾返回 nil,不取消息、不标记;不是落盘确认失败,
// 不记重新入组退避。
// 红证据同第一个用例:闸改成 select 之前阻塞等待时,本用例报 ConsumeClaim did not return within 5s。
func TestConsumeClaim_RouteReadyGateClosedRebalanceReturnsNil(t *testing.T) {
	f := newShutdownFixture(t)
	gate, _ := untrustedRouteGate(t, f.w.redisClient)
	require.NoError(t, gate.probe(context.Background()))
	f.c.routeGate = gate
	f.c.durableBackoff = &durableFailureBackoff{}
	task := f.addWrite(t, 0, false)
	done := f.startClaim()
	waitGateWaitsAbove(t, gate, 0, "the claim must park on the closed gate")

	f.cancelSession()

	require.NoError(t, waitClaimResult(t, done), "a revoked claim parked on the closed gate returns nil like any rebalance")
	assert.Empty(t, f.h.callsForKey(task.Key))
	assert.Empty(t, f.session.marked())
	assert.Len(t, f.claim.messages, 1, "the message stays in Kafka for the next owner")
	d, _ := f.c.durableBackoff.take(time.Unix(1_800_000_000, 0))
	assert.Zero(t, d, "a rebalance is not a durability failure: no rejoin backoff")
}

// 闸关之前已交给入队层的毒消息(异步确认),确认失败落在 claim 停在闸前的时候:claim 照样从 failureCh 收到错误、带错返回,
// 重新入组退避照样记上一笔;停在闸前期间后面那条消息没被取走。
// 红证据:删掉 ConsumeClaim 顶层 select 的 `case err := <-acker.failureCh` 分支,本用例报 ConsumeClaim did not return
// within 5s;删掉 ConsumeClaim 的 defer 里 durableBackoff.claimFailed(),退避断言失败。
func TestConsumeClaim_RouteReadyGateClosedDurabilityFailureArmsRejoinBackoff(t *testing.T) {
	f := newShutdownFixture(t)
	reader := newFakeRouteReadyReader(t, f.w.redisClient)
	gate := newRouteReadyGate(reader, time.Hour)
	require.NoError(t, gate.probe(context.Background()))
	require.True(t, gate.opened())
	f.c.routeGate = gate
	f.c.durableBackoff = &durableFailureBackoff{}
	held := &holdingEnqueue{}
	t.Cleanup(held.close)
	f.c.durable = gateClosingEnqueue{holdingEnqueue: held, closeGate: func() {
		reader.setAbsent()
		refreshGate(gate)
	}}
	f.claim.messages <- &sarama.ConsumerMessage{Topic: f.w.topic, Partition: 0, Offset: 0, Value: []byte{0xff, 0xff, 0xff}}
	next := f.addWrite(t, 1, false)
	done := f.startClaim()

	var reqs []*heldEnqueue
	require.Eventually(t, func() bool { reqs = held.requests(); return len(reqs) == 1 }, 5*time.Second, 5*time.Millisecond)
	require.False(t, gate.opened())
	waitGateWaitsAbove(t, gate, 0, "the claim must park on the gate that closed right after the poison was handed over")
	assert.Len(t, f.claim.messages, 1, "the next message is not taken while the gate is closed")

	reqs[0].release(fmt.Errorf("%w: injected", errEnqueueNotDurable))

	err := waitClaimResult(t, done)
	require.Error(t, err)
	assert.ErrorIs(t, err, errEnqueueNotDurable)
	d, _ := f.c.durableBackoff.take(time.Unix(1_800_000_000, 0))
	assert.Equal(t, time.Second, d, "a durability failure while parked on the gate still arms the rejoin backoff")
	assert.Empty(t, f.session.marked())
	assert.Empty(t, f.h.callsForKey(next.Key), "the next message never ran")
	assert.Len(t, f.claim.messages, 1)
}

// 开关打开时,claim 因落盘确认失败退出后,消费循环照样在重新入组之前退避(这里已连续 9 次,这次封顶 30s),没有立刻
// 重新入组;退避期间闸关上与消费循环无关;Stop 立即打断退避,不重新入组、不等满 30s。
// 红证据:删掉 Start 消费循环里的 waitRejoinBackoff 调用,连续次数等不到 10;删掉 waitRejoinBackoff 里 select 的
// pullStop 分支,本用例报 Stop did not return within 2s。
func TestStop_RouteReadyGateOnRejoinBackoffKeptAndInterrupted(t *testing.T) {
	c, group, rc := newStopTestConsumer(t)
	counted := &countingConsumerGroup{fakeConsumerGroup: group}
	c.consumer = counted
	reader := newFakeRouteReadyReader(t, rc)
	c.routeGate = newRouteReadyGate(reader, time.Hour)
	backoff := &durableFailureBackoff{streak: 9, lastAt: time.Now()}
	c.durableBackoff = backoff
	held := &holdingEnqueue{}
	c.durable = held

	require.NoError(t, c.Start())
	require.True(t, c.routeGate.opened(), "the startup probe found a trusted marker")
	require.Eventually(t, group.sessionStarted, 5*time.Second, 5*time.Millisecond)
	group.claim.messages <- &sarama.ConsumerMessage{Topic: c.topic, Partition: 0, Offset: 0, Value: []byte{0xff, 0xff, 0xff}}
	var reqs []*heldEnqueue
	require.Eventually(t, func() bool { reqs = held.requests(); return len(reqs) == 1 }, 5*time.Second, 5*time.Millisecond)
	reqs[0].release(fmt.Errorf("%w: injected", errEnqueueNotDurable))

	// 会话释放之后消费循环取了这一笔(连续次数 9 → 10,等 30s),停在退避里。
	require.Eventually(t, func() bool { return backoffStreak(backoff) == 10 }, 5*time.Second, time.Millisecond,
		"the consume loop must enter the rejoin backoff after the durability failure")
	reader.setAbsent()
	refreshGate(c.routeGate)
	require.False(t, c.routeGate.opened())

	stopWithin(t, c, 2*time.Second)

	assert.Equal(t, int32(1), counted.consumes.Load(), "the loop never rejoined: Stop interrupted the backoff")
	assert.Equal(t, []string{"released", "close"}, group.snapshotEvents())
	waitDone(t, c.consumeDone, "consume loop exit")
}

// 闸由开转关时,空闲分区上等消息的 claim 被那次刷新唤醒、回到循环顶停在闸前:之后到达的消息不取、不执行、不标记、
// 不进重试队列;再刷新一次仍不可信就醒来复查、再停下;放行后照常消费。
// 红证据:把 gatedMessages 改回「闸放行时只返回消息通道、刷新通道为 nil」(HEAD c35e1133d 的写法),claim 停在 select 上
// 醒不过来,第一个 waitGateWaitsAbove 超时;不等它的话,下一条消息会被取走、逐条判不可信后推进重试队列。
func TestConsumeClaim_RouteReadyGateClosingWakesIdleClaim(t *testing.T) {
	f := newShutdownFixture(t)
	reader := newFakeRouteReadyReader(t, f.w.redisClient)
	f.w.placement.ready = reader // 逐条判与闸用同一个读方:漏取的消息会被判不可信、推进重试队列
	gate := newRouteReadyGate(reader, time.Hour)
	require.NoError(t, gate.probe(context.Background()))
	require.True(t, gate.opened())
	f.c.routeGate = gate
	first := f.addWrite(t, 0, false)
	claim := &probedClaim{fakeClaim: f.claim}
	done := make(chan error, 1)
	go func() { done <- f.handler.ConsumeClaim(f.session, claim) }()

	// 第 1 轮取到第一条并派发;第 2 次求值 Messages() 时通道已空,刷新通道已在它之前取到:claim 停在第 2 轮顶层 select 上,
	// 闸开着。
	require.Eventually(t, func() bool { return claim.messagesCalls.Load() >= 2 }, 5*time.Second, time.Millisecond)
	require.Eventually(t, func() bool { return len(f.session.marked()) == 1 }, 5*time.Second, time.Millisecond)
	assert.Zero(t, gate.waits.Load(), "waiting for messages behind an open gate is not a wait on the gate")

	reader.setAbsent()
	refreshGate(gate)
	waitGateWaitsAbove(t, gate, 0, "the refresh that closes the gate must wake the idle claim, which then parks on the gate")

	second := f.addWrite(t, 1, false)
	waits := gate.waits.Load()
	refreshGate(gate)
	waitGateWaitsAbove(t, gate, waits, "an untrusted refresh wakes the parked claim, which parks again")
	assert.Len(t, f.claim.messages, 1, "a message arriving after the gate closed stays in Kafka")
	assert.Empty(t, f.h.callsForKey(second.Key))
	assert.Zero(t, redisListLen(t, f.w, f.w.retryQueueKey), "nothing is pushed into the retry queue while the gate is closed")
	assert.Equal(t, []int64{0}, f.session.marked())

	reader.setTrusted(t)
	refreshGate(gate)
	require.Eventually(t, func() bool { return len(f.session.marked()) == 2 }, 5*time.Second, time.Millisecond,
		"once the gate reopens the waiting message is consumed and marked")
	assert.Len(t, f.h.callsForKey(first.Key), 1)
	assert.Len(t, f.h.callsForKey(second.Key), 1)

	f.c.stopPulling()
	require.NoError(t, waitClaimResult(t, done))
}

// 闸开着时派发的写还卡在 SQL 之后,闸关上、claim 停在闸前时停机:已派发的照常做完并标记(drainClaimForShutdown),不按
// 再平衡或失败作废;停在闸前期间到达的消息不取。
func TestConsumeClaim_RouteReadyGateClosedShutdownMarksInFlight(t *testing.T) {
	f := newShutdownFixture(t)
	reader := newFakeRouteReadyReader(t, f.w.redisClient)
	gate := newRouteReadyGate(reader, time.Hour)
	require.NoError(t, gate.probe(context.Background()))
	f.c.routeGate = gate
	// 逐条判不接读方(placement.ready 为 nil,走 MGET):在途的写不受闸关影响,照常落库。
	inFlight := f.addWrite(t, 0, true)
	done := f.startClaim()
	waitEntered(t, f, inFlight.TaskId)

	reader.setAbsent()
	refreshGate(gate)
	waitGateWaitsAbove(t, gate, 0, "the claim must park on the gate that just closed")
	next := f.addWrite(t, 1, false)

	f.c.stopPulling()
	f.openGate() // 放行卡在 SQL 之后的那条(fixture 的 SQL 闸,不是就绪闸)

	require.NoError(t, waitClaimResultWithin(t, done, 3*time.Second))
	assert.Equal(t, []int64{0}, f.session.marked(), "the in-flight task completes and is marked before the claim returns")
	assert.Empty(t, f.h.callsForKey(next.Key), "the message that arrived while parked never runs")
	assert.Len(t, f.claim.messages, 1)
	assert.Zero(t, redisListLen(t, f.w, f.w.retryQueueKey))
}

// 闸关着时交接闸守望让出(对方组正在消费本 topic):停在闸前的 claim 按让出收尾返回 nil,不取消息、不标记;这不是停机。
// 红证据同第一个用例:闸改成 select 之前阻塞等待时,本用例报 ConsumeClaim did not return within 5s。
func TestConsumeClaim_RouteReadyGateClosedHandoffYieldReturnsNil(t *testing.T) {
	f := newShutdownFixture(t)
	hh := newHandoffHarness(t, testZoneGroup, testLegacyGroup, 1)
	hh.forceRelease()
	hh.cluster.setGroup(testZoneGroup, groupStateEmpty, nil, map[int32]int64{0: 5})
	hh.cluster.setGroup(testLegacyGroup, groupStateStable, []int32{0}, map[int32]int64{0: 1})
	f.c.handoff = hh.h
	require.NoError(t, f.handler.Setup(f.session))
	_, yieldDone := f.handler.sessionScope(f.session)
	require.NotNil(t, yieldDone, "with a handoff the claim scope is the yield context")
	gate, _ := untrustedRouteGate(t, f.w.redisClient)
	require.NoError(t, gate.probe(context.Background()))
	f.c.routeGate = gate
	task := f.addWrite(t, 0, false)
	done := f.startClaim()
	waitGateWaitsAbove(t, gate, 0, "the claim must park on the closed gate")

	// 误部署的旧发布物拿走本 topic 的分区并推进位点:守望第 1 拍建基线,第 2 拍看到前进 → 让出。
	hh.h.watchOnce()
	hh.cluster.setCommitted(testLegacyGroup, map[int32]int64{0: 2})
	hh.h.watchOnce()
	waitDone(t, yieldDone, "handoff yield")

	require.NoError(t, waitClaimResult(t, done), "a claim parked on the closed gate returns nil when the handoff yields")
	assert.False(t, f.c.pullStopped())
	assert.Empty(t, f.h.callsForKey(task.Key))
	assert.Empty(t, f.session.marked())
	assert.Len(t, f.claim.messages, 1)
	require.NoError(t, f.handler.Cleanup(f.session))
}

// owner 模式(retry_routing.go):闸关着时本拍连共享收件箱的分拣也停,不认领、不执行;放行后照常分拣。闸检查在 ownerRouting
// 分支之前,两种模式共用(shared 模式见 route_ready_gate_test.go 的 TestConsumeRetryQueue_RouteReadyGateClosedClaimsNothing)。
// 红证据:把闸检查挪进 consumeRetryQueueShared,本用例的收件箱会被分拣进 p3 / p1。
func TestConsumeRetryQueue_RouteReadyGateClosedOwnerModeForwardsNothing(t *testing.T) {
	f := newRoutingFixture(t)
	c := f.consumer("db-a:6000", db_config.RetryRoutingOwner, 4)
	gate, reader := untrustedRouteGate(t, f.rc)
	require.NoError(t, gate.probe(context.Background()))
	c.routeGate = gate
	p3, _ := routedPayload(t, 9711, 11, 3, 0)
	p1, _ := routedPayload(t, 9712, 12, 1, 0)
	f.push(t, c.retryQueueKey, p3, p1)

	c.consumeRetryQueue()

	assert.Equal(t, int64(2), f.llen(t, c.retryQueueKey), "a closed gate does not even sort the inbox")
	for p := int32(0); p < 4; p++ {
		assert.Zerof(t, f.llen(t, pKey(p)), "p%d", p)
	}
	assert.Zero(t, f.llen(t, c.retryProcessingKey))
	assert.Empty(t, f.h.snapshot())
	assertNoDispatch(t, c)
	assert.Equal(t, int64(1), gate.waits.Load(), "the retry tick paused on the gate once")

	reader.setTrusted(t)
	refreshGate(gate)
	c.consumeRetryQueue()

	assert.Equal(t, [][]byte{p3}, f.list(t, pKey(3)), "once the gate opens the inbox is sorted as before")
	assert.Equal(t, [][]byte{p1}, f.list(t, pKey(1)))
	assert.Zero(t, f.llen(t, c.retryQueueKey))
}
