// Code generated by goctl. DO NOT EDIT.
// Source: deploy_service.proto

package deployservice

import (
	"context"

	"deploy/pb/game"

	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
)

type (
	CentreNodeDb      = game.CentreNodeDb
	CentreNodeListDb  = game.CentreNodeListDb
	DatabaseNodeDb    = game.DatabaseNodeDb
	GameNodeDb        = game.GameNodeDb
	GameNodeListDb    = game.GameNodeListDb
	GateNodeDb        = game.GateNodeDb
	GateNodeListDb    = game.GateNodeListDb
	GetIDRequest      = game.GetIDRequest
	GetIDResponse     = game.GetIDResponse
	LobbyNodeDb       = game.LobbyNodeDb
	LoginNodeDb       = game.LoginNodeDb
	LoginNodeListDb   = game.LoginNodeListDb
	NodeInfoRequest   = game.NodeInfoRequest
	NodeInfoResponse  = game.NodeInfoResponse
	NodesInfoData     = game.NodesInfoData
	RedisNodeDb       = game.RedisNodeDb
	RedisNodeListDb   = game.RedisNodeListDb
	ReleaseIDRequest  = game.ReleaseIDRequest
	ReleaseIDResponse = game.ReleaseIDResponse

	DeployService interface {
		GetNodeInfo(ctx context.Context, in *NodeInfoRequest, opts ...grpc.CallOption) (*NodeInfoResponse, error)
		GetID(ctx context.Context, in *GetIDRequest, opts ...grpc.CallOption) (*GetIDResponse, error)
		ReleaseID(ctx context.Context, in *ReleaseIDRequest, opts ...grpc.CallOption) (*ReleaseIDResponse, error)
	}

	defaultDeployService struct {
		cli zrpc.Client
	}
)

func NewDeployService(cli zrpc.Client) DeployService {
	return &defaultDeployService{
		cli: cli,
	}
}

func (m *defaultDeployService) GetNodeInfo(ctx context.Context, in *NodeInfoRequest, opts ...grpc.CallOption) (*NodeInfoResponse, error) {
	client := game.NewDeployServiceClient(m.cli.Conn())
	return client.GetNodeInfo(ctx, in, opts...)
}

func (m *defaultDeployService) GetID(ctx context.Context, in *GetIDRequest, opts ...grpc.CallOption) (*GetIDResponse, error) {
	client := game.NewDeployServiceClient(m.cli.Conn())
	return client.GetID(ctx, in, opts...)
}

func (m *defaultDeployService) ReleaseID(ctx context.Context, in *ReleaseIDRequest, opts ...grpc.CallOption) (*ReleaseIDResponse, error) {
	client := game.NewDeployServiceClient(m.cli.Conn())
	return client.ReleaseID(ctx, in, opts...)
}
