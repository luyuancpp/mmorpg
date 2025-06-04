package loginservicelogic

import (
	"context"
	"login/data"
	"strconv"

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

func (l *LeaveGameLogic) LeaveGame(in *game.LeaveGameC2LRequest) (*game.Empty, error) {
	sessionId := strconv.FormatUint(in.SessionId, 10)
	data.SessionList.Remove(sessionId)
	return &game.Empty{}, nil
}
