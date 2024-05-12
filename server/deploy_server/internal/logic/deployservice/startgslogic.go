package deployservicelogic

import (
	"context"

	"deploy_server/internal/svc"
	"deploy_server/pb/deploy"

	"github.com/zeromicro/go-zero/core/logx"
)

type StartGsLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewStartGsLogic(ctx context.Context, svcCtx *svc.ServiceContext) *StartGsLogic {
	return &StartGsLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *StartGsLogic) StartGs(in *deploy.StartGsRequest) (*deploy.StartGsResponse, error) {
	// todo: add your logic here and delete this line

	return &deploy.StartGsResponse{}, nil
}
