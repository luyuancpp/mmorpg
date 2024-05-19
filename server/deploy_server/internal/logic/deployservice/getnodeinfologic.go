package deployservicelogic

import (
	"context"
	"deploy_server/pkg"
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
	response := &deploy.NodeInfoResponse{Info: &deploy.ServersInfoData{GateInfo: &deploy.GateServerDb{}}}
	pkg.PbDb.LoadOneByKV(response.Info.GetGateInfo(), "zone_id", strconv.FormatUint(uint64(in.ZoneId), 10))
	return response, nil
}
