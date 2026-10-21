package team

import (
	"context"
	"testing"
	"time"

	"match/internal/svc"

	base "proto/common/base"
	teampb "proto/team"
	"shared/serviceauth"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
)

func TestTeamTiDBSceneMembershipRequiresRegisteredAdmissionAndSignedReply(t *testing.T) {
	f := newNativeTeamFixture(t)
	f.create(t, 1, 101)
	_, err := f.owner.DB.Exec("CREATE TABLE account_role_admission(player_id BIGINT UNSIGNED NOT NULL,scene_instance_id VARBINARY(36) NOT NULL,highest_generation BIGINT UNSIGNED NOT NULL,closed_through BIGINT UNSIGNED NOT NULL,identity_proto MEDIUMBLOB NOT NULL,PRIMARY KEY(player_id,scene_instance_id))")
	require.NoError(t, err)
	admission := &base.RoleAdmissionIdentity{PlayerId: 1, SceneInstanceId: uuid.NewString(), Generation: 5, OwnerRevision: 7, OwnerEpoch: 9, SceneNodeId: "scene-original", SceneId: 1001, ZoneId: 1, SessionId: 2, LoginGeneration: 3}
	payload, err := proto.Marshal(admission)
	require.NoError(t, err)
	_, err = f.owner.DB.Exec("INSERT INTO account_role_admission VALUES(?,?,5,0,?)", 1, []byte(admission.SceneInstanceId), payload)
	require.NoError(t, err)
	secret := "fixture-only-scene-membership-secret-00000000"
	verifier, err := serviceauth.New(map[string]string{"scene": secret})
	require.NoError(t, err)
	server := &AuthorityServer{svc: &svc.ServiceContext{Authority: f.owner}, verifier: verifier, secret: secret}
	request := &teampb.ReadTeamMembershipRequest{Admission: admission}
	require.NoError(t, serviceauth.Sign(teamMembershipMethod, request, "scene", secret, uint64(time.Now().UnixMilli())))
	response, err := server.ReadMembership(context.Background(), request)
	require.NoError(t, err)
	require.Equal(t, uint64(101), response.TeamId)
	require.Equal(t, uint64(1), response.Version)
	digest, err := serviceauth.Digest(request)
	require.NoError(t, err)
	require.Equal(t, digest, response.RequestDigest)
	replyVerifier, err := serviceauth.New(map[string]string{"team": secret})
	require.NoError(t, err)
	caller, err := replyVerifier.Verify(context.Background(), teamMembershipMethod+"/reply", response)
	require.NoError(t, err)
	require.Equal(t, "team", caller)
	clientCtx := metadata.NewIncomingContext(context.Background(), metadata.Pairs("x-session-detail-bin", "invalid-old-session"))
	_, err = server.ReadMembership(clientCtx, request)
	require.Equal(t, codes.PermissionDenied, status.Code(err))
	// 即使是真scene重签，旧E/旧generation也不能通过已登记原件的完整比较。
	old := proto.Clone(request).(*teampb.ReadTeamMembershipRequest)
	old.Admission.OwnerEpoch = 8
	require.NoError(t, serviceauth.Sign(teamMembershipMethod, old, "scene", secret, uint64(time.Now().UnixMilli())))
	_, err = server.ReadMembership(context.Background(), old)
	require.Equal(t, codes.FailedPrecondition, status.Code(err))
	_, err = f.owner.DB.Exec("UPDATE account_role_admission SET closed_through=5 WHERE player_id=1")
	require.NoError(t, err)
	_, err = server.ReadMembership(context.Background(), request)
	require.Equal(t, codes.FailedPrecondition, status.Code(err))
}
