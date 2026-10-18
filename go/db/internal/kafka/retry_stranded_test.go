package kafka

// R7 复核 [2] 的单测:已认领进本实例 processing、该搬走却因 Redis 出错没搬成的收据登记进待重搬表,
// 重试协程下一拍重搬(retry_stranded.go)。故障用 retry_claim_guard_test.go 的 scriptHook 注入,只作用于
// moveRetryReceiptScript;全部跑在 miniredis 上,不依赖墙钟。

import (
	"context"
	"testing"

	db_config "db/internal/config"
	"db/internal/metrics"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// withStranded 给直接拼出来的消费者及其 worker 装上待重搬表(生产由 NewKeyOrderedKafkaConsumer / buildWorker 装)。
func withStranded(c *KeyOrderedKafkaConsumer) *strandedRetries {
	c.stranded = newStrandedRetries()
	for _, w := range c.workers {
		w.stranded = c.stranded
	}
	return c.stranded
}

func strandedGauge(t *testing.T) float64 {
	t.Helper()
	return metricCount(t, "db_retry_stranded_receipts", map[string]string{})
}

// claim 撤销后 handleTask 搬回 p{origin} 时撞上 Redis 错误:修前收据留在本实例 processing 直到重启;
// 现在登记进待重搬表,下一拍重搬回 p0,补记 requeued_revoked,表与 gauge 归零。
func TestRetryStranded_RevokedRequeueFailureIsMovedNextTick(t *testing.T) {
	f := newRoutingFixture(t)
	c := f.consumer("db-a:6000", db_config.RetryRoutingOwner, 1)
	stranded := withStranded(c)
	claimCtx, revoke := context.WithCancel(context.Background())
	defer revoke()
	lease := c.owned.acquire(0, claimCtx)
	pl, _ := routedPayload(t, 9741, 41, 0, 0)
	f.push(t, pKey(0), pl)
	c.consumeRetryQueue()
	queued := drainDispatched(c.workers[0])
	require.Len(t, queued, 1)

	revoke()
	f.rc.AddHook(newScriptHook(moveRetryReceiptScript, hookFaultBeforeSend, 1))
	c.workers[0].handleTask(queued[0], true)
	assert.Equal(t, [][]byte{pl}, f.list(t, c.retryProcessingKey), "the failed move leaves the receipt in processing")
	assert.Zero(t, f.llen(t, pKey(0)))
	assert.Equal(t, 1, stranded.size())
	assert.Equal(t, float64(1), strandedGauge(t))
	assert.Zero(t, lease.inflightCount(), "the task reached its terminal state; the claim drain is not held up")

	before := routeCount(t, metrics.RetryModeOwner, metrics.RetryRouteRequeuedRevoked)
	c.consumeRetryQueue()
	assert.Zero(t, f.llen(t, c.retryProcessingKey))
	assert.Equal(t, [][]byte{pl}, f.list(t, pKey(0)), "the receipt returns byte-for-byte to its origin partition")
	assert.Zero(t, stranded.size())
	assert.Zero(t, strandedGauge(t))
	assert.Equal(t, before+1, routeCount(t, metrics.RetryModeOwner, metrics.RetryRouteRequeuedRevoked))
	assert.Empty(t, f.h.callsForKey(9741), "a revoked retry never reaches the database")
}

// 重试执行失败后写回 ready 时撞上 Redis 错误:登记的是 RetryCount 已更新的载荷;下一拍重搬,收据随即被属主重新认领。
// Redis 仍出错的那一拍不丢、不重复:表里的条目留到下一拍。
func TestRetryStranded_RescheduleFailureKeepsTheReplacementPayload(t *testing.T) {
	f := newRoutingFixture(t)
	c := f.consumer("db-a:6000", db_config.RetryRoutingOwner, 1)
	stranded := withStranded(c)
	lease := c.owned.acquire(0, context.Background())
	pl, task := routedPayload(t, 9751, 51, 0, 0)
	f.push(t, pKey(0), pl)
	c.consumeRetryQueue()
	queued := drainDispatched(c.workers[0])
	require.Len(t, queued, 1)

	f.h.programFailure(task.TaskId, 1)
	f.rc.AddHook(newScriptHook(moveRetryReceiptScript, hookFaultBeforeSend, 2))
	c.workers[0].handleTask(queued[0], true)
	require.Equal(t, 1, stranded.size())
	assert.Equal(t, [][]byte{pl}, f.list(t, c.retryProcessingKey))

	// 第一拍重搬仍失败(第 2 次注入):条目留着,什么都没动;租约 p0 为空,不认领。
	c.consumeRetryQueue()
	assert.Equal(t, 1, stranded.size())
	assert.Equal(t, [][]byte{pl}, f.list(t, c.retryProcessingKey))
	assert.Empty(t, drainDispatched(c.workers[0]))

	// 第二拍重搬成功:p0 收到 RetryCount=1 的载荷,同一拍里被属主认领、派发。
	c.consumeRetryQueue()
	assert.Zero(t, stranded.size())
	redispatched := drainDispatched(c.workers[0])
	require.Len(t, redispatched, 1)
	assert.Equal(t, int32(2), redispatched[0].dbTask.RetryCount, "one attempt persisted + one claim in memory")
	processing := f.list(t, c.retryProcessingKey)
	require.Len(t, processing, 1)
	persisted, _, _, _ := decodeRetry(t, processing[0])
	assert.Equal(t, int32(1), persisted.RetryCount, "the stranded move published the replacement, not the original receipt")
	assert.Equal(t, 1, lease.inflightCount())
}

// 收据已不在 processing(moved=false、err==nil:多半是重发时首发已执行)不登记;没有表时照旧只打 ERROR。
func TestRetryStranded_OnlyRedisErrorsAreQueued(t *testing.T) {
	f := newRoutingFixture(t)
	c := f.consumer("db-a:6000", db_config.RetryRoutingOwner, 2)
	stranded := withStranded(c)
	missing, _ := routedPayload(t, 9761, 61, 1, 0)

	assert.False(t, c.moveClaimedRetry(missing, pKey(1), metrics.RetryRouteMisrouted))
	assert.Zero(t, stranded.size(), "a receipt that is no longer in processing is not queued")

	c.stranded = nil
	f.rc.AddHook(newScriptHook(moveRetryReceiptScript, hookFaultBeforeSend, 1))
	assert.False(t, c.moveClaimedRetry(missing, pKey(1), metrics.RetryRouteMisrouted), "without a table the old behaviour stays")
	assert.Zero(t, stranded.size())
}
