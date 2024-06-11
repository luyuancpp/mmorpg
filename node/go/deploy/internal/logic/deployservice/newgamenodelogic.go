package deployservicelogic

import (
	"context"

	"deploy_server/internal/svc"
	"deploy_server/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type NewGameNodeLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewNewGameNodeLogic(ctx context.Context, svcCtx *svc.ServiceContext) *NewGameNodeLogic {
	return &NewGameNodeLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *NewGameNodeLogic) NewGameNode(in *game.NewGameNodeRequest) (*game.NewGameNodeResponse, error) {
	// todo: add your logic here and delete this line

	return &game.NewGameNodeResponse{}, nil
}
