package playerlocatorlogic

import (
	"context"
	"google.golang.org/protobuf/encoding/protojson"
	"player_locator/internal/keys"

	"player_locator/internal/svc"
	"player_locator/pb/game"

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
	val, err := l.svcCtx.RedisCluster.Get(l.ctx, key).Result()
	if err != nil {
		// Key 不存在时，按业务可能仍返回 Empty
		return &game.Empty{}, nil
	}

	var location game.PlayerLocation
	if err := protojson.Unmarshal([]byte(val), &location); err != nil {
		return nil, err
	}

	location.Online = false
	location.Migrating = false

	data, err := protojson.Marshal(&location)
	if err != nil {
		return nil, err
	}

	err = l.svcCtx.RedisCluster.Set(l.ctx, key, data, 0).Err()
	if err != nil {
		return nil, err
	}

	return &game.Empty{}, nil
}
