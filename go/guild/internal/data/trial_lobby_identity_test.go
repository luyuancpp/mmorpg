package data

import (
	"context"
	"testing"

	base "proto/common/base"
	dspb "proto/data_service"
	"shared/roleidentity"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
)

func TestTrialConsentKeepsOriginalAccountRevisionAndRejectsChangedOwner(t *testing.T) {
	mr := miniredis.RunT(t)
	client := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { require.NoError(t, client.Close()) })
	r, err := NewTrialLobbyRepo(client)
	require.NoError(t, err)
	lobby := &TrialLobby{LobbyID: 51, Roster: []uint64{11, 22}, ExpireAtMs: 2000}
	for _, player := range lobby.Roster {
		ctx, err := roleidentity.Capture(context.Background(), &dspb.AccountRoleOwnership{Account: "原账号", OwnerRevision: 7, Role: &base.AccountSimplePlayer{PlayerId: player}})
		require.NoError(t, err)
		require.NoError(t, r.rememberConsent(ctx, lobby, player, 1000))
	}
	roster, err := r.LoadConsents(context.Background(), lobby)
	require.NoError(t, err)
	require.Equal(t, "原账号", roster[0].Account)
	require.EqualValues(t, 7, roster[0].OwnerRevision)
	changed, err := roleidentity.Capture(context.Background(), &dspb.AccountRoleOwnership{Account: "买家账号", OwnerRevision: 8, Role: &base.AccountSimplePlayer{PlayerId: 11}})
	require.NoError(t, err)
	require.ErrorIs(t, r.rememberConsent(changed, lobby, 11, 1000), ErrWriteConflict)
	_, err = client.Del(context.Background(), trialConsentKey(51, 22)).Result()
	require.NoError(t, err)
	_, err = r.LoadConsents(context.Background(), lobby)
	require.Error(t, err, "缺原件必须重新组队，不补当前owner")
}
