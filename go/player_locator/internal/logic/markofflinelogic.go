package logic

import (
	"context"

	"github.com/zeromicro/go-zero/core/logx"

	"player_locator/internal/svc"
	common "proto/common/base"
	pb "proto/player_locator"
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

func (l *MarkOfflineLogic) MarkOffline(in *pb.PlayerId) (*common.Empty, error) {
	key := locationKey(in.Uid)
	if err := l.svcCtx.RedisClient.Del(l.ctx, key).Err(); err != nil {
		l.Errorf("MarkOffline: failed to delete location for player %d: %v", in.Uid, err)
	}
	return &common.Empty{}, nil
}
