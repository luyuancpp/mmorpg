package deployservicelogic

import (
	"context"

	"deploy/internal/svc"
	"deploy/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type ReleaseIDLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewReleaseIDLogic(ctx context.Context, svcCtx *svc.ServiceContext) *ReleaseIDLogic {
	return &ReleaseIDLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *ReleaseIDLogic) ReleaseID(in *game.ReleaseIDRequest) (*game.ReleaseIDResponse, error) {
	// todo: add your logic here and delete this line

	return &game.ReleaseIDResponse{}, nil
}
