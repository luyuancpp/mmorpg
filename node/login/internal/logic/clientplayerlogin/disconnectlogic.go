package clientplayerloginlogic

import (
	"context"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/utils/sessioncleaner"
	"login/internal/svc"
	"login/pb/game"

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

func (l *DisconnectLogic) Disconnect(in *game.LoginNodeDisconnectRequest) (*game.Empty, error) {
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok {
		logx.Error("Session not found in context during leave game")
		return &game.Empty{}, nil
	}

	_ = sessioncleaner.CleanupSession(
		l.ctx,
		l.svcCtx.RedisClient,
		sessionDetails.SessionId,
		"disconnect",
	)

	centreRequest := &game.GateSessionDisconnectRequest{
		SessionInfo: &game.SessionDetails{SessionId: in.SessionId},
	}
	node := l.svcCtx.GetCentreClient()
	if nil == node {
		return &game.Empty{}, nil
	}
	node.Send(centreRequest, game.CentreLoginNodeSessionDisconnectMessageId)
	return &game.Empty{}, nil
}
