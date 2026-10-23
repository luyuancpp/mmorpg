package routing

import (
	"context"
	"errors"
	"testing"
	"time"

	goredis "github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// AcquirePlayerLock 拿锁失败时的回滚:SETNX 的结果不明(服务端已执行、客户端没拿到成功回包)时,
// 必须按 token 把可能已落地的锁释放掉,否则该玩家的写要等 PlayerLockTTLSec 过期才恢复;
// 按 token 释放又保证别人持有的锁不受影响。dev 模式下 mapping 锁与数据锁在同一个 miniredis 里,两把都能直接看到。

var errSetNXReplyLost = errors.New("connection reset after SETNX was sent")

// lostReplySetNXClient 让数据侧 SETNX 真正写入,再把回包换成错误:模拟「服务端已执行、回包丢失」。
type lostReplySetNXClient struct {
	*goredis.Client
}

func (c *lostReplySetNXClient) SetNX(ctx context.Context, key string, value interface{}, expiration time.Duration) *goredis.BoolCmd {
	if err := c.Client.SetNX(ctx, key, value, expiration).Err(); err != nil {
		return goredis.NewBoolResult(false, err)
	}
	return goredis.NewBoolResult(false, errSetNXReplyLost)
}

// retriedSetNXClient 让数据侧 SETNX 真正写入,再回 false:模拟客户端对断连自动重试,第二次撞上第一次写入的自己的 token。
type retriedSetNXClient struct {
	*goredis.Client
}

func (c *retriedSetNXClient) SetNX(ctx context.Context, key string, value interface{}, expiration time.Duration) *goredis.BoolCmd {
	if err := c.Client.SetNX(ctx, key, value, expiration).Err(); err != nil {
		return goredis.NewBoolResult(false, err)
	}
	return goredis.NewBoolResult(false, nil)
}

// 数据侧 SETNX 已写入却回错误:两把锁都按 token 释放,随后立刻能重新拿到。
func TestAcquirePlayerLock_DataSetNXReplyLostReleasesBothLocks(t *testing.T) {
	r, mr := newTestRouter(t)
	devClient := devClientOf(t, r)

	token, ok, err := r.AcquirePlayerLock(context.Background(), &lostReplySetNXClient{Client: devClient}, 791)
	require.ErrorIs(t, err, errSetNXReplyLost)
	assert.False(t, ok)
	assert.Empty(t, token)

	requireNoPlayerLocks(t, mr, 791)
	_, ok, err = r.AcquirePlayerLock(context.Background(), devClient, 791)
	require.NoError(t, err)
	assert.True(t, ok, "lock must be re-acquirable right away, not after the TTL")
}

// 数据侧 SETNX 回 false,但锁其实是自己这次写入的(重试撞上自己):同样两把都释放。
func TestAcquirePlayerLock_DataSetNXRetryHitsOwnTokenReleasesBothLocks(t *testing.T) {
	r, mr := newTestRouter(t)
	devClient := devClientOf(t, r)

	token, ok, err := r.AcquirePlayerLock(context.Background(), &retriedSetNXClient{Client: devClient}, 792)
	require.NoError(t, err)
	assert.False(t, ok)
	assert.Empty(t, token)

	requireNoPlayerLocks(t, mr, 792)
	_, ok, err = r.AcquirePlayerLock(context.Background(), devClient, 792)
	require.NoError(t, err)
	assert.True(t, ok, "lock must be re-acquirable right away, not after the TTL")
}

// 数据锁真被别人持有:回滚只释放自己的 mapping 锁,别人的数据锁原样保留。
func TestAcquirePlayerLock_DataLockHeldByOtherIsUntouched(t *testing.T) {
	r, mr := newTestRouter(t)
	devClient := devClientOf(t, r)
	require.NoError(t, mr.Set(PlayerDataLockKey(793), "other-owner"))

	token, ok, err := r.AcquirePlayerLock(context.Background(), devClient, 793)
	require.NoError(t, err)
	assert.False(t, ok)
	assert.Empty(t, token)

	got, err := mr.Get(PlayerDataLockKey(793))
	require.NoError(t, err)
	assert.Equal(t, "other-owner", got, "another owner's data lock must survive the rollback")
	assert.False(t, mr.Exists(mappingPlayerLockKey(793)), "our own mapping lock must be rolled back")
}

// mapping 锁冲突:失败方的回滚释放不能动现任持锁者的两把锁。
func TestAcquirePlayerLock_MappingConflictLeavesHolderLocks(t *testing.T) {
	r, mr := newTestRouter(t)
	devClient := devClientOf(t, r)
	holder, ok, err := r.AcquirePlayerLock(context.Background(), devClient, 794)
	require.NoError(t, err)
	require.True(t, ok)

	token, ok, err := r.AcquirePlayerLock(context.Background(), devClient, 794)
	require.NoError(t, err)
	assert.False(t, ok)
	assert.Empty(t, token)

	for _, key := range []string{mappingPlayerLockKey(794), PlayerDataLockKey(794)} {
		got, err := mr.Get(key)
		require.NoError(t, err, key)
		assert.Equal(t, holder, got, "holder's lock %s must survive the loser's rollback", key)
	}
}
