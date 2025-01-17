package deployservicelogic

import (
	"context"
	"deploy/internal/logic/pkg/node_id_etcd"
	clientv3 "go.etcd.io/etcd/client/v3"

	"deploy/internal/svc"
	"deploy/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type RenewLeaseLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewRenewLeaseLogic(ctx context.Context, svcCtx *svc.ServiceContext) *RenewLeaseLogic {
	return &RenewLeaseLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *RenewLeaseLogic) RenewLease(in *game.RenewLeaseIDRequest) (*game.RenewLeaseIDResponse, error) {
	err := node_id_etcd.RenewLease(l.ctx, l.svcCtx.NodeEtcdClient, clientv3.LeaseID(in.LeaseId))
	if err != nil {
		logx.Error(err)
		return &game.RenewLeaseIDResponse{}, err
	}
	return &game.RenewLeaseIDResponse{}, nil
}
