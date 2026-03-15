package clientplayerloginlogic

import (
	"context"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/sessionmanager"
	"login/internal/logic/utils/sessioncleaner"
	"login/internal/svc"
	login_proto "login/proto/service/go/grpc/login"

	"github.com/zeromicro/go-zero/core/logx"
)

type DisconnectLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewDisconnectLogic(ctx context.Context, svcCtx *svc.ServiceContext) *DisconnectLogic {
	return &DisconnectLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *DisconnectLogic) Disconnect(in *login_proto.LoginNodeDisconnectRequest) (*login_proto.LoginEmptyResponse, error) {
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok {
		logx.Error("Session not found in context during disconnect")
		return &login_proto.LoginEmptyResponse{}, nil
	}

	_ = sessioncleaner.CleanupSession(
		l.ctx,
		l.svcCtx.RedisClient,
		sessionDetails.SessionId,
		"disconnect",
	)

	// Mark session as disconnecting with 30s lease in Redis (replaces Centre's DelayedCleanupTimer).
	// When the Redis key TTL expires, the session is effectively cleaned up.
	if sessionDetails.PlayerId > 0 {
		if err := sessionmanager.SetSessionDisconnecting(l.ctx, l.svcCtx.RedisClient,
			sessionDetails.PlayerId, in.SessionId); err != nil {
			logx.Errorf("Failed to set session disconnecting for player %d: %v", sessionDetails.PlayerId, err)
		} else {
			logx.Infof("Session disconnecting with 30s lease: player=%d session=%d", sessionDetails.PlayerId, in.SessionId)
		}
	}

	return &login_proto.LoginEmptyResponse{}, nil
}
