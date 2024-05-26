package accountdbservicelogic

import (
	"context"

	"db_sever/internal/svc"
	"db_sever/pb/db"

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

func (l *Load2RedisLogic) Load2Redis(in *db.LoadAccountRequest) (*db.LoadAccountResponse, error) {
	// todo: add your logic here and delete this line

	return &db.LoadAccountResponse{}, nil
}
