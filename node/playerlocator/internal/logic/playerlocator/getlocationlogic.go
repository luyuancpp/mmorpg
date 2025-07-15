package playerlocatorlogic

import (
	"context"

	"playerlocator/internal/svc"
	"playerlocator/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type GetLocationLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewGetLocationLogic(ctx context.Context, svcCtx *svc.ServiceContext) *GetLocationLogic {
	return &GetLocationLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *GetLocationLogic) GetLocation(in *game.PlayerId) (*game.PlayerLocation, error) {
	// todo: add your logic here and delete this line

	return &game.PlayerLocation{}, nil
}
