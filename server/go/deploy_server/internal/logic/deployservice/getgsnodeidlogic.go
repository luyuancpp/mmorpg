package deployservicelogic

import (
	"context"

	"deploy_server/internal/svc"
	"deploy_server/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type GetGsNodeIdLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewGetGsNodeIdLogic(ctx context.Context, svcCtx *svc.ServiceContext) *GetGsNodeIdLogic {
	return &GetGsNodeIdLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *GetGsNodeIdLogic) GetGsNodeId(in *game.GsNodeIdRequest) (*game.GsNodeIdResponse, error) {
	// todo: add your logic here and delete this line

	return &game.GsNodeIdResponse{}, nil
}
