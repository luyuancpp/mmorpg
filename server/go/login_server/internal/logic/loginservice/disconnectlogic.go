package loginservicelogic

import (
	"context"
	"login_server/data"
	"login_server/internal/svc"
	"login_server/pb/game"
	"strconv"

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
	return &game.Empty{}, nil
}
