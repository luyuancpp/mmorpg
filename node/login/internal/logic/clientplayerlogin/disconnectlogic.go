package clientplayerloginlogic

import (
	"context"
	"login/data"
	"login/internal/logic/pkg/ctxkeys"
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
	sessionId, ok := ctxkeys.GetSessionID(l.ctx)
	if !ok {
		logx.Error("failed to get SessionId from context")
		return &game.Empty{}, nil
	}

	data.SessionList.Remove(sessionId)

	centreRequest := &game.GateSessionDisconnectRequest{
		SessionInfo: &game.SessionDetails{SessionId: in.SessionId},
	}
	l.svcCtx.GetCentreClient().Send(centreRequest, game.CentreLoginNodeSessionDisconnectMessageId)
	return &game.Empty{}, nil
}
