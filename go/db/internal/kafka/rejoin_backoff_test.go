package kafka

// R7 复核 [4] 的单测:claim 因入队未确认落盘而退出时,消费循环重新入组之前指数退避;确认成功清零;撤拉取立即打断。
// 时间一律注入(take 的 now),等待只有失败兜底的上限,不靠墙钟推进剧情。

import (
	"context"
	"errors"
	"fmt"
	"testing"
	"time"

	"github.com/IBM/sarama"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestRejoinBackoffFor(t *testing.T) {
	for streak, want := range map[int]time.Duration{
		0: 0, 1: time.Second, 2: 2 * time.Second, 3: 4 * time.Second, 5: 16 * time.Second, 6: 30 * time.Second, 100: 30 * time.Second,
	} {
		assert.Equalf(t, want, rejoinBackoffFor(streak), "streak=%d", streak)
	}
}

func TestDurableFailureBackoff_TakeAndReset(t *testing.T) {
	base := time.Unix(1_800_000_000, 0)
	b := &durableFailureBackoff{}
	d, _ := b.take(base)
	assert.Zero(t, d, "nothing failed in this session")

	b.claimFailed()
	b.claimFailed() // 同一轮会话里多个 claim 失败只算一次
	d, streak := b.take(base)
	assert.Equal(t, time.Second, d)
	assert.Equal(t, 1, streak)
	b.claimFailed()
	d, streak = b.take(base.Add(10 * time.Second))
	assert.Equal(t, 2*time.Second, d)
	assert.Equal(t, 2, streak)

	b.confirmed() // 落盘确认恢复
	b.claimFailed()
	d, streak = b.take(base.Add(20 * time.Second))
	assert.Equal(t, time.Second, d, "a confirmed enqueue resets the streak")
	assert.Equal(t, 1, streak)

	b.claimFailed()
	d, _ = b.take(base.Add(30 * time.Second))
	assert.Equal(t, 2*time.Second, d)
	b.claimFailed()
	d, streak = b.take(base.Add(30*time.Second + rejoinBackoffResetAfter + time.Second))
	assert.Equal(t, time.Second, d, "isolated failures far apart are not consecutive")
	assert.Equal(t, 1, streak)

	var none *durableFailureBackoff
	none.claimFailed()
	none.confirmed()
	d, _ = none.take(base)
	assert.Zero(t, d, "a nil backoff (direct test consumers) never waits")
}

// ConsumeClaim 因 not_durable / WAITAOF 被拒退出时记一笔,别的失败不记。
func TestConsumeClaim_DurabilityFailureArmsRejoinBackoff(t *testing.T) {
	for _, tc := range []struct {
		name  string
		err   error
		armed bool
	}{
		{name: "not_durable", err: fmt.Errorf("%w: injected", errEnqueueNotDurable), armed: true},
		{name: "waitaof_rejected", err: fmt.Errorf("%w: ERR appendonly no", errWaitAOFRejected), armed: true},
		{name: "other", err: errors.New("LPUSH rejected: WRONGTYPE"), armed: false},
	} {
		t.Run(tc.name, func(t *testing.T) {
			f := newShutdownFixture(t)
			f.c.durableBackoff = &durableFailureBackoff{}
			held := &holdingEnqueue{}
			t.Cleanup(held.close)
			f.c.durable = held
			f.claim.messages <- &sarama.ConsumerMessage{Topic: f.w.topic, Partition: 0, Offset: 0, Value: []byte{0xff, 0xff, 0xff}}
			done := f.startClaim()

			var reqs []*heldEnqueue
			require.Eventually(t, func() bool { reqs = held.requests(); return len(reqs) == 1 }, 5*time.Second, 5*time.Millisecond)
			reqs[0].release(tc.err)
			require.Error(t, waitClaimResult(t, done))

			d, _ := f.c.durableBackoff.take(time.Unix(1_800_000_000, 0))
			if tc.armed {
				assert.Equal(t, time.Second, d, "the next rejoin waits")
			} else {
				assert.Zero(t, d, "only unconfirmed durability arms the rejoin backoff")
			}
		})
	}
}

// 退避等待中撤拉取或 c.ctx 取消立即返回(这里的退避是封顶的 30s)。
func TestWaitRejoinBackoff_InterruptedByPullStopAndCancel(t *testing.T) {
	newArmed := func() *durableFailureBackoff {
		return &durableFailureBackoff{pending: true, streak: 10, lastAt: time.Now()}
	}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	c := &KeyOrderedKafkaConsumer{ctx: ctx, pullStop: make(chan struct{}), durableBackoff: newArmed(), now: time.Now}
	c.stopPulling()
	returned := make(chan struct{})
	go func() { c.waitRejoinBackoff(); close(returned) }()
	waitDone(t, returned, "rejoin backoff interrupted by pulling stop")

	ctx2, cancel2 := context.WithCancel(context.Background())
	c2 := &KeyOrderedKafkaConsumer{ctx: ctx2, pullStop: make(chan struct{}), durableBackoff: newArmed(), now: time.Now}
	cancel2()
	returned2 := make(chan struct{})
	go func() { c2.waitRejoinBackoff(); close(returned2) }()
	waitDone(t, returned2, "rejoin backoff interrupted by consumer cancel")

	// 没有因落盘确认失败的退出:不等。
	c3 := &KeyOrderedKafkaConsumer{ctx: context.Background(), pullStop: make(chan struct{}), durableBackoff: &durableFailureBackoff{}, now: time.Now}
	returned3 := make(chan struct{})
	go func() { c3.waitRejoinBackoff(); close(returned3) }()
	waitDone(t, returned3, "no rejoin backoff without a durability failure")
}

// confirmingEnqueue:确认成功(done(nil))清零连续次数,失败不清零;done 照常恰好一次。
func TestConfirmingEnqueue_ResetsStreakOnConfirmation(t *testing.T) {
	b := &durableFailureBackoff{streak: 5}
	held := &holdingEnqueue{}
	q := confirmingEnqueue{durableEnqueue: held, backoff: b}

	var results []error
	q.submit(context.Background(), "dead", "k", []byte("p"), func(err error) { results = append(results, err) })
	q.submit(context.Background(), "dead", "k", []byte("p"), func(err error) { results = append(results, err) })
	reqs := held.requests()
	require.Len(t, reqs, 2)

	reqs[0].release(errEnqueueNotDurable)
	b.mu.Lock()
	assert.Equal(t, 5, b.streak, "a failed confirmation does not reset")
	b.mu.Unlock()
	reqs[1].release(nil)
	b.mu.Lock()
	assert.Zero(t, b.streak, "a confirmed enqueue resets the streak")
	b.mu.Unlock()
	assert.Equal(t, []error{errEnqueueNotDurable, nil}, results)
}
