package deployservicelogic

import (
	"context"

	"deploy_server/internal/svc"
	"deploy_server/pb/deploy"

	"github.com/zeromicro/go-zero/core/logx"
)

type GetNodeInfoLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewGetNodeInfoLogic(ctx context.Context, svcCtx *svc.ServiceContext) *GetNodeInfoLogic {
	return &GetNodeInfoLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *GetNodeInfoLogic) GetNodeInfo(in *deploy.NodeInfoRequest) (*deploy.NodeInfoResponse, error) {
	// todo: add your logic here and delete this line

	return &deploy.NodeInfoResponse{}, nil
}
