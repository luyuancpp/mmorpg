package kafka

// ordering_lease.go —— 跨实例排序锁 distributed:lock:kafka:ordering:{topic}:{key}:{msgType} 的租约:取锁、续租看门狗、
// 写 SQL 前的本地截止时间复核、令牌发布 / 推进脚本的核锁、释放(docs/design/db-task-pipeline-bottlenecks.md G1)。
//
// G1 治标做的四件事:
//  1. 写 SQL 的 ctx 来自租约(beginSQL),只在失租时取消;claim 被收回、进程停机、worker ctx 取消都不取消正在跑的
//     写 SQL。原因:取消之后锁会随任务一起释放,而服务端可能还在执行这条语句 —— 它可能在别的实例写入新版本之后
//     才提交,造成回档。所以只有「锁已经不归我 / 快要不归我」时才取消,而且 SQL 在途时被取消的,释放阶段不删锁,
//     留给 TTL 过期(sqlAbandoned)。不用带 deadline 的 ctx:context.DeadlineExceeded 会被 storeerr 归成
//     conn_lost,误触发探活与库级冷却。
//  2. applied / equal 在落库后复核之后,由同一 Lua 核 token 并发布缓存 / SQL 证据;游标与 applied epoch 的
//     推进也由带 token 的 Lua 原子完成。older_seq 不发布缓存,只走 verify 核锁后确认。脚本返回 0 记
//     owner_changed,核锁或令牌脚本往返失败记 verify_error;失租不发布缓存,也不推进游标与 applied epoch。
//  3. 续租在本地单调时钟的截止时间(validUntil − margin)之内有界退避重试:首次 500ms、逐次翻倍、封顶 5s、±20% 抖动,
//     单次调用预算 3s,每轮故障最多 10 次。用尽次数记 renew_exhausted,到截止时间记 renew_deadline,两者都失租并
//     取消 SQL;续租发现易主时立即失租(owner_changed),不重试。旧实现出错一次就放弃,一次 Redis 抖动就会被放大成失租。
//  4. 释放结果按 released / not_owner / error / skipped_sql_abandoned 计数(db_ordering_lock_release_total);
//     not_owner 与失租各配一条告警(deploy/k8s/db-task-retry-alerts.yaml 的 db-task.ordering-lease 组)。
//
// 这不是栅栏,撤不回已经进了 MySQL 的语句:
//   - go-sql-driver 取消 ctx 时只关掉客户端这一侧的连接、返回 ctx.Err(),不发 KILL QUERY,服务端照样可能提交。
//     被遗弃语句在服务端的存活上限靠会话级 lock_wait_timeout / innodb_lock_wait_timeout(L-04),所以 margin 必须
//     大于两者之和再加余量(leaseMarginCoversLockWaits,启动时检查)。
//   - owner_changed 时锁已经不归本实例,「锁留到 TTL」挡不住已经持锁的别的实例;窗口只是缩小了。
//   - 本地截止时间依赖本进程的单调时钟:虚拟机暂停恢复后,SQL goroutine 可能抢在看门狗之前发出语句。
//     beginSQL 只在发 SQL 前复核一次,缩小窗口,不能关闭。
//   持久化防线另由条件写提供(docs/design/db-write-fencing-mysql.md),不能把租约成功当作 SQL 围栏。

import (
	"context"
	"db/internal/locker"
	"db/internal/metrics"
	"errors"
	"fmt"
	"github.com/redis/go-redis/v9"
	"math/rand/v2"
	db_proto "proto/db"
	"sync"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
)

const (
	// orderingLockTTL 是排序锁在 Redis 上的过期时间;orderingLockRenewInterval 是正常续租间隔。
	orderingLockTTL           = 2 * time.Minute
	orderingLockRenewInterval = 30 * time.Second
	// orderingLockReleaseTimeout bounds the lock release issued while shutting down or after a canceled claim.
	orderingLockReleaseTimeout = 2 * time.Second

	// orderingLeaseSafetyMargin:本地截止时间 = 锁在 Redis 上最早可能过期的时刻 − margin。必须大于服务端一条被遗弃
	// 语句的最长存活时间,即会话 lock_wait_timeout + innodb_lock_wait_timeout(默认各 10s),再加余量:在本地截止时间
	// 取消的语句,最晚在锁真正过期之前被服务端以 1205 中止。见 leaseMarginCoversLockWaits。
	orderingLeaseSafetyMargin = 30 * time.Second
	// orderingLeaseMarginSlack 是 margin 在两个会话锁等待之和之上至少要留的余量。
	orderingLeaseMarginSlack = 5 * time.Second

	// orderingLeaseRenewCallTimeout 是单次续租调用的预算。
	orderingLeaseRenewCallTimeout = 3 * time.Second
	// 续租失败后的退避:首次 500ms、逐次翻倍、封顶 5s,带 ±20% 抖动。
	orderingLeaseRenewBackoffMin = 500 * time.Millisecond
	orderingLeaseRenewBackoffMax = 5 * time.Second
	// orderingLeaseRenewMaxAttempts 是一轮故障里最多尝试的续租次数(成功一次即清零)。
	orderingLeaseRenewMaxAttempts = 10
)

// errOrderingLeaseReleased 是释放时取消 SQL ctx 用的原因:此时 SQL 早已结束,只为不泄漏 ctx。
var errOrderingLeaseReleased = errors.New("ordering lease released")

func orderingLockKey(topic string, task *db_proto.DBTask) string {
	return fmt.Sprintf("kafka:ordering:%s:%d:%s", topic, task.Key, task.MsgType)
}

// orderingLeaseTiming 是租约用到的全部时间参数与时间源。生产用 orderingLeaseTimingDefault;单测替换这个包级变量
// (并在 t.Cleanup 里还原),注入可控的 now / after / jitter,不依赖墙钟。取锁时整份拷进租约,之后不再读包级变量。
type orderingLeaseTiming struct {
	ttl         time.Duration
	renewEvery  time.Duration
	margin      time.Duration
	callTimeout time.Duration
	backoffMin  time.Duration
	backoffMax  time.Duration
	maxAttempts int

	// now 必须带单调时钟读数(time.Now 满足):截止时间的比较不受墙钟回拨影响。
	now    func() time.Time
	after  func(time.Duration) <-chan time.Time
	jitter func(time.Duration) time.Duration
}

var orderingLeaseTimingDefault = orderingLeaseTiming{
	ttl:         orderingLockTTL,
	renewEvery:  orderingLockRenewInterval,
	margin:      orderingLeaseSafetyMargin,
	callTimeout: orderingLeaseRenewCallTimeout,
	backoffMin:  orderingLeaseRenewBackoffMin,
	backoffMax:  orderingLeaseRenewBackoffMax,
	maxAttempts: orderingLeaseRenewMaxAttempts,
	now:         time.Now,
	after:       time.After,
	jitter:      jitterOrderingLeaseBackoff,
}

// jitterOrderingLeaseBackoff 取 0.8~1.2 倍:同一次 Redis 故障里所有在途写的续租同时失败,不加抖动会同时重试。
func jitterOrderingLeaseBackoff(d time.Duration) time.Duration {
	return time.Duration(float64(d) * (0.8 + 0.4*rand.Float64()))
}

// backoff 返回第 attempt 次失败(从 1 起)之后的等待:min(backoffMin<<(attempt-1), backoffMax),再加抖动。
func (t orderingLeaseTiming) backoff(attempt int) time.Duration {
	if attempt < 1 {
		attempt = 1
	}
	d := t.backoffMax
	if attempt <= 32 {
		if shifted := t.backoffMin << (attempt - 1); shifted > 0 && shifted < d {
			d = shifted
		}
	}
	return t.jitter(d)
}

// orderingLease 是一次持锁。生命周期:acquireOrderingLock 建出并起看门狗 → (写)runDBOp 里 beginSQL / verify →
// handleTask 结束时 releaseOrderingLock。stop / done 由 releaseOrderingLock 收掉看门狗。
type orderingLease struct {
	result  *locker.TryLockResult
	lockKey string // 只用于日志(locker 的完整键在它的前缀之后)
	timing  orderingLeaseTiming

	stop chan struct{}
	done chan struct{}

	// sqlCtx 以 context.Background 为父,只在 setLost(原因为 lostErr)或释放(原因为 errOrderingLeaseReleased)时取消。
	sqlCtx    context.Context
	cancelSQL context.CancelCauseFunc

	mu sync.Mutex
	// validUntil 是锁在 Redis 上最早可能过期的时刻(本地单调时钟):取锁 / 续租成功时以「发出请求的时刻 + TTL」
	// 保守推算,只向前推。
	validUntil   time.Time
	lostErr      error
	lostReason   string
	sqlStarted   bool // beginSQL 放行过一次(只用于释放时的日志)
	sqlAbandoned bool // SQL 在途时被失租取消:释放时不删锁,留给 TTL
}

// setLost 记下失租(只记第一次),计数并取消 SQL。lostErr 包着 errOrderingLeaseLost 与具体原因。
func (l *orderingLease) setLost(reason string, cause error) {
	l.mu.Lock()
	if l.lostErr != nil {
		l.mu.Unlock()
		return
	}
	l.lostErr = fmt.Errorf("%w: %w", errOrderingLeaseLost, cause)
	l.lostReason = reason
	lostErr := l.lostErr
	l.mu.Unlock()

	metrics.CountOrderingLeaseLost(reason)
	if l.cancelSQL != nil {
		l.cancelSQL(lostErr)
	}
}

// lost 返回失租错误;未失租时为 nil。
func (l *orderingLease) lost() error {
	l.mu.Lock()
	defer l.mu.Unlock()
	return l.lostErr
}

// abandoned 报告 SQL 是否在途时被失租取消。
func (l *orderingLease) abandoned() bool {
	l.mu.Lock()
	defer l.mu.Unlock()
	return l.sqlAbandoned
}

// localDeadline 是本地截止时间:过了它就不再发 SQL,看门狗也放弃续租。
func (l *orderingLease) localDeadline() time.Time {
	l.mu.Lock()
	defer l.mu.Unlock()
	return l.validUntil.Add(-l.timing.margin)
}

// extendValidUntil 只向前推 validUntil(并发的 verify 与看门狗续租谁晚到都不会把它往回拉)。
func (l *orderingLease) extendValidUntil(t time.Time) {
	l.mu.Lock()
	defer l.mu.Unlock()
	if t.After(l.validUntil) {
		l.validUntil = t
	}
}

// acquireOrderingLock 取排序锁并起续租看门狗。锁被别人占着时返回包着 errOrderingLockBusy 的错误(不耗重试预算)。
func acquireOrderingLock(ctx context.Context, w *worker, task *db_proto.DBTask) (*orderingLease, error) {
	timing := orderingLeaseTimingDefault
	key := orderingLockKey(w.topic, task)
	// start 取在 SETNX 之前:锁的真实过期时刻不早于 start + TTL,以它推算 validUntil 是保守的。
	start := timing.now()
	stepStart := time.Now()
	result, err := w.locker.TryLock(ctx, key, timing.ttl)
	metrics.ObserveRedisStep("write", metrics.RedisStepLockAcquire, time.Since(stepStart))
	if err != nil {
		return nil, fmt.Errorf("acquire cross-instance ordering lock: taskID=%s: %w", task.TaskId, err)
	}
	if !result.IsLocked() {
		return nil, fmt.Errorf("%w: taskID=%s key=%d msgType=%s", errOrderingLockBusy, task.TaskId, task.Key, task.MsgType)
	}

	sqlCtx, cancelSQL := context.WithCancelCause(context.Background())
	lease := &orderingLease{
		result:     result,
		lockKey:    key,
		timing:     timing,
		stop:       make(chan struct{}),
		done:       make(chan struct{}),
		sqlCtx:     sqlCtx,
		cancelSQL:  cancelSQL,
		validUntil: start.Add(timing.ttl),
	}
	go lease.watch()
	return lease, nil
}

// watch 是续租看门狗:正常每 renewEvery 续一次;出错按退避在本地截止时间之内重试。
//
// 刻意不监听 worker ctx:停机不等于失租。停机时在途的写 SQL 照常跑完、照常核锁与释放(Stop 的顺序见
// KeyOrderedKafkaConsumer.Stop);在这里把停机当成失租,就会取消 SQL 并把锁留到 TTL,白白让该玩家的存盘排队 2 分钟。
func (l *orderingLease) watch() {
	defer close(l.done)
	next := l.timing.renewEvery
	attempts := 0
	for {
		remaining := l.localDeadline().Sub(l.timing.now())
		if remaining <= 0 {
			l.setLost(metrics.LeaseLostRenewDeadline,
				fmt.Errorf("ordering lease reached its local deadline without a successful renewal (attempts=%d)", attempts))
			return
		}
		wait := next
		if remaining < wait {
			wait = remaining
		}
		select {
		case <-l.stop:
			return
		case <-l.timing.after(wait):
		}
		select {
		case <-l.stop:
			return
		default:
		}
		if !l.timing.now().Before(l.localDeadline()) {
			l.setLost(metrics.LeaseLostRenewDeadline,
				fmt.Errorf("ordering lease reached its local deadline without a successful renewal (attempts=%d)", attempts))
			return
		}

		callStart := l.timing.now()
		callCtx, cancel := context.WithTimeout(context.Background(), l.timing.callTimeout)
		owned, err := l.result.Extend(callCtx, l.timing.ttl)
		cancel()
		switch {
		case err != nil:
			attempts++
			if attempts >= l.timing.maxAttempts {
				l.setLost(metrics.LeaseLostRenewExhausted,
					fmt.Errorf("ordering lease renewal failed %d times: %w", attempts, err))
				return
			}
			metrics.CountOrderingLeaseRenewRetry()
			if ok, suppressed := storeLogLimiter.allow("ordering_lease_renew", time.Now()); ok {
				logx.Errorf("ordering lease renewal failed, retrying within the lease budget: key=%s attempt=%d/%d err=%v suppressed_since_last=%d",
					l.lockKey, attempts, l.timing.maxAttempts, err, suppressed)
			}
			next = l.timing.backoff(attempts)
		case !owned:
			l.setLost(metrics.LeaseLostOwnerChanged, errors.New("ordering lease renewal found a different owner"))
			return
		default:
			l.extendValidUntil(callStart.Add(l.timing.ttl))
			attempts = 0
			next = l.timing.renewEvery
		}
	}
}

// beginSQL 在发出写 SQL 之前调用一次。已失租,或已过本地截止时间(看门狗还没来得及判),返回失租错误,
// 调用方一条 SQL 都不发;否则交出只随失租取消的 sqlCtx,以及 SQL 结束后必须调用一次的 finish。
func (l *orderingLease) beginSQL() (context.Context, func(handlerErr error), error) {
	if err := l.lost(); err != nil {
		return nil, nil, err
	}
	if !l.timing.now().Before(l.localDeadline()) {
		l.setLost(metrics.LeaseLostRenewDeadline, errors.New("ordering lease past its local deadline before SQL"))
		return nil, nil, l.lost()
	}
	l.mu.Lock()
	l.sqlStarted = true
	l.mu.Unlock()
	return l.sqlCtx, l.finishSQL, nil
}

// finishSQL 在 SQL 返回后调用:SQL 失败且 sqlCtx 已被取消,说明它是在途时被失租取消的 —— 服务端可能还在执行,
// 释放阶段不删锁。database/sql 还在等连接时就被取消的也会落到这里,保守地按在途处理。
func (l *orderingLease) finishSQL(handlerErr error) {
	if handlerErr == nil || l.sqlCtx.Err() == nil {
		return
	}
	l.mu.Lock()
	l.sqlAbandoned = true
	l.mu.Unlock()
}

// verify 在共享缓存回写之前做一次带 token 的核锁(Lua:锁值仍是自己的 token 才续期)。
//
// ctx 已取消(claim 被收回、停机)导致的失败不算失租:锁大概率仍是自己的,释放时照常删锁;任务本身照常延后。
func (l *orderingLease) verify(ctx context.Context) error {
	if err := l.lost(); err != nil {
		return err
	}
	start := l.timing.now()
	stepStart := time.Now()
	owned, err := l.result.Extend(ctx, l.timing.ttl)
	metrics.ObserveRedisStep("write", metrics.RedisStepLeaseVerify, time.Since(stepStart))
	if err != nil {
		if ctx.Err() != nil {
			return fmt.Errorf("verify ordering lease: %w", err)
		}
		l.setLost(metrics.LeaseLostVerifyError, fmt.Errorf("verify ordering lease: %w", err))
		return l.lost()
	}
	if !owned {
		l.setLost(metrics.LeaseLostOwnerChanged, errors.New("ordering lease token is no longer the Redis lock owner"))
		return l.lost()
	}
	l.extendValidUntil(start.Add(l.timing.ttl))
	return nil
}

// runTokenScript 把核令牌、发布和续期合并；取消和单步超时不伪判失租。
func (l *orderingLease) runTokenScript(ctx context.Context, script *redis.Script, keys []string, args ...interface{}) (int64, error) {
	if err := l.lost(); err != nil {
		return 0, err
	}
	start := l.timing.now()
	argv := append([]interface{}{l.timing.ttl.Milliseconds()}, args...)
	result, err := l.result.RunTokenScript(ctx, script, keys, argv...)
	if err != nil {
		var serverErr redis.Error
		if ctx.Err() == nil && !errors.As(err, &serverErr) {
			l.setLost(metrics.LeaseLostVerifyError, err)
		}
		return 0, fmt.Errorf("%w: %w", errFenceRedis, err)
	}
	if result == 0 {
		l.setLost(metrics.LeaseLostOwnerChanged, errors.New("令牌脚本发现锁已易主"))
		return 0, l.lost()
	}
	if result > 0 {
		l.extendValidUntil(start.Add(l.timing.ttl))
	}
	return result, nil
}

// releaseOrderingLock 收掉看门狗并释放锁。SQL 在途时被失租取消的不删锁,留给 TTL。
//
// Release must still run after the caller's context is canceled: the worker context is canceled by Stop() before
// in-flight tasks unwind, and releasing with it would always fail, leaving the lock to expire by TTL (2 minutes)
// while every other instance queues behind it. So detach from the cancellation and bound the call instead.
// Token-checked Lua prevents this owner from deleting a successor's lock after TTL expiry.
func releaseOrderingLock(ctx context.Context, lease *orderingLease, task *db_proto.DBTask) {
	if lease == nil {
		return
	}
	close(lease.stop)
	<-lease.done
	// SQL 早已结束,这里取消只为释放 ctx 的资源。
	defer lease.cancelSQL(errOrderingLeaseReleased)

	if lease.abandoned() {
		metrics.CountOrderingLockRelease(metrics.LockReleaseSkippedSQLAbandoned)
		logx.Errorf("ordering lock kept until TTL after its SQL was canceled on lease loss: taskID=%s key=%d msgType=%s cause=%v",
			task.TaskId, task.Key, task.MsgType, lease.lost())
		return
	}

	releaseCtx, cancel := context.WithTimeout(context.WithoutCancel(ctx), orderingLockReleaseTimeout)
	defer cancel()
	stepStart := time.Now()
	released, err := lease.result.Release(releaseCtx)
	metrics.ObserveRedisStep("write", metrics.RedisStepLockRelease, time.Since(stepStart))
	switch {
	case err != nil:
		metrics.CountOrderingLockRelease(metrics.LockReleaseError)
		logx.Errorf("release cross-instance ordering lock failed: taskID=%s key=%d msgType=%s err=%v",
			task.TaskId, task.Key, task.MsgType, err)
	case !released && lease.lost() != nil:
		metrics.CountOrderingLockRelease(metrics.LockReleaseSkippedSQLAbandoned)
	case !released:
		lease.mu.Lock()
		sqlStarted := lease.sqlStarted
		lease.mu.Unlock()
		metrics.CountOrderingLockRelease(metrics.LockReleaseNotOwner)
		logx.Errorf("ordering lock already expired or taken over at release; MySQL may hold an older snapshot until the next save: taskID=%s key=%d msgType=%s leaseLost=%v sqlStarted=%v",
			task.TaskId, task.Key, task.MsgType, lease.lost(), sqlStarted)
	default:
		metrics.CountOrderingLockRelease(metrics.LockReleaseReleased)
	}
}

// leaseMarginCoversLockWaits 检查 margin 是否盖得住服务端一条被遗弃语句的最长存活时间(两个会话锁等待之和 + 余量)。
// 任一锁等待 ≤0 表示没设(服务端默认:lock_wait_timeout 一年、innodb_lock_wait_timeout 50 秒),被遗弃的语句没有上界。
// 调用方只打 ERROR 不拒启:它削弱的是失租之后的保护,拒启等于停掉整个 zone 的存盘。
func leaseMarginCoversLockWaits(lockWaitSeconds, innodbLockWaitSeconds int, margin time.Duration) error {
	if lockWaitSeconds <= 0 || innodbLockWaitSeconds <= 0 {
		return fmt.Errorf("session lock waits are not set (lock_wait_timeout=%d innodb_lock_wait_timeout=%d, server defaults are one year / 50s): "+
			"an orphaned statement can outlive the ordering lock", lockWaitSeconds, innodbLockWaitSeconds)
	}
	need := time.Duration(lockWaitSeconds+innodbLockWaitSeconds)*time.Second + orderingLeaseMarginSlack
	if need > margin {
		return fmt.Errorf("ordering lease margin %v is below lock_wait_timeout %ds + innodb_lock_wait_timeout %ds + %v slack: "+
			"an orphaned statement can outlive the ordering lock", margin, lockWaitSeconds, innodbLockWaitSeconds, orderingLeaseMarginSlack)
	}
	return nil
}
