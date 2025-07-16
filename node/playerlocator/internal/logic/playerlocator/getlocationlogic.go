package playerlocatorlogic

import (
	"context"
	"encoding/json"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"playerlocator/internal/keys"

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
	key := keys.PlayerLocationKey(in.Uid)
	val, err := l.svcCtx.Redis.Get(l.ctx, key).Result()

	if err != nil {
		if err.Error() == "redis: nil" {
			return nil, status.Errorf(codes.NotFound, "player %d not found", in.Uid)
		}
		return nil, err
	}

	var location playerlocatorpb.PlayerLocation
	if err := json.Unmarshal([]byte(val), &location); err != nil {
		return nil, err
	}

	return &location, nil
}
