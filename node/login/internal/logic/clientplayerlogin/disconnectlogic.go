package clientplayerloginlogic

import (
	"context"
	"login/data"
	"strconv"

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
	sessionId := strconv.FormatUint(in.SessionId, 10)
	data.SessionList.Remove(sessionId)

	centreRequest := &game.GateSessionDisconnectRequest{
		SessionInfo: &game.SessionDetails{SessionId: in.SessionId},
	}
	l.svcCtx.GetCentreClient().Send(centreRequest, game.CentreServiceLoginNodeSessionDisconnectMessageId)
	return &game.Empty{}, nil
}
