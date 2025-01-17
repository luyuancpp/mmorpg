package deployservicelogic

import (
	"context"
	"deploy/internal/logic/pkg/db"
	"deploy/internal/logic/pkg/node_id_etcd"
	"strconv"

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
	response := &game.NodeInfoResponse{
		Info: &game.NodesInfoData{
			DatabaseInfo: &game.DatabaseNodeDb{},
			LoginInfo:    &game.LoginNodeListDb{},
			CentreInfo:   &game.CentreNodeListDb{},
			GateInfo:     &game.GateNodeListDb{},
			GameInfo:     &game.GameNodeListDb{},
			RedisInfo:    &game.RedisNodeListDb{},
		},
	}

	id, leaseID, err := node_id_etcd.GenerateIDWithLease(l.ctx, l.svcCtx.NodeEtcdClient, in.NodeType)
	if err != nil {
		logx.Error(err)
		return response, err
	}

	response.NodeId = uint32(id)
	response.LeaseId = uint64(leaseID)

	zoneId := strconv.FormatUint(uint64(in.GetZoneId()), 10)
	db.PbDb.LoadOneByWhereCase(response.Info.GetDatabaseInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetLoginInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetCentreInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetGameInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetGateInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetRedisInfo(), "where zone_id="+zoneId)

	return response, nil
}
