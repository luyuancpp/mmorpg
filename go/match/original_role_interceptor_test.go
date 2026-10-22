package main

import (
	"context"
	"encoding/base64"
	"testing"
	"time"

	"github.com/alicebob/miniredis/v2"
	redv9 "github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"google.golang.org/protobuf/proto"
	"match/internal/authority"
	"match/internal/pkg/ctxkeys"
	"match/internal/svc"
	base "proto/common/base"
	dspb "proto/data_service"
	matchpb "proto/match"
	"shared/loginsession"
	"shared/roleidentity"
)

func originalRoleSession() *base.SessionDetails {
	return &base.SessionDetails{SessionId: 7, PlayerId: 11, GateInstanceId: "original-gate-uuid", SessionIncarnation: 19, OwnerRevision: 9}
}

func originalRoleIncoming(t *testing.T, detail *base.SessionDetails) context.Context {
	t.Helper()
	raw, err := proto.Marshal(detail)
	require.NoError(t, err)
	return metadata.NewIncomingContext(context.Background(), metadata.Pairs(sessionMetadataKey, base64.StdEncoding.EncodeToString(raw)))
}

func originalRoleFixture(t *testing.T) (*svc.ServiceContext, *redv9.Client) {
	t.Helper()
	mr := miniredis.RunT(t)
	client := redv9.NewClient(&redv9.Options{Addr: mr.Addr(), DB: 2, MaxRetries: -1})
	t.Cleanup(func() { require.NoError(t, client.Close()) })
	// DB故意为nil：原件捕获不准查询当前SQL；若入口偷偷补权，此例会直接失败。
	return &svc.ServiceContext{Authority: &authority.Store{}, LoginSessions: client}, client
}

func originalRoleChain(sc *svc.ServiceContext, ctx context.Context, handler grpc.UnaryHandler) (any, error) {
	info := &grpc.UnaryServerInfo{FullMethod: "/match.MatchService/JoinQueue"}
	return sessionInterceptor(ctx, &matchpb.JoinQueueRequest{}, info, func(decoded context.Context, request any) (any, error) {
		return originalRoleInterceptor(sc)(decoded, request, info, handler)
	})
}

func TestOriginalRoleInterceptorCapturesLoginAccountAndWireRevisionAfterSessionDecode(t *testing.T) {
	sc, client := originalRoleFixture(t)
	detail := originalRoleSession()
	key, err := loginsession.ConnectionKey(detail)
	require.NoError(t, err)
	require.NoError(t, client.Set(context.Background(), key, "原账号", time.Hour).Err())
	// 同一Redis服务的另一个DB与旧session号键均不能为此连接授予身份。
	other := redv9.NewClient(&redv9.Options{Addr: client.Options().Addr, DB: 0, MaxRetries: -1})
	defer other.Close()
	require.NoError(t, other.Set(context.Background(), key, "错误实例账号", time.Hour).Err())
	require.NoError(t, client.Set(context.Background(), "login_session:7", "旧session号账号", time.Hour).Err())
	calls := 0
	result, err := originalRoleChain(sc, originalRoleIncoming(t, detail), func(ctx context.Context, _ any) (any, error) {
		calls++
		decoded, ok := ctxkeys.GetSessionDetails(ctx)
		require.True(t, ok)
		require.True(t, proto.Equal(detail, decoded), "原会话字段与未知字段必须完整保留")
		captured, ok := roleidentity.Original(ctx)
		require.True(t, ok)
		require.Equal(t, "原账号", captured.Account)
		require.EqualValues(t, 9, captured.OwnerRevision)
		require.EqualValues(t, 11, captured.Role.PlayerId)
		copy, ok := roleidentity.Original(ctx)
		require.True(t, ok)
		copy.Account = "不得改写上下文"
		again, ok := roleidentity.Original(ctx)
		require.True(t, ok)
		require.Equal(t, "原账号", again.Account)
		return captured, nil
	})
	require.NoError(t, err)
	require.Equal(t, 1, calls)
	require.Equal(t, "原账号", result.(*dspb.AccountRoleOwnership).Account)
}

func TestOriginalRoleInterceptorRejectsMissingRevLegacyConnectionAndRevokedLogin(t *testing.T) {
	for _, mode := range []string{"zero-revision", "no-gate-uuid", "no-incarnation", "revoked", "missing-login-pool", "bad-session-bytes", "bad-session-protobuf"} {
		t.Run(mode, func(t *testing.T) {
			sc, client := originalRoleFixture(t)
			detail := originalRoleSession()
			key, err := loginsession.ConnectionKey(detail)
			require.NoError(t, err)
			require.NoError(t, client.Set(context.Background(), key, "原账号", time.Hour).Err())
			switch mode {
			case "zero-revision":
				detail.OwnerRevision = 0
			case "no-gate-uuid":
				detail.GateInstanceId = ""
			case "no-incarnation":
				detail.SessionIncarnation = 0
			case "revoked":
				require.NoError(t, client.Del(context.Background(), key).Err())
			case "missing-login-pool":
				sc.LoginSessions = nil
			}
			ctx := originalRoleIncoming(t, detail)
			if mode == "bad-session-bytes" {
				ctx = metadata.NewIncomingContext(context.Background(), metadata.Pairs(sessionMetadataKey, "%%%"))
			}
			if mode == "bad-session-protobuf" {
				ctx = metadata.NewIncomingContext(context.Background(), metadata.Pairs(sessionMetadataKey, base64.StdEncoding.EncodeToString([]byte{0xff})))
			}
			calls := 0
			_, err = originalRoleChain(sc, ctx, func(context.Context, any) (any, error) { calls++; return nil, nil })
			require.Equal(t, codes.PermissionDenied, status.Code(err))
			require.Zero(t, calls)
		})
	}
}

func TestOriginalRoleInterceptorDoesNotBorrowLegacySessionIDOrAnotherIncarnation(t *testing.T) {
	sc, client := originalRoleFixture(t)
	detail := originalRoleSession()
	require.NoError(t, client.Set(context.Background(), "login_session:7", "错误旧账号", time.Hour).Err())
	previous := proto.Clone(detail).(*base.SessionDetails)
	previous.SessionIncarnation--
	key, err := loginsession.ConnectionKey(previous)
	require.NoError(t, err)
	require.NoError(t, client.Set(context.Background(), key, "旧连接账号", time.Hour).Err())
	calls := 0
	_, err = originalRoleChain(sc, originalRoleIncoming(t, detail), func(context.Context, any) (any, error) { calls++; return nil, nil })
	require.Equal(t, codes.PermissionDenied, status.Code(err))
	require.Zero(t, calls)
}

func TestOriginalRoleInterceptorInternalCallCannotManufactureClientActor(t *testing.T) {
	sc, _ := originalRoleFixture(t)
	info := &grpc.UnaryServerInfo{FullMethod: matchpb.MatchInternal_CancelActivityBattle_FullMethodName}
	calls := 0
	_, err := sessionInterceptor(context.Background(), &matchpb.CancelActivityBattleRequest{}, info, func(ctx context.Context, request any) (any, error) {
		return originalRoleInterceptor(sc)(ctx, request, info, func(bound context.Context, _ any) (any, error) {
			calls++
			_, ok := roleidentity.Original(bound)
			require.False(t, ok)
			return nil, nil
		})
	})
	require.NoError(t, err)
	require.Equal(t, 1, calls)
}
