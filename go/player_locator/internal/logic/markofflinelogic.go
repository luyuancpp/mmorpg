package logic

import (
	"context"

	"github.com/zeromicro/go-zero/core/logx"

	"player_locator/internal/svc"
	common "player_locator/proto/common"
	pb "player_locator/proto/player_locator"
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
	l.svcCtx.RedisClient.Del(l.ctx, key)
	return &common.Empty{}, nil
}
