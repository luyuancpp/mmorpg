package clientplayerloginlogic

import (
	"context"
	"login/data"
	"login/internal/logic/pkg/ctxkeys"
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
	sessionId, ok := ctxkeys.GetSessionID(l.ctx)
	if !ok {
		logx.Error("failed to get SessionId from context")
		return &game.Empty{}, nil
	}

	node := l.svcCtx.GetCentreClient()
	if nil == node {
		return &game.Empty{}, nil
	}

	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok {
		logx.Error("Session not found in context during leave game ")
		return &game.Empty{}, nil
	}

	centreRequest := &game.LoginNodeLeaveGameRequest{
		SessionInfo: sessionDetails,
	}

	node.Send(centreRequest, game.CentreLoginNodeLeaveGameMessageId)

	defer data.SessionList.Remove(sessionId)
	return &game.Empty{}, nil
}
