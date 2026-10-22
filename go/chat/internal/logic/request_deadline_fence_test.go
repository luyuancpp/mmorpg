package logic

import (
	"chat/internal/authority"
	"chat/internal/constants"
	"context"
	"github.com/stretchr/testify/require"
	chatpb "proto/chat"
	"testing"
)

func TestSendChatAfterRequestDeadlineWritesNothing(t *testing.T) {
	sc, spy := fixture()
	ctx, cancel := context.WithCancel(playerCtx(playerA))
	cancel()
	for _, id := range []string{"deadline-req", ""} {
		in := request()
		in.RequestId = id
		require.Equal(t, constants.ErrStorage, send(t, sc, ctx, in))
	}
	require.Zero(t, spy.sends)
}

// 截止结果未知不能改用Redis或换UUID，原件由SQL唯一键恢复；真实事务回归在authority包。
func TestSendChatDeadlineDuringSQLKeepsOriginalIDForRetry(t *testing.T) {
	sc, spy := fixture()
	ctx, cancel := context.WithCancel(playerCtx(playerA))
	defer cancel()
	spy.send = func(got context.Context, in *chatpb.SendChatRequest) (authority.SendResult, error) {
		require.Equal(t, "original-uuid", in.RequestId)
		cancel()
		return authority.SendResult{}, got.Err()
	}
	require.Equal(t, constants.ErrStorage, send(t, sc, ctx, request()))
	require.Equal(t, 1, spy.sends)
	spy.send = func(context.Context, *chatpb.SendChatRequest) (authority.SendResult, error) {
		return authority.SendResult{Duplicate: true}, nil
	}
	require.Zero(t, send(t, sc, playerCtx(playerA), request()))
	require.Equal(t, 2, spy.sends)
}
