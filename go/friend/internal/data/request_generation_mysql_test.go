package data

import (
	"context"
	"errors"
	"sync/atomic"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestRequestGeneration_ReplayAndLateActions(t *testing.T) {
	db, ctx := openFriendTestDB(t)
	repo, _ := newFriendTestRepo(t, db)
	lim := AddFriendLimits{MaxFriends: 10, MaxPendingRequests: 10, MaxIncomingRequests: 10}
	first, err := repo.CreateRequest(ctx, 101, 202, lim)
	require.NoError(t, err)
	require.True(t, first.Changed)
	require.NotZero(t, first.RequestID)
	_, err = db.ExecContext(ctx, "UPDATE friend_request SET request_time_ms=111, updated_ms=222 WHERE from_player_id=101 AND to_player_id=202")
	require.NoError(t, err)

	// pending 重放无需号源，更不能借重复点击刷新时间或制造新代次。
	repo.nextRequestID = func(context.Context) (uint64, error) { return 0, errors.New("号源不可用") }
	replay, err := repo.CreateRequest(ctx, 101, 202, lim)
	require.NoError(t, err)
	assert.Equal(t, first.RequestID, replay.RequestID)
	assert.False(t, replay.Changed)
	var requested, updated int64
	require.NoError(t, db.QueryRowContext(ctx, "SELECT request_time_ms, updated_ms FROM friend_request WHERE from_player_id=101 AND to_player_id=202").Scan(&requested, &updated))
	assert.EqualValues(t, 111, requested)
	assert.EqualValues(t, 222, updated)
	_, err = repo.AcceptRequest(ctx, first.RequestID, 303, 10)
	assert.ErrorIs(t, err, ErrRequestNotFound, "其他玩家不能消费此申请")
	_, err = repo.AcceptRequest(ctx, 0, 202, 10)
	assert.ErrorIs(t, err, ErrRequestNotFound)
	_, err = repo.RejectRequest(ctx, 0, 202)
	assert.ErrorIs(t, err, ErrRequestNotFound)

	rejected, err := repo.RejectRequest(ctx, first.RequestID, 202)
	require.NoError(t, err)
	assert.True(t, rejected.Changed)
	rejected, err = repo.RejectRequest(ctx, first.RequestID, 202)
	require.NoError(t, err)
	assert.False(t, rejected.Changed)
	repo.nextRequestID = func(context.Context) (uint64, error) { return testRequestIDs.Add(1), nil }
	second, err := repo.CreateRequest(ctx, 101, 202, lim)
	require.NoError(t, err)
	assert.True(t, second.Changed)
	require.NotEqual(t, first.RequestID, second.RequestID)
	_, err = repo.AcceptRequest(ctx, first.RequestID, 202, 10)
	assert.ErrorIs(t, err, ErrRequestNotFound, "旧代次不能接受重发的新申请")
	_, err = repo.RejectRequest(ctx, first.RequestID, 202)
	assert.ErrorIs(t, err, ErrRequestNotFound, "旧代次不能拒绝重发的新申请")
	accepted, err := repo.AcceptRequest(ctx, second.RequestID, 202, 10)
	require.NoError(t, err)
	assert.True(t, accepted.Changed)
	accepted, err = repo.AcceptRequest(ctx, second.RequestID, 202, 10)
	require.NoError(t, err)
	assert.False(t, accepted.Changed)
	assertFriendInvariants(t, ctx, db)
	assert.EqualValues(t, 2, mustCount(t, ctx, db, "SELECT COUNT(*) FROM friend"))
	removed, err := repo.RemoveFriend(ctx, 101, 202)
	require.NoError(t, err)
	require.True(t, removed)
	accepted, err = repo.AcceptRequest(ctx, second.RequestID, 202, 10)
	require.NoError(t, err)
	assert.False(t, accepted.Changed)
	assert.Zero(t, mustCount(t, ctx, db, "SELECT COUNT(*) FROM friend"), "旧成功回包重放不得恢复已删除的关系")
}

func TestRequestGeneration_LegacyBackfillIsStableUnderConcurrentReadsAndResend(t *testing.T) {
	db, ctx := openFriendTestDB(t)
	repo, mr := newFriendTestRepo(t, db)
	_, err := db.ExecContext(ctx, `INSERT INTO friend_request
		(from_player_id,to_player_id,request_time_ms,status,updated_ms) VALUES(401,402,123,1,456)`)
	require.NoError(t, err)
	// 即使当前键里也被塞入旧0代次列表，权威读仍须回源；旧v3键完全不参与读取。
	mr.Set(pendingRequestsKey(402), `[{"from_player_id":401,"to_player_id":402,"status":1}]`)
	mr.Set("friend:{f:402}:req:v3", `[{"from_player_id":999,"to_player_id":402,"status":1}]`)
	var observed atomic.Uint64
	errList := runConcurrent(t, 16, func(i int) error {
		var id uint64
		if i%2 == 0 {
			entries, err := repo.GetPendingRequests(ctx, 402)
			if err != nil {
				return err
			}
			if len(entries) != 1 || entries[0].RequestTimeMs != 123 || entries[0].RequestID == 0 {
				return errors.New("历史申请未保留时刻或未补代次")
			}
			id = entries[0].RequestID
		} else {
			change, err := repo.CreateRequest(ctx, 401, 402, AddFriendLimits{MaxFriends: 10})
			if err != nil {
				return err
			}
			id = change.RequestID
		}
		observed.CompareAndSwap(0, id)
		if observed.Load() != id {
			return errors.New("并发回填轮换了已发出的非零代次")
		}
		return nil
	})
	for _, err := range errList {
		require.NoError(t, err)
	}
	require.NotZero(t, observed.Load())
	assert.EqualValues(t, 456, readRequestUpdatedMs(t, ctx, db, 401, 402), "补号不改状态时刻")
	_, err = repo.RejectRequest(ctx, observed.Load(), 402)
	require.NoError(t, err)
	entry, pending, err := repo.ensurePendingRequest(ctx, 401, 402)
	require.NoError(t, err)
	assert.False(t, pending)
	assert.Equal(t, observed.Load(), entry.RequestID, "拒绝后列表回填不得轮换非零ID")
	entries, err := repo.GetPendingRequests(ctx, 402)
	require.NoError(t, err)
	assert.Empty(t, entries, "权威读不能返回预置陈旧pending缓存")
}

func TestRequestGeneration_LegacySingleConnectionAndSourceFailure(t *testing.T) {
	db, ctx := openFriendTestDB(t)
	db.SetMaxOpenConns(1)
	repo, _ := newFriendTestRepo(t, db)
	_, err := db.ExecContext(ctx, `INSERT INTO friend_request
		(from_player_id,to_player_id,request_time_ms,status,updated_ms) VALUES(501,502,123,1,456)`)
	require.NoError(t, err)
	repo.nextRequestID = func(context.Context) (uint64, error) { return 0, errors.New("号源断开") }
	_, err = repo.GetPendingRequests(ctx, 502)
	require.ErrorContains(t, err, "号源断开")
	assert.Zero(t, mustCount(t, ctx, db, "SELECT COUNT(*) FROM friend_request WHERE request_id<>0"))
	_, err = repo.CreateRequest(ctx, 503, 504, AddFriendLimits{})
	require.ErrorContains(t, err, "号源断开")
	assert.Zero(t, mustCount(t, ctx, db, "SELECT COUNT(*) FROM friend_request WHERE from_player_id=503"))
	repo.nextRequestID = func(context.Context) (uint64, error) { return testRequestIDs.Add(1), nil }
	entries, err := repo.GetPendingRequests(ctx, 502)
	require.NoError(t, err, "列表必须先关闭rows再用同一连接池开回填事务")
	require.Len(t, entries, 1)
	assert.NotZero(t, entries[0].RequestID)
}

func TestRequestGeneration_DuplicateIDFailsClosedAndListsIgnoreStaleCache(t *testing.T) {
	db, ctx := openFriendTestDB(t)
	repo, mr := newFriendTestRepo(t, db)
	_, err := db.ExecContext(ctx, `INSERT INTO friend_request
		(from_player_id,to_player_id,request_time_ms,status,updated_ms,request_id)
		VALUES(601,602,1,1,1,777),(603,604,1,1,1,777)`)
	require.NoError(t, err)
	_, err = repo.AcceptRequest(ctx, 777, 602, 10)
	require.ErrorContains(t, err, "多条记录")
	_, err = repo.RejectRequest(ctx, 777, 604)
	require.ErrorContains(t, err, "多条记录")
	assert.Zero(t, mustCount(t, ctx, db, "SELECT COUNT(*) FROM friend"))
	assert.EqualValues(t, 2, mustCount(t, ctx, db, "SELECT COUNT(*) FROM friend_request WHERE status=1"))
	mr.Set(friendListKey(602), `[{"friend_player_id":999,"since_ms":1}]`)
	friends, err := repo.GetFriendList(ctx, 602)
	require.NoError(t, err)
	assert.Empty(t, friends, "好友权限列表必须来自当前权威库")
	require.NoError(t, CheckRequestIDLookupIndex(ctx, db))
}

func TestRequestGeneration_BackfillReturnsSingleAuthoritySnapshot(t *testing.T) {
	db, ctx := openFriendTestDB(t)
	repo, _ := newFriendTestRepo(t, db)
	first, err := repo.CreateRequest(ctx, 701, 703, AddFriendLimits{})
	require.NoError(t, err)
	_, err = db.ExecContext(ctx, `INSERT INTO friend_request
		(from_player_id,to_player_id,request_time_ms,status,updated_ms) VALUES(702,703,1,1,1)`)
	require.NoError(t, err)
	var minted atomic.Bool
	repo.nextRequestID = func(context.Context) (uint64, error) {
		// 确定性安排：列表初读后、旧0行补号前，先提交另一条申请的接受。
		// 号源在守卫事务外调用，所以接受可以取得同一个接收者容量守卫。
		if minted.CompareAndSwap(false, true) {
			_, err := repo.AcceptRequest(ctx, first.RequestID, 703, 10)
			if err != nil {
				return 0, err
			}
		}
		return testRequestIDs.Add(1), nil
	}
	entries, err := repo.GetPendingRequests(ctx, 703)
	require.NoError(t, err)
	require.True(t, minted.Load())
	require.Len(t, entries, 1, "不能把已接受的旧快照A与后来补号的B拼成从未存在的列表")
	assert.EqualValues(t, 702, entries[0].FromPlayerID)
	assert.NotZero(t, entries[0].RequestID)
}
