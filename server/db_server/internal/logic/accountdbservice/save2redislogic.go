package accountdbservicelogic

import (
	"context"

	"db_sever/internal/svc"
	"db_sever/pb/db"

	"github.com/zeromicro/go-zero/core/logx"
)

type Save2RedisLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewSave2RedisLogic(ctx context.Context, svcCtx *svc.ServiceContext) *Save2RedisLogic {
	return &Save2RedisLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *Save2RedisLogic) Save2Redis(in *db.SaveAccountRequest) (*db.SaveAccountResponse, error) {
	// todo: add your logic here and delete this line

	return &db.SaveAccountResponse{}, nil
}
