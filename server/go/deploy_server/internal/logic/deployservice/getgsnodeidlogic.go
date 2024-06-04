package deployservicelogic

import (
	"context"
	"deploy_server/internal/svc"
	"deploy_server/pb/deploy"

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

func (l *GetGsNodeIdLogic) GetGsNodeId(in *deploy.GsNodeIdRequest) (*deploy.GsNodeIdResponse, error) {
	// todo: add your logic here and delete this line
	return &deploy.GsNodeIdResponse{}, nil
}
