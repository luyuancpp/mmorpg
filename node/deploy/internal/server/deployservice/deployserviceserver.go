// Code generated by goctl. DO NOT EDIT.
// Source: deploy_service.proto

package server

import (
	"context"

	"deploy/internal/logic/deployservice"
	"deploy/internal/svc"
	"deploy/pb/game"
)

type DeployServiceServer struct {
	svcCtx *svc.ServiceContext
	game.UnimplementedDeployServiceServer
}

func NewDeployServiceServer(svcCtx *svc.ServiceContext) *DeployServiceServer {
	return &DeployServiceServer{
		svcCtx: svcCtx,
	}
}

func (s *DeployServiceServer) GetNodeInfo(ctx context.Context, in *game.NodeInfoRequest) (*game.NodeInfoResponse, error) {
	l := deployservicelogic.NewGetNodeInfoLogic(ctx, s.svcCtx)
	return l.GetNodeInfo(in)
}

func (s *DeployServiceServer) GetID(ctx context.Context, in *game.GetIDRequest) (*game.GetIDResponse, error) {
	l := deployservicelogic.NewGetIDLogic(ctx, s.svcCtx)
	return l.GetID(in)
}

func (s *DeployServiceServer) ReleaseID(ctx context.Context, in *game.ReleaseIDRequest) (*game.ReleaseIDResponse, error) {
	l := deployservicelogic.NewReleaseIDLogic(ctx, s.svcCtx)
	return l.ReleaseID(in)
}

func (s *DeployServiceServer) RenewLease(ctx context.Context, in *game.RenewLeaseIDRequest) (*game.RenewLeaseIDResponse, error) {
	l := deployservicelogic.NewRenewLeaseLogic(ctx, s.svcCtx)
	return l.RenewLease(in)
}
