package deployservicelogic

import (
	"context"
	"deploy/internal/logic/pkg/node_id_etcd"

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
	err := node_id_etcd.ReleaseID(l.ctx, l.svcCtx.NodeEtcdClient, in.Id, in.NodeType)
	if err != nil {
		logx.Error(err)
		return &game.ReleaseIDResponse{}, err
	}

	return &game.ReleaseIDResponse{}, nil
}
