package deployservicelogic

import (
	"context"
	"deploy/internal/logic/pkg/constants"
	"deploy/internal/logic/pkg/db"
	"strconv"

	"deploy/internal/svc"
	"deploy/pb/game"

	"github.com/redis/go-redis/v9"
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
	db.PbDb.LoadOneByWhereCase(response.Info.GetDatabaseInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetLoginInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetCentreInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetGameInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetGateInfo(), "where zone_id="+zoneId)
	db.PbDb.LoadListByWhereCase(response.Info.GetRedisInfo(), "where zone_id="+zoneId)

	//to do 分布式异步
	setKeyName := ""
	if game.ENodeType(in.GetNodeType()) == game.ENodeType_kCentreNode {
		response.NodeId = in.GetZoneId()
	} else if game.ENodeType(in.GetNodeType()) == game.ENodeType_kGameNode {
		setKeyName = constants.GameNodeSetKeyName
	} else if game.ENodeType(in.GetNodeType()) == game.ENodeType_kLoginNode {
		setKeyName = constants.LoginNodeSetKeyName
	} else if game.ENodeType(in.GetNodeType()) == game.ENodeType_kGateNode {
		setKeyName = constants.GateNodeSetKeyName
	}
	if setKeyName != "" {
		count, _ := l.svcCtx.Redis.ZCard(l.ctx, setKeyName).Result()
		nodeId := uint32(count) + 1
		err := l.svcCtx.Redis.ZAdd(l.ctx, setKeyName,
			redis.Z{Score: float64(in.GetZoneId()), Member: nodeId}).Err()
		if err != nil {
			logx.Error(err.Error())
			return response, err
		}
		response.NodeId = nodeId
	}
	response.NodeId = 1
	return response, nil
}
