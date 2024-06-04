package loginservicelogic

import (
	"context"

	"login_server/internal/svc"
	"login_server/pb/game"

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
	// todo: add your logic here and delete this line

	return &game.Empty{}, nil
}
