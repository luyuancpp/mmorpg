package playerlocatorlogic

import (
	"context"

	"playerlocator/internal/svc"
	"playerlocator/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type MarkOfflineLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewMarkOfflineLogic(ctx context.Context, svcCtx *svc.ServiceContext) *MarkOfflineLogic {
	return &MarkOfflineLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *MarkOfflineLogic) MarkOffline(in *game.PlayerId) (*game.Empty, error) {
	// todo: add your logic here and delete this line

	return &game.Empty{}, nil
}
