package playerlocatorlogic

import (
	"context"
	"encoding/json"
	"google.golang.org/protobuf/types/known/emptypb"
	"playerlocator/internal/keys"

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
	key := keys.PlayerLocationKey(in.Uid)
	val, err := l.svcCtx.Redis.Get(l.ctx, key).Result()
	if err != nil {
		return &emptypb.Empty{}, nil
	}

	var location playerlocatorpb.PlayerLocation
	if err := json.Unmarshal([]byte(val), &location); err != nil {
		return nil, err
	}

	location.Online = false
	location.Migrating = false

	data, _ := json.Marshal(location)
	err = l.svcCtx.Redis.Set(l.ctx, key, data, 0).Err()
	if err != nil {
		return nil, err
	}

	return &emptypb.Empty{}, nil
}
