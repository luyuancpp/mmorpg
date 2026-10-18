package kafka

// 入队未确认落盘时的重新入组退避(R7 复核)。
//
// 背景:组提交在同一个 pipeline 里先 LPUSH、后 WAITAOF(durable_enqueue.go)。WAITAOF 持续无法确认时 —— redis-ha 下
// WaitAOFNumReplicas=1 而副本在维护、AOF 盘 fsync 超过单批预算、运行期被 CONFIG SET appendonly no —— LPUSH 早已生效,
// 未确认只让本 claim 失败。分区的队头若是一条确定性进死信的消息(毒消息、orderingIncomparable、stale_topic、
// missing_required),每个周期就是「LPUSH 进死信 → 预算耗尽或被拒 → acker.fail → ConsumeClaim 返回 error → sarama
// 取消整个会话 → Consume 返回 nil、立即重新入组 → 从同一 offset 重投」,每周期多推一份字节相同的副本;死信没有消费者,
// 副本数随事故时长线性增长(约 5~10s 一份)。本实例其余 claim 每周期也一起作废、一起重投。
//
// 做法:ConsumeClaim 因 errEnqueueNotDurable / errWaitAOFRejected 退出时记一笔(claimFailed);消费循环每次 Consume
// 返回之后取一次(take),本轮有这样的退出就在重新入组之前等 rejoinBackoffFor(连续次数):1s 起、翻倍、上限 30s。
// 任一入队请求确认成功(confirmingEnqueue)就把连续次数清零 —— 三类入队点共用同一个 Redis,一次成功说明落盘确认已恢复;
// 距上一次计入超过 rejoinBackoffResetAfter 也从头算:稳态下入队很少,只靠确认成功清零,相隔几天的两次孤立抖动会被
// 当成「连续」。死循环的周期是「单批预算 5s + 退避 ≤30s + 重新入组」,远小于这个间隔。
// 等待期间撤拉取或 c.ctx 取消立即返回。退避期间本成员不在会话里,超过 Session.Timeout 时 coordinator 会把分区交给别的
// 成员;别的成员遇到同一条队头会同样退避。退避只是限速:事故期间死信仍会累积字节相同的副本(按 (topic, partition,
// offset) 或 taskID 人工去重),副本数的硬上界要在死信入队处按 (topic, partition, offset) 去重,另立改动。
//
// 全部方法对 nil 接收者安全(直接拼结构体的单测没有它):不记、不退避。

import (
	"context"
	"errors"
	"sync"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
)

const (
	// rejoinBackoffBase / rejoinBackoffMax 是第 1 次与封顶的重新入组退避。
	rejoinBackoffBase = time.Second
	rejoinBackoffMax  = 30 * time.Second
	// rejoinBackoffResetAfter:距上一次计入超过它,连续次数从头算(见文件头)。
	rejoinBackoffResetAfter = 5 * time.Minute
)

// isDurabilityFailure 判断 claim 的退出是否因为入队没有拿到落盘确认(预算内份数不足 / 网络错误重试到预算耗尽,或
// WAITAOF 被拒)。停机取消(errDurableEnqueueCanceled)不算。
func isDurabilityFailure(err error) bool {
	return errors.Is(err, errEnqueueNotDurable) || errors.Is(err, errWaitAOFRejected)
}

// rejoinBackoffFor 是连续第 streak 次(≥1)因落盘确认失败退出之后的等待:1s、2s、4s……封顶 30s;streak ≤ 0 时为 0。
func rejoinBackoffFor(streak int) time.Duration {
	if streak <= 0 {
		return 0
	}
	d := rejoinBackoffBase
	for i := 1; i < streak && d < rejoinBackoffMax; i++ {
		d *= 2
	}
	return min(d, rejoinBackoffMax)
}

// durableFailureBackoff 记「本轮会话有 claim 因落盘确认失败退出」与连续次数。claimFailed 在 ConsumeClaim 的 goroutine,
// confirmed 在入队层的回调里(可能是组提交的写协程),take 在消费循环里:一把互斥锁。
type durableFailureBackoff struct {
	mu      sync.Mutex
	pending bool
	streak  int
	lastAt  time.Time // 上一次计入连续次数的时刻(take 的 now)
}

// claimFailed 记一次因落盘确认失败的 claim 退出(同一轮会话里多个 claim 都失败只算一次)。
func (b *durableFailureBackoff) claimFailed() {
	if b == nil {
		return
	}
	b.mu.Lock()
	b.pending = true
	b.mu.Unlock()
}

// confirmed 在任一入队请求确认成功时调用:连续次数清零。
func (b *durableFailureBackoff) confirmed() {
	if b == nil {
		return
	}
	b.mu.Lock()
	b.streak = 0
	b.mu.Unlock()
}

// take 在 Consume 返回之后调用(now 由调用方给出,单测注入):本轮有因落盘确认失败的退出时连续次数加一(距上一次计入
// 超过 rejoinBackoffResetAfter 先清零),返回这次该等多久与连续次数;否则返回 0。
func (b *durableFailureBackoff) take(now time.Time) (time.Duration, int) {
	if b == nil {
		return 0, 0
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	if !b.pending {
		return 0, b.streak
	}
	b.pending = false
	if !b.lastAt.IsZero() && now.Sub(b.lastAt) > rejoinBackoffResetAfter {
		b.streak = 0
	}
	b.streak++
	b.lastAt = now
	return rejoinBackoffFor(b.streak), b.streak
}

// waitRejoinBackoff 是消费循环在下一次 Consume 之前的退避(见文件头);撤拉取或 c.ctx 取消立即返回。
func (c *KeyOrderedKafkaConsumer) waitRejoinBackoff() {
	d, streak := c.durableBackoff.take(c.nowTime())
	if d <= 0 {
		return
	}
	logx.Errorf("consumer group rejoin delayed: a claim failed because an enqueue was not confirmed durable (dead-letter copies may repeat, dedupe by topic/partition/offset): groupID=%s topic=%s delay=%v consecutive=%d",
		c.groupID, c.topic, d, streak)
	var ctxDone <-chan struct{}
	if c.ctx != nil {
		ctxDone = c.ctx.Done()
	}
	timer := time.NewTimer(d)
	defer timer.Stop()
	select {
	case <-c.pullStop:
	case <-ctxDone:
	case <-timer.C:
	}
}

// confirmingEnqueue 包住入队层:任一请求确认成功(done(nil))时先清零退避的连续次数,再回调原 done。
// start / close 原样委托。
type confirmingEnqueue struct {
	durableEnqueue
	backoff *durableFailureBackoff
}

var _ durableEnqueue = confirmingEnqueue{}

func (e confirmingEnqueue) submit(ctx context.Context, target, queueKey string, payload []byte, done func(error)) {
	e.durableEnqueue.submit(ctx, target, queueKey, payload, func(err error) {
		if err == nil {
			e.backoff.confirmed()
		}
		done(err)
	})
}
