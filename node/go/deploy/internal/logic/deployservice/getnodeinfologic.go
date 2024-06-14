package deployservicelogic

import (
	"context"
	"deploy/pkg"
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

	zoneId := strconv.FormatUint(uint64(in.GetZoneId()), 10)
	pkg.PbDb.LoadOneByWhereCase(response.Info.GetDatabaseInfo(), "where zone_id="+zoneId)
	pkg.PbDb.LoadListByWhereCase(response.Info.GetLoginInfo(), "where zone_id="+zoneId)
	pkg.PbDb.LoadListByWhereCase(response.Info.GetCentreInfo(), "where zone_id="+zoneId)
	pkg.PbDb.LoadListByWhereCase(response.Info.GetGameInfo(), "where zone_id="+zoneId)
	pkg.PbDb.LoadListByWhereCase(response.Info.GetGateInfo(), "where zone_id="+zoneId)
	pkg.PbDb.LoadListByWhereCase(response.Info.GetRedisInfo(), "where zone_id="+zoneId)

	//to do 分布式异步
	if game.ENodeType(in.GetNodeType()) == game.ENodeType_kCentreNode {
		response.NodeId = 1
	} else if game.ENodeType(in.GetNodeType()) == game.ENodeType_kGameNode {
		response.NodeId = 1
	} else if game.ENodeType(in.GetNodeType()) == game.ENodeType_kLoginNode {
		response.NodeId = 1
	} else if game.ENodeType(in.GetNodeType()) == game.ENodeType_kGateNode {
		response.NodeId = 1
	}

	return response, nil
}
