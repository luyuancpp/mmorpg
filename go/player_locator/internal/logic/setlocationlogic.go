package logic

import (
	"context"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/svc"
	common "proto/common/base"
	pb "proto/player_locator"
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

func (l *SetLocationLogic) SetLocation(in *pb.PlayerLocation) (*common.Empty, error) {
	key := locationKey(in.Uid)
	in.Ts = time.Now().Unix()
	in.Online = true

	data, err := proto.Marshal(in)
	if err != nil {
		return nil, err
	}

	if err := l.svcCtx.RedisClient.Set(l.ctx, key, data, 0).Err(); err != nil {
		return nil, err
	}

	return &common.Empty{}, nil
}
