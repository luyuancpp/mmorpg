package logic

import (
	"context"
	"errors"
	"testing"

	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"match/internal/svc"
	dspb "proto/data_service"
	pb "proto/match"
)

func TestGlobalChallengeNoticeDoesNotExposePrivateOriginalAccount(t *testing.T) {
	invitation := &pb.MatchChallengeInvitationRecord{ChallengeId: 9, ChallengerPlayerId: 1, TargetPlayerId: 2, ConfigId: 7, ChallengerAccount: "private-original-login", ChallengerRevision: 3, TargetAccount: "private-target-login", TargetRevision: 4, ExpiresMs: 123456}
	shown := globalChallengeInvitationNotice(invitation, "公开展示名")
	require.Equal(t, "公开展示名", shown.ChallengerName)
	require.Equal(t, invitation.ChallengeId, shown.ChallengeId)
	require.Equal(t, invitation.ConfigId, shown.BattleConfigId)
	require.Empty(t, globalChallengeInvitationNotice(invitation, "").ChallengerName, "公开来源缺失也不能回退到私有原账号")
}

type challengeNameClient struct {
	dspb.DataServiceClient
	response    *dspb.BatchGetPlayerNameResponse
	err         error
	requested   []uint64
	hasDeadline bool
}

func (c *challengeNameClient) BatchGetPlayerName(ctx context.Context, in *dspb.BatchGetPlayerNameRequest, _ ...grpc.CallOption) (*dspb.BatchGetPlayerNameResponse, error) {
	c.requested = append([]uint64(nil), in.PlayerIds...)
	_, c.hasDeadline = ctx.Deadline()
	return c.response, c.err
}

func TestChallengePublicNameUsesOnlyBoundedRoleNameLookup(t *testing.T) {
	client := &challengeNameClient{response: &dspb.BatchGetPlayerNameResponse{Names: map[uint64]string{1: "公开角色名", 2: "其他角色"}}}
	s := &svc.ServiceContext{DataServiceClient: client}
	require.Equal(t, "公开角色名", challengePublicName(context.Background(), s, 1))
	require.Equal(t, []uint64{1}, client.requested)
	require.True(t, client.hasDeadline)
	client.err = errors.New("fixture lookup unavailable")
	require.Empty(t, challengePublicName(context.Background(), s, 1))
	require.Empty(t, challengePublicName(context.Background(), &svc.ServiceContext{}, 1))
}
