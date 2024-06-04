package deployservicelogic

import (
	"context"
	"deploy_server/pkg"
	"log"
	"strconv"

	"deploy_server/internal/svc"
	"deploy_server/pb/deploy"

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

func (l *GetNodeInfoLogic) GetNodeInfo(in *deploy.NodeInfoRequest) (*deploy.NodeInfoResponse, error) {
	// todo: add your logic here and delete this line
	response := &deploy.NodeInfoResponse{
		Info: &deploy.ServersInfoData{
			DatabaseInfo:   &deploy.DatabaseServerDb{},
			LoginInfo:      &deploy.LoginServerDb{},
			ControllerInfo: &deploy.ControllerServerDb{},
			GateInfo:       &deploy.GateServerDb{},
			RedisInfo:      &deploy.RedisServerDb{},
			LobbyInfo:      &deploy.LobbyServerDb{},
		},
	}
	log.Println("zone id ", in.ZoneId)
	return response, nil

	zoneId := strconv.FormatUint(uint64(in.ZoneId), 10)
	pkg.PbDb.LoadOneByKV(response.Info.GetDatabaseInfo(), "zone_id", zoneId)
	pkg.PbDb.LoadOneByKV(response.Info.GetLoginInfo(), "zone_id", zoneId)
	pkg.PbDb.LoadOneByKV(response.Info.GetControllerInfo(), "zone_id", zoneId)
	pkg.PbDb.LoadOneByKV(response.Info.GetGateInfo(), "zone_id", zoneId)
	pkg.PbDb.LoadOneByKV(response.Info.GetLobbyInfo(), "zone_id", zoneId)

	return response, nil
}
