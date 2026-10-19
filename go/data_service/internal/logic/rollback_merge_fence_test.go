package logic

import (
	"context"
	"testing"

	"data_service/internal/constants"
	"data_service/internal/guildcheck"
	"data_service/internal/routing"
	"data_service/internal/store"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// 合服围栏内的回档:写门禁在拿锁前拒绝(22、零写入),不算「走到写那一步」—— 写后帮会复查不触发,
// 回 22 而不是 29(复查的第二个结果故意带分歧,一旦被触发就会报 29)。写法同 TestRollbackGuildGate_PostWriteFlagAfterFailedWriteCountsAsFailed。
func TestRollbackPlayer_MergeFenceRefusesWithoutWriteAttempt(t *testing.T) {
	const pid = uint64(829)
	ctx := context.Background()
	svcCtx, mr := newTestSvcCtx(t)
	setupPlayer(t, svcCtx, pid, 9)
	setResp, err := SetPlayerField(ctx, svcCtx, pid, "gold", []byte("current"), 0)
	require.NoError(t, err)
	require.Equal(t, constants.ErrCodeOK, setResp.ErrorCode)
	snapshots := &fakeSnapshotStore{snapshotByID: map[uint64]*store.SnapshotRow{
		1: {ID: 1, PlayerID: pid, CreatedAt: gateSnapshotCreatedAt, Data: mustSnapshotData(t, map[string][]byte{"gold": []byte("old")})},
	}}
	svcCtx.SnapshotStore = snapshots
	svcCtx.RollbackFence = &fakeRollbackFence{}
	enableGuildGate(svcCtx, staticGuildChecker(guildcheck.GuildCheckResult{}, guildcheck.GuildCheckResult{Divergences: divergenceRows(pid, 8)}))
	require.NoError(t, mr.Set(routing.MergeFenceKey(9), "{}"))

	resp, err := RollbackPlayer(ctx, svcCtx, &RollbackPlayerReq{PlayerID: pid, SnapshotID: 1, Operator: "ops", Reason: "r"})
	require.NoError(t, err)
	assert.Equal(t, constants.ErrCodeZoneMergeInProgress, resp.ErrorCode, "不是 29:写后复查没有触发")
	assert.Equal(t, "current", goldOf(t, svcCtx, pid), "被围栏拒绝,数据没有被覆盖")
	require.NotEmpty(t, snapshots.audits)
	last := snapshots.audits[len(snapshots.audits)-1]
	assert.Zero(t, last.PlayersAffected)
	assert.Equal(t, uint32(1), last.PlayersFailed)
}
