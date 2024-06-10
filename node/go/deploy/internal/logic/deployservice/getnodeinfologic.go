package deployservicelogic

import (
	"context"
	"deploy_server/pkg"
	"strconv"

	"deploy_server/internal/svc"
	"deploy_server/pb/game"

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
	// todo: add your logic here and delete this line

	response := &game.NodeInfoResponse{
		Info: &game.ServersInfoData{
			DatabaseInfo: &game.DatabaseServerDb{},
			LoginInfo:    &game.LoginServerDb{},
			CentreInfo:   &game.CentreServerDb{},
			GateInfo:     &game.GateServerDb{},
			RedisInfo:    &game.RedisServerDb{},
		},
	}
	//log.Println(strconv.FormatUint(uint64(in.GetZoneId()), 10))

	zoneId := strconv.FormatUint(uint64(in.GetZoneId()), 10)
	pkg.PbDb.LoadOneByKV(response.Info.GetDatabaseInfo(), "zone_id", zoneId)
	pkg.PbDb.LoadOneByKV(response.Info.GetLoginInfo(), "zone_id", zoneId)
	pkg.PbDb.LoadOneByKV(response.Info.GetCentreInfo(), "zone_id", zoneId)
	pkg.PbDb.LoadOneByKV(response.Info.GetGateInfo(), "zone_id", zoneId)
	return response, nil
}
