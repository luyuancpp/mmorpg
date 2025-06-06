package clientplayerloginlogic

import (
	"context"
	"login/data"
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
	sessionId, ok := l.ctx.Value("SessionId").(*string)
	if !ok {
		logx.Error("failed to get SessionId from context")
		return &game.Empty{}, nil
	}
	defer data.SessionList.Remove(*sessionId)
	return &game.Empty{}, nil
}
