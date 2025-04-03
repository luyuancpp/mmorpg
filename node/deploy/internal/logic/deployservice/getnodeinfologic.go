package deployservicelogic

import (
	"context"
	"deploy/internal/logic/pkg/node_id_etcd"
	"deploy/internal/svc"
	"deploy/pb/game"

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

func (l *GetNodeInfoLogic) GetNodeInfo(in *game.NodeInfoRequest) (*game.NodeInfoResponse, error) {
	response := &game.NodeInfoResponse{}

	id, leaseID, err := node_id_etcd.GenerateIDWithLease(l.ctx, l.svcCtx.NodeEtcdClient, in.NodeType)
	if err != nil {
		logx.Error(err)
		return response, err
	}

	response.NodeId = uint32(id)
	response.LeaseId = uint64(leaseID)

	return response, nil
}
