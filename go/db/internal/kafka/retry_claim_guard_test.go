package kafka

// R7 复核 [3] / [7] 的单测:认领与收据 ack 在 go-redis 重发下的幂等、结果未知的认领由下一拍 resolve、实例生效模式登记。
//
// scriptHook 是挂在 go-redis 上的测试 Hook,只作用于指定脚本的 EVAL,前 times 次按 mode 处理:
//   - hookFaultBeforeSend:不发给 Redis,直接返回 err(脚本没执行);
//   - hookLostReply:真正执行一次,再向调用方返回 err(回包丢了,go-redis 的重发也都失败);
//   - hookResend:真正执行一次、丢掉回包,再原样执行一次并返回第二次的结果 —— 即 go-redis 读超时 / EOF 后的原样重发。
// go-redis 的 Hook 在它自己的重试循环之外,所以这里调两次 next 就是「同一条命令、同样的参数到达服务端两次」。
// 全部跑在 miniredis 上,不依赖墙钟。

import (
	"context"
	"errors"
	"sync"
	"testing"

	db_config "db/internal/config"
	"db/internal/metrics"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

type scriptHookMode int

const (
	hookFaultBeforeSend scriptHookMode = iota
	hookLostReply
	hookResend
)

type scriptHook struct {
	script string
	mode   scriptHookMode
	err    error

	mu       sync.Mutex
	times    int             // 还要处理几次;0 之后一律放行
	captured [][]interface{} // 被处理的那几次 EVAL 的完整参数
}

func newScriptHook(script string, mode scriptHookMode, times int) *scriptHook {
	return &scriptHook{script: script, mode: mode, times: times, err: fakeTimeoutNetErr{}}
}

// isEvalOf 报告 cmd 是不是脚本 script 的 EVAL。
func isEvalOf(cmd redis.Cmder, script string) bool {
	args := cmd.Args()
	if cmd.Name() != "eval" || len(args) < 2 {
		return false
	}
	s, ok := args[1].(string)
	return ok && s == script
}

func (h *scriptHook) take(cmd redis.Cmder) bool {
	h.mu.Lock()
	defer h.mu.Unlock()
	if h.times <= 0 {
		return false
	}
	h.times--
	h.captured = append(h.captured, append([]interface{}(nil), cmd.Args()...))
	return true
}

func (h *scriptHook) capturedArgs() [][]interface{} {
	h.mu.Lock()
	defer h.mu.Unlock()
	return append([][]interface{}(nil), h.captured...)
}

func (h *scriptHook) DialHook(next redis.DialHook) redis.DialHook { return next }

func (h *scriptHook) ProcessPipelineHook(next redis.ProcessPipelineHook) redis.ProcessPipelineHook {
	return next
}

func (h *scriptHook) ProcessHook(next redis.ProcessHook) redis.ProcessHook {
	return func(ctx context.Context, cmd redis.Cmder) error {
		if !isEvalOf(cmd, h.script) || !h.take(cmd) {
			return next(ctx, cmd)
		}
		switch h.mode {
		case hookFaultBeforeSend:
			cmd.SetErr(h.err)
			return h.err
		case hookLostReply:
			_ = next(ctx, cmd)
			cmd.SetErr(h.err)
			return h.err
		default:
			_ = next(ctx, cmd)
			return next(ctx, cmd)
		}
	}
}

// R7 [3]:认领脚本的首发已在服务端执行、回包丢了,go-redis 原样重发。修前重发再 RPOPLPUSH 一次,第一次认领到的 r1
// 留在 processing 里无人派发;现在同一个 nonce 的重发只找回 r1,不再 POP。
func TestRetryClaim_ResentEvalReturnsTheFirstReceipt(t *testing.T) {
	f := newRoutingFixture(t)
	c := f.consumer("db-a:6000", db_config.RetryRoutingShared, 1)
	r1, _ := routedPayload(t, 9701, 1, 0, 0)
	r2, _ := routedPayload(t, 9702, 2, 0, 0)
	f.push(t, c.retryQueueKey, r1, r2) // LPUSH:队尾(认领端)是 r1
	f.rc.AddHook(newScriptHook(retryClaimScript, hookResend, 1))

	got, err := c.retryOwner.claim(f.ctx)
	require.NoError(t, err)
	assert.Equal(t, r1, got, "the resent EVAL returns the receipt the first execution claimed")
	assert.Equal(t, [][]byte{r1}, f.list(t, c.retryProcessingKey), "the resent EVAL must not pop a second receipt")
	assert.Equal(t, [][]byte{r2}, f.list(t, c.retryQueueKey))
	assert.Zero(t, c.retryOwner.unresolvedClaimCount())
}

// R7 [3]:认领脚本执行了、但调用方只拿到错误(结果未知):记进 unresolved,下一拍先把 r1 原样搬回 ready 的队尾,
// 同一拍里照常认领、派发;迟到执行的旧副本(同一个 nonce)不再 POP。
func TestRetryClaim_UnknownResultIsResolvedNextTick(t *testing.T) {
	f := newRoutingFixture(t)
	c := f.consumer("db-a:6000", db_config.RetryRoutingShared, 1)
	r1, t1 := routedPayload(t, 9711, 1, 0, 0)
	r2, t2 := routedPayload(t, 9712, 2, 0, 0)
	f.push(t, c.retryQueueKey, r1, r2)
	hook := newScriptHook(retryClaimScript, hookLostReply, 1)
	f.rc.AddHook(hook)

	c.consumeRetryQueue()
	assert.Empty(t, drainDispatched(c.workers[0]), "the caller only saw an error")
	assert.Equal(t, [][]byte{r1}, f.list(t, c.retryProcessingKey), "the lost-reply claim did move r1 into processing")
	require.Equal(t, 1, c.retryOwner.unresolvedClaimCount())

	c.consumeRetryQueue()
	assert.Zero(t, c.retryOwner.unresolvedClaimCount())
	dispatched := drainDispatched(c.workers[0])
	require.Len(t, dispatched, 2, "r1 went back to the tail of ready and is claimed again in the same tick")
	assert.Equal(t, t1.TaskId, dispatched[0].dbTask.TaskId)
	assert.Equal(t, t2.TaskId, dispatched[1].dbTask.TaskId)
	assert.Zero(t, f.llen(t, c.retryQueueKey))
	assert.ElementsMatch(t, [][]byte{r1, r2}, f.list(t, c.retryProcessingKey), "each receipt is in processing exactly once")

	// 旧连接里迟到执行的那次认领(同样的参数)读到 resolved:不 POP、不返回元素。
	f.push(t, c.retryQueueKey, r1)
	args := hook.capturedArgs()
	require.Len(t, args, 1)
	_, err := f.rc.Do(f.ctx, args[0]...).Result()
	assert.True(t, errors.Is(err, redis.Nil), "a late duplicate of a resolved claim must not pop: err=%v", err)
	assert.Equal(t, [][]byte{r1}, f.list(t, c.retryQueueKey))
	assert.Len(t, f.list(t, c.retryProcessingKey), 2)
}

// R7 [3]:resolve 时 Redis 仍出错 —— 记录留着,本拍不再分拣、认领(不再添新的结果未知的认领),下一拍再 resolve。
func TestRetryClaim_ResolveFailurePausesClaims(t *testing.T) {
	f := newRoutingFixture(t)
	c := f.consumer("db-a:6000", db_config.RetryRoutingShared, 1)
	r1, _ := routedPayload(t, 9721, 1, 0, 0)
	f.push(t, c.retryQueueKey, r1)
	f.rc.AddHook(newScriptHook(retryClaimScript, hookLostReply, 1))
	c.consumeRetryQueue()
	require.Equal(t, 1, c.retryOwner.unresolvedClaimCount())

	f.rc.AddHook(newScriptHook(retryClaimResolveScript, hookFaultBeforeSend, 1))
	c.consumeRetryQueue()
	assert.Equal(t, 1, c.retryOwner.unresolvedClaimCount(), "the unresolved claim is kept for the next tick")
	assert.Equal(t, [][]byte{r1}, f.list(t, c.retryProcessingKey))
	assert.Empty(t, drainDispatched(c.workers[0]), "no claim while an unknown-result claim is pending")

	c.consumeRetryQueue()
	assert.Zero(t, c.retryOwner.unresolvedClaimCount())
	require.Len(t, drainDispatched(c.workers[0]), 1)
}

// R7 [3]:processing 里有两份字节相同的收据(Kafka 重投 / 组提交重推),一次 ack 的 LREM 被 go-redis 原样重发。
// 修前重发把另一份也删掉;现在同一个 nonce 的重发只返回记下的条数,另一份留着。
func TestAckRetryReceipt_ResentEvalRemovesOneCopy(t *testing.T) {
	mr := miniredis.RunT(t)
	rc := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { _ = rc.Close() })
	ctx := context.Background()
	const processing = "kafka:retry:processing:test-db-task:db-a:6000"
	dup := []byte("same-bytes-receipt")
	require.NoError(t, rc.LPush(ctx, processing, dup, dup).Err())
	rc.AddHook(newScriptHook(ackRetryReceiptScript, hookResend, 1))

	require.NoError(t, ackRetryReceipt(ctx, rc, processing, dup))
	n, err := rc.LLen(ctx, processing).Result()
	require.NoError(t, err)
	assert.Equal(t, int64(1), n, "the resent LREM must not delete the other identical copy")

	// 不重发时照常:删掉最后一份,再 ack 一次报「不在 processing」。
	require.NoError(t, ackRetryReceipt(ctx, rc, processing, dup))
	assert.Error(t, ackRetryReceipt(ctx, rc, processing, dup))
}

// R7 [7]:续租与认领在同一原子操作里登记生效模式;正常停机 release 摘掉登记时一并摘掉模式;RetryQueueDepths 读出模式。
func TestRetryOwnership_RegistersRoutingMode(t *testing.T) {
	f := newRetryOwnershipFixture(t)
	owner := f.owner("db-a:6000")
	owner.routingMode = metrics.RetryModeOwner
	shared := f.owner("db-b:6000")
	shared.routingMode = metrics.RetryModeShared
	legacy := f.owner("db-old:6000") // routingMode 为空:不登记模式(本版本之前的镜像)
	require.NoError(t, owner.start(f.ctx))
	require.NoError(t, shared.start(f.ctx))
	require.NoError(t, legacy.renewLease(f.ctx))

	report, err := RetryQueueDepths(f.ctx, f.rc, retryTestTopic, 1)
	require.NoError(t, err)
	assert.Equal(t, map[string]string{"db-a:6000": "owner", "db-b:6000": "shared", "db-old:6000": ""}, report.InstanceModes)
	assert.Equal(t, []string{"db-a:6000"}, report.OwnerInstances())
	assert.False(t, report.LooksUnused())

	require.NoError(t, owner.release(f.ctx))
	exists, err := f.rc.HExists(f.ctx, retryInstanceModesKey(retryTestTopic), "db-a:6000").Result()
	require.NoError(t, err)
	assert.False(t, exists, "release drops the mode together with the registration")
	report, err = RetryQueueDepths(f.ctx, f.rc, retryTestTopic, 1)
	require.NoError(t, err)
	assert.Empty(t, report.OwnerInstances())

	// 被回收之后又认领的实例(租约断过但还活着):认领脚本把登记与模式一起写回。
	require.NoError(t, f.rc.LPush(f.ctx, retryTestReady, "one").Err())
	_, err = owner.claim(f.ctx)
	require.NoError(t, err)
	mode, err := f.rc.HGet(f.ctx, retryInstanceModesKey(retryTestTopic), "db-a:6000").Result()
	require.NoError(t, err)
	assert.Equal(t, "owner", mode)
}

// R7 [5]:登记表为空、各列表都是 0 时 LooksUnused 为真(retry_requeue 据此拒绝以 0 退出)。
func TestRetryQueueDepths_LooksUnused(t *testing.T) {
	f := newRetryOwnershipFixture(t)
	report, err := RetryQueueDepths(f.ctx, f.rc, retryTestTopic, 2)
	require.NoError(t, err)
	assert.True(t, report.LooksUnused())

	require.NoError(t, f.rc.LPush(f.ctx, retryDeadKey(retryTestTopic), "x").Err())
	report, err = RetryQueueDepths(f.ctx, f.rc, retryTestTopic, 2)
	require.NoError(t, err)
	assert.False(t, report.LooksUnused(), "a non-empty dead queue means the target is in use")
}
