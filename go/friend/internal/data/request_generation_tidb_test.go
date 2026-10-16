package data

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// MySQL锁回归不能证明TiDB事务契约；此组单独连真实TiDB、每物理连接pessimistic+RC，
// 使用NONCLUSTERED主键夹具验证代次与并发容量。没有DSN明确跳过，不冒充验收通过。
func TestTiDBRequestGeneration_TransactionsAndHardLimit(t *testing.T) {
	db, ctx := openFriendTiDBTestDB(t, friendTiDBTestDSN(t))
	repo := newFriendTiDBTestRepo(t, db)
	require.NoError(t, CheckRequestIDLookupIndex(ctx, db))
	first, err := repo.CreateRequest(ctx, 11, 12, AddFriendLimits{})
	require.NoError(t, err)
	repeated, err := repo.CreateRequest(ctx, 11, 12, AddFriendLimits{})
	require.NoError(t, err)
	assert.Equal(t, first.RequestID, repeated.RequestID)
	assert.False(t, repeated.Changed)
	_, err = repo.RejectRequest(ctx, first.RequestID, 12)
	require.NoError(t, err)
	second, err := repo.CreateRequest(ctx, 11, 12, AddFriendLimits{})
	require.NoError(t, err)
	require.NotEqual(t, first.RequestID, second.RequestID)
	_, err = repo.AcceptRequest(ctx, first.RequestID, 12, 10)
	assert.ErrorIs(t, err, ErrRequestNotFound)
	accepted, err := repo.AcceptRequest(ctx, second.RequestID, 12, 10)
	require.NoError(t, err)
	assert.True(t, accepted.Changed)
	accepted, err = repo.AcceptRequest(ctx, second.RequestID, 12, 10)
	require.NoError(t, err)
	assert.False(t, accepted.Changed)

	const target uint64 = 200
	ids := make([]uint64, 16)
	for i := range ids {
		change, err := repo.CreateRequest(ctx, uint64(100+i), target, AddFriendLimits{})
		require.NoError(t, err)
		ids[i] = change.RequestID
	}
	errs := runConcurrent(t, len(ids), func(i int) error {
		change, err := repo.AcceptRequest(ctx, ids[i], target, 2)
		if err == nil && !change.Changed {
			return fmt.Errorf("首次处理被误判为重放")
		}
		return err
	})
	consumed := 0
	for _, err := range errs {
		if err == nil {
			consumed++
			continue
		}
		assert.ErrorIs(t, err, ErrAcceptorFriendsFull)
	}
	assert.Equal(t, 2, consumed, "守卫应在TiDB当前读中完整复核硬上限")
	assert.EqualValues(t, 2, mustCount(t, ctx, db, "SELECT COUNT(*) FROM friend WHERE player_id=?", target))
	assertFriendInvariants(t, ctx, db)

	_, err = db.ExecContext(ctx, `INSERT INTO friend_request
		(from_player_id,to_player_id,request_time_ms,status,updated_ms) VALUES(501,502,123,1,456)`)
	require.NoError(t, err)
	entries, err := repo.GetPendingRequests(ctx, 502)
	require.NoError(t, err)
	require.Len(t, entries, 1)
	require.NotZero(t, entries[0].RequestID)
	assert.EqualValues(t, 123, entries[0].RequestTimeMs)
	_, err = repo.AcceptRequest(ctx, entries[0].RequestID, 503, 10)
	assert.ErrorIs(t, err, ErrRequestNotFound)
	_, err = repo.AcceptRequest(ctx, entries[0].RequestID, 502, 10)
	require.NoError(t, err)
}
