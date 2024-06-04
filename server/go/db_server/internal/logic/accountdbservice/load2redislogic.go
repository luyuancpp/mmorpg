package accountdbservicelogic

import (
	"context"

	"db_server/internal/svc"
	"db_server/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type Load2RedisLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewLoad2RedisLogic(ctx context.Context, svcCtx *svc.ServiceContext) *Load2RedisLogic {
	return &Load2RedisLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *Load2RedisLogic) Load2Redis(in *game.LoadAccountRequest) (*game.LoadAccountResponse, error) {
	l.svcCtx.Rdb.HGet(l.svcCtx.Ctx, "account", in.Account)

	return &game.LoadAccountResponse{}, nil
}
