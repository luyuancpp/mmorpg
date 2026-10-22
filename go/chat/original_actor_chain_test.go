package main

import (
	"context"
	"encoding/base64"
	"testing"

	"chat/internal/session"
	chatpb "proto/chat"
	base "proto/common/base"
	"shared/killswitch"
	"shared/loginsession"
	"shared/roleidentity"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
	"google.golang.org/protobuf/proto"
)

func TestOriginalChatActorIsCapturedAfterWireSessionDecode(t *testing.T) {
	mr := miniredis.RunT(t)
	reader := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	t.Cleanup(func() { require.NoError(t, reader.Close()) })
	detail := &base.SessionDetails{PlayerId: 11, SessionId: 1, GateInstanceId: "chat-original-gate", SessionIncarnation: 19, OwnerRevision: 7}
	key, err := loginsession.ConnectionKey(detail)
	require.NoError(t, err)
	require.NoError(t, mr.Set(key, "original-account"))
	raw, err := proto.Marshal(detail)
	require.NoError(t, err)
	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs(session.MetadataKey, base64.StdEncoding.EncodeToString(raw)))
	chain := buildUnaryInterceptors(killswitch.New(killswitch.Config{}), session.OriginalIdentityInterceptor(reader))
	called := false
	handler := chainUnary(chain, &grpc.UnaryServerInfo{FullMethod: "/chatpb.ClientPlayerChat/SendChat"}, func(ctx context.Context, _ any) (any, error) {
		called = true
		decoded, ok := session.GetSessionDetails(ctx)
		require.True(t, ok)
		require.True(t, proto.Equal(detail, decoded))
		original, ok := roleidentity.Original(ctx)
		require.True(t, ok)
		require.Equal(t, "original-account", original.Account)
		require.EqualValues(t, 7, original.OwnerRevision)
		require.EqualValues(t, 11, original.Role.PlayerId)
		return &chatpb.SendChatResponse{}, nil
	})
	_, err = handler(ctx, nil)
	require.NoError(t, err)
	require.True(t, called)
}
