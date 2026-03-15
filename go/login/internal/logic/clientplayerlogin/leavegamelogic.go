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

type LeaveGameLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewLeaveGameLogic(ctx context.Context, svcCtx *svc.ServiceContext) *LeaveGameLogic {
	return &LeaveGameLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *LeaveGameLogic) LeaveGame(in *login_proto.LeaveGameRequest) (*login_proto.LoginEmptyResponse, error) {
	resp := &login_proto.LoginEmptyResponse{}

	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok {
		logx.Error("Session not found in context during leave game")
		return resp, nil
	}

	// ✅ 统一 session 清理
	err := sessioncleaner.CleanupSession(
		l.ctx,
		l.svcCtx.RedisClient,
		sessionDetails.SessionId,
		"leave",
	)
	if err != nil {
		logx.Errorf("LeaveGame cleanup failed: %v", err)
	}

	// Remove player session from Redis (replaces Centre notification)
	if sessionDetails.PlayerId > 0 {
		if err := sessionmanager.DeleteSession(l.ctx, l.svcCtx.RedisClient, sessionDetails.PlayerId); err != nil {
			logx.Errorf("Failed to delete session for player %d: %v", sessionDetails.PlayerId, err)
		}
	}

	return resp, nil
}
