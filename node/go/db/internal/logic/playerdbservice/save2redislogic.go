package playerdbservicelogic

import (
	"context"

	"db_server/internal/svc"
	"db_server/pb/game"

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

func (l *Save2RedisLogic) Save2Redis(in *game.SavePlayerRequest) (*game.SavePlayerResponse, error) {
	// todo: add your logic here and delete this line

	return &game.SavePlayerResponse{}, nil
}
