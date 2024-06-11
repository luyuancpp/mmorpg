package deployservicelogic

import (
	"context"

	"deploy_server/internal/svc"
	"deploy_server/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type GetGameNodeInfoLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewGetGameNodeInfoLogic(ctx context.Context, svcCtx *svc.ServiceContext) *GetGameNodeInfoLogic {
	return &GetGameNodeInfoLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *GetGameNodeInfoLogic) GetGameNodeInfo(in *game.GetGameNodeInfoRequest) (*game.GetGameNodeInfoResponse, error) {
	// todo: add your logic here and delete this line

	return &game.GetGameNodeInfoResponse{}, nil
}
