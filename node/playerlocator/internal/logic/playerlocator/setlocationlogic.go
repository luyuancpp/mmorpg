package playerlocatorlogic

import (
	"context"
	"encoding/json"
	"playerlocator/internal/keys"

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
	key := keys.PlayerLocationKey(in.Uid)
	data, _ := json.Marshal(in)

	err := l.svcCtx.Redis.Set(l.ctx, key, data, 0).Err()
	if err != nil {
		return nil, err
	}
	return &emptypb.Empty{}, nil
}
