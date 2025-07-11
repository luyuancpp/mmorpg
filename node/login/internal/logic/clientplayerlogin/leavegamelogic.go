package clientplayerloginlogic

import (
	"context"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/utils/sessioncleaner"
	"login/internal/svc"
	"login/pb/game"

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

func (l *LeaveGameLogic) LeaveGame(in *game.LeaveGameRequest) (*game.Empty, error) {
	resp := &game.Empty{}

	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok {
		logx.Error("Session not found in context during leave game")
		return resp, nil
	}

	// ✅ 统一 session 清理
	err := sessioncleaner.CleanupSessionAndNotify(
		l.ctx,
		l.svcCtx.Redis,
		sessionDetails.SessionId,
		"leave",
	)
	if err != nil {
		logx.Errorf("LeaveGame cleanup failed: %v", err)
	}

	// 6. 通知中心服务
	node := l.svcCtx.GetCentreClient()
	if node != nil {
		centreRequest := &game.LoginNodeLeaveGameRequest{
			SessionInfo: sessionDetails,
		}
		node.Send(centreRequest, game.CentreLoginNodeLeaveGameMessageId)
	} else {
		logx.Error("Centre client is nil during leave")
	}

	return resp, nil
}
