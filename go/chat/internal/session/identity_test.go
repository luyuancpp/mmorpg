package session

import (
	"context"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	base "proto/common/base"
	"shared/loginsession"
	"shared/roleidentity"
)

func TestOriginalChatActorCapturesOnlyOriginalGateAccountAndWireRevision(t *testing.T) {
	mr := miniredis.RunT(t)
	r := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { require.NoError(t, r.Close()) })
	detail := &base.SessionDetails{PlayerId: 11, SessionId: 1, GateInstanceId: "original-gate-uuid", SessionIncarnation: 19, OwnerRevision: 7}
	key, err := loginsession.ConnectionKey(detail)
	require.NoError(t, err)
	require.NoError(t, mr.Set(key, "original-account"))
	_, err = OriginalIdentityInterceptor(r)(WithSessionDetails(context.Background(), detail), nil, &grpc.UnaryServerInfo{}, func(ctx context.Context, _ any) (any, error) {
		original, ok := roleidentity.Original(ctx)
		require.True(t, ok)
		require.Equal(t, "original-account", original.Account)
		require.EqualValues(t, 7, original.OwnerRevision)
		require.EqualValues(t, 11, original.Role.PlayerId)
		return nil, nil
	})
	require.NoError(t, err)
}

func TestMissingOriginalChatRevisionOrLoginNeverCreatesActor(t *testing.T) {
	for _, detail := range []*base.SessionDetails{{PlayerId: 11}, {PlayerId: 11, OwnerRevision: 7, SessionId: 1, GateInstanceId: "gone", SessionIncarnation: 19}} {
		_, err := OriginalIdentityInterceptor(nil)(WithSessionDetails(context.Background(), detail), nil, &grpc.UnaryServerInfo{}, func(ctx context.Context, _ any) (any, error) {
			_, ok := roleidentity.Original(ctx)
			require.False(t, ok, "不能用当前目录补原账号或修订")
			return nil, nil
		})
		require.NoError(t, err)
	}
}
