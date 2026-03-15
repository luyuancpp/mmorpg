package logic

import (
	"context"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/svc"
	pb "player_locator/proto/player_locator"
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

func (l *GetLocationLogic) GetLocation(in *pb.PlayerId) (*pb.PlayerLocation, error) {
	key := locationKey(in.Uid)
	data, err := l.svcCtx.RedisClient.Get(l.ctx, key).Bytes()
	if err == redis.Nil {
		return &pb.PlayerLocation{Uid: in.Uid, Online: false}, nil
	}
	if err != nil {
		return nil, err
	}

	loc := &pb.PlayerLocation{}
	if err := proto.Unmarshal(data, loc); err != nil {
		return nil, err
	}
	return loc, nil
}
