package deployservicelogic

import (
	"context"
	node_id_etcd "deploy/internal/logic/pkg/node_id_etcd"
	"deploy/internal/svc"
	"deploy/pb/game"
	"github.com/zeromicro/go-zero/core/logx"
)

type GetIDLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewGetIDLogic(ctx context.Context, svcCtx *svc.ServiceContext) *GetIDLogic {
	return &GetIDLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *GetIDLogic) GetID(in *game.GetIDRequest) (*game.GetIDResponse, error) {
	id, err := node_id_etcd.GenerateID(l.ctx, l.svcCtx.NodeEtcdClient, in.NodeType)
	if err != nil {
		logx.Error(err)
		return &game.GetIDResponse{}, err
	}

	return &game.GetIDResponse{Id: id}, nil
}
