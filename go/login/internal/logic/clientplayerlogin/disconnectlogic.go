package clientplayerloginlogic

import (
	"context"
	"login/internal/svc"
	login_proto "login/proto/login"

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
	sessionDetails, ok := getSessionDetailsForAction(l.ctx, "disconnect")
	if !ok {
		return &login_proto.LoginEmptyResponse{}, nil
	}

	cleanupLoginSessionState(l.ctx, l.svcCtx, sessionDetails.SessionId, "disconnect")

	// Mark session as disconnecting with 30s lease in Redis (replaces Centre's DelayedCleanupTimer).
	// When the Redis key TTL expires, the session is effectively cleaned up.
	markPlayerSessionDisconnecting(l.ctx, l.svcCtx, sessionDetails.PlayerId, in.SessionId)

	return &login_proto.LoginEmptyResponse{}, nil
}
