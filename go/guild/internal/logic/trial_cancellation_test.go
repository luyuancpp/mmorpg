package logic

import (
	"context"
	"strings"
	"testing"
	"time"

	matchpb "proto/match"
	"shared/serviceauth"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
)

type cancellationRPCFixture struct {
	mode, secret string
	t            *testing.T
}

func (f cancellationRPCFixture) CancelActivityBattle(ctx context.Context, in *matchpb.CancelActivityBattleRequest, _ ...grpc.CallOption) (*matchpb.CancelActivityBattleResponse, error) {
	verifier, err := serviceauth.New(map[string]string{"guild": f.secret})
	require.NoError(f.t, err)
	method := matchpb.MatchInternal_CancelActivityBattle_FullMethodName
	caller, err := verifier.Verify(ctx, method, in)
	require.NoError(f.t, err)
	require.Equal(f.t, "guild", caller)
	digest, err := serviceauth.Digest(in)
	require.NoError(f.t, err)
	out := &matchpb.CancelActivityBattleResponse{RequestCancelled: true, RequestDigest: digest}
	if f.mode == "wrong-original" {
		out.RequestDigest = strings.Repeat("0", 64)
	}
	if f.mode != "unsigned" {
		replyMethod := method + "/reply"
		if f.mode == "wrong-method" {
			replyMethod = method
		}
		require.NoError(f.t, serviceauth.Sign(replyMethod, out, "match", f.secret, uint64(time.Now().UnixMilli())))
	}
	if f.mode == "tampered" {
		out.BattleId = 9
	}
	return out, nil
}
func TestTrialCancellationVerifiesReplySignatureAndOriginalRequestDigest(t *testing.T) {
	for _, mode := range []string{"valid", "unsigned", "wrong-original", "wrong-method", "tampered"} {
		t.Run(mode, func(t *testing.T) {
			secret := strings.Repeat("k", 32)
			client, err := NewTrialCancellation(cancellationRPCFixture{mode: mode, secret: secret, t: t}, secret)
			require.NoError(t, err)
			out, err := client.CancelOriginal(context.Background(), &matchpb.StartActivityBattleRequest{RequestId: 51, BattleConfigId: 1, MemberPlayerIds: []uint64{11, 22}})
			if mode == "valid" {
				require.NoError(t, err)
				require.True(t, out.RequestCancelled)
				require.Zero(t, out.BattleId)
			} else {
				require.ErrorIs(t, err, serviceauth.ErrDenied)
			}
		})
	}
}
