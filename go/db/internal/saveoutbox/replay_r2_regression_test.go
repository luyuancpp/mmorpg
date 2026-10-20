package saveoutbox

import (
	"context"
	"strconv"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"shared/savebarrier"
)

// 第二轮审计 data-overwrite-rollback-audit-r2-20261019.md §3.1 r2-db#1、§3.2 r2-db#3 的回归用例。
// 只用改动前就有的接口写成，可在旧代码上先跑出红：
//   - r2-db#1：旧认领脚本先把 attempts+1 再交出意图，broker 不可用时每次失败都计次，约 21 分钟后停在 exhausted，恢复后不再投；
//   - r2-db#3：旧在途戳只取较大值，时钟超前的实例写下的未来时刻会把补投推迟「超前量 + 窗口」，且后来的正确时刻覆盖不了它。

// r2-db#1：broker 一直不可用时，失败的发布不消耗尝试次数；恢复后照常补投，而不是停在 exhausted。
func TestPublishFailureDoesNotConsumeAttempts(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	in := testIntent(t, fixtureID, 7, 9, 1)
	storeObservedIntent(t, rc, fixtureID, 1, in)
	p := &recordingPublisher{failAt: 1} // 从不成功：失败不入 tasks，下一次仍满足 failAt
	r := testReplayWith(t, rc, p, nil, Config{RetryAfter: time.Second, MaxRetryAfter: 3 * time.Second, MaxAttempts: 3, RecheckAfter: time.Minute})
	now := fixtureNowMs
	for i := 0; i < 6; i++ { // 是 MaxAttempts 的两倍
		at := now
		r.bucket, r.now = 0, func() time.Time { return time.UnixMilli(at) }
		_, err := r.Tick(ctx)
		require.Error(t, err, "第 %d 次:broker 不可用必须报错,不能被当成 exhausted 静默跳过", i+1)
		require.Equal(t, "0", rc.HGet(ctx, savebarrier.Key(fixtureID), fieldAttempts).Val(), "第 %d 次发布失败不计次", i+1)
		now += time.Minute.Milliseconds()
	}
	require.Zero(t, resultCount(r, "exhausted"))

	p.failAt = 0 // Kafka 恢复
	at := now
	r.bucket, r.now = 0, func() time.Time { return time.UnixMilli(at) }
	n, err := r.Tick(ctx)
	require.NoError(t, err)
	require.Equal(t, 2, n, "恢复后照常补投")
	require.Equal(t, "1", rc.HGet(ctx, savebarrier.Key(fixtureID), fieldAttempts).Val(), "两条都经 broker 确认才计一次")
	require.Equal(t, in.version, rc.HGet(ctx, savebarrier.Key(fixtureID), "pending").Val())
}

// r2-db#1：两条里只确认了一条同样不计次(下一次两条都重投，SQL 围栏按 equal 吸收重复)。
func TestPartialPublishDoesNotConsumeAttempt(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	storeObservedIntent(t, rc, fixtureID, 1, testIntent(t, fixtureID, 7, 9, 1))
	p := &recordingPublisher{failAt: 2}
	r := testReplay(t, rc, p)
	n, err := r.Tick(ctx)
	require.Error(t, err)
	require.Equal(t, 1, n)
	require.Equal(t, "0", rc.HGet(ctx, savebarrier.Key(fixtureID), fieldAttempts).Val())
}

// r2-db#1：发布失败后整个补投器暂停 RetryAfter，不在 broker 故障期间逐拍认领、逐个推迟其他玩家的意图。
func TestPublishFailurePausesReplay(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	storeObservedIntent(t, rc, fixtureID, 1, testIntent(t, fixtureID, 7, 9, 1))
	storeObservedIntent(t, rc, 128, 1, testIntent(t, 128, 8, 1, 1))
	p := &recordingPublisher{failAt: 1}
	r := testReplay(t, rc, p) // RetryAfter = 1s
	_, err := r.Tick(ctx)
	require.Error(t, err)
	require.Equal(t, 1, p.calls)

	r.bucket, r.now = 0, func() time.Time { return time.UnixMilli(fixtureNowMs + 999) }
	n, err := r.Tick(ctx)
	require.NoError(t, err, "暂停期内整拍跳过,不再报错刷日志")
	require.Zero(t, n)
	require.Equal(t, 1, p.calls, "暂停期内不碰 broker")
	require.Equal(t, 1.0, resultCount(r, "publish_paused"))

	p.failAt = 0
	r.bucket, r.now = 0, func() time.Time { return time.UnixMilli(fixtureNowMs + 1_000) }
	n, err = r.Tick(ctx)
	require.NoError(t, err)
	require.Equal(t, 4, n, "暂停结束后两名玩家的意图都照常补投")
}

// r2-db#1 的边界：意图本身不合法(不会因为 broker 恢复而变好)仍按次数走到 exhausted，不无限重试。
func TestInvalidIntentStillConsumesAttempts(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	poison := testIntent(t, fixtureID, 7, 9, 1)
	poison.first = "损坏的DBTask"
	storeObservedIntent(t, rc, fixtureID, 1, poison)
	p := &recordingPublisher{}
	r := testReplayWith(t, rc, p, nil, Config{RetryAfter: time.Second, MaxAttempts: 2, RecheckAfter: time.Minute})
	now := fixtureNowMs
	for i := 0; i < 3; i++ {
		at := now
		r.bucket, r.now = 0, func() time.Time { return time.UnixMilli(at) }
		_, err := r.Tick(ctx)
		require.NoError(t, err)
		now += time.Minute.Milliseconds()
	}
	require.Empty(t, p.tasks)
	require.Equal(t, "2", rc.HGet(ctx, savebarrier.Key(fixtureID), fieldAttempts).Val())
	require.Equal(t, 1.0, resultCount(r, "exhausted"))
}

// r2-db#3：时钟超前的实例写下远在未来的在途戳，补投不能因此推迟「超前量 + 窗口」；超过 now+窗口的戳按非法截掉。
func TestFutureSkewedRetryStampDoesNotBlockReplay(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	storeObservedIntent(t, rc, fixtureID, 1, testIntent(t, fixtureID, 7, 9, 1))
	require.NoError(t, NoteRetryDeferred(ctx, rc, fixtureID, time.UnixMilli(fixtureNowMs+time.Hour.Milliseconds())))
	p := &recordingPublisher{}
	r := testReplay(t, rc, p) // InflightWindow 缺省 2min
	n, err := r.Tick(ctx)
	require.NoError(t, err)
	require.Equal(t, 2, n, "超前一小时的戳不得推迟补投")
	require.False(t, rc.HExists(ctx, savebarrier.Key(fixtureID), fieldRetryStamp).Val(), "非法戳被截掉")
}

// r2-db#3：窗口之内的未来戳(实例间正常的小偏差)仍按在途处理，推迟上限是两个窗口。
func TestSlightlyFutureRetryStampStillDefers(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	storeObservedIntent(t, rc, fixtureID, 1, testIntent(t, fixtureID, 7, 9, 1))
	require.NoError(t, NoteRetryDeferred(ctx, rc, fixtureID, time.UnixMilli(fixtureNowMs+(2*time.Minute).Milliseconds())))
	p := &recordingPublisher{}
	r := testReplay(t, rc, p)
	n, err := r.Tick(ctx)
	require.NoError(t, err)
	require.Zero(t, n)
	require.Equal(t, 1.0, resultCount(r, "inflight_deferred"))
	require.True(t, rc.HExists(ctx, savebarrier.Key(fixtureID), fieldRetryStamp).Val())
}

// r2-db#3：在途戳取最后一次写入，不取较大值；否则一个超前的值会一直盖住之后正确的时刻。
func TestRetryStampLastWriterWins(t *testing.T) {
	rc := fixtureRedis(t)
	ctx := context.Background()
	storeIntent(t, rc, fixtureID, 1, testIntent(t, fixtureID, 7, 9, 1))
	require.NoError(t, NoteRetryDeferred(ctx, rc, fixtureID, time.UnixMilli(fixtureNowMs+time.Hour.Milliseconds())))
	require.NoError(t, NoteRetryDeferred(ctx, rc, fixtureID, time.UnixMilli(fixtureNowMs)))
	require.Equal(t, strconv.FormatInt(fixtureNowMs, 10), rc.HGet(ctx, savebarrier.Key(fixtureID), fieldRetryStamp).Val())
}
