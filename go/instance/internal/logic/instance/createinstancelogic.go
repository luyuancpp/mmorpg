package instancelogic

import (
	"context"

	"instance/internal/svc"
	"instance/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type CreateInstanceLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewCreateInstanceLogic(ctx context.Context, svcCtx *svc.ServiceContext) *CreateInstanceLogic {
	return &CreateInstanceLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// 创建副本
func (l *CreateInstanceLogic) CreateInstance(in *game.CreateInstanceRequest) (*game.CreateInstanceResponse, error) {
	// todo: add your logic here and delete this line

	return &game.CreateInstanceResponse{}, nil
}
