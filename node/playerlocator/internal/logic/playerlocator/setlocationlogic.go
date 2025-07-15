package playerlocatorlogic

import (
	"context"

	"playerlocator/internal/svc"
	"playerlocator/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type SetLocationLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewSetLocationLogic(ctx context.Context, svcCtx *svc.ServiceContext) *SetLocationLogic {
	return &SetLocationLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *SetLocationLogic) SetLocation(in *game.PlayerLocation) (*game.Empty, error) {
	// todo: add your logic here and delete this line

	return &game.Empty{}, nil
}
