// Code generated by goctl. DO NOT EDIT.
// Source: login_service.proto

package loginservice

import (
	"context"

	"login/pb/game"

	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
)

type (
	CreatePlayerC2LRequest     = game.CreatePlayerC2LRequest
	CreatePlayerC2LResponse    = game.CreatePlayerC2LResponse
	EnterGameC2LRequest        = game.EnterGameC2LRequest
	EnterGameC2LResponse       = game.EnterGameC2LResponse
	LeaveGameC2LRequest        = game.LeaveGameC2LRequest
	LoginC2LRequest            = game.LoginC2LRequest
	LoginC2LResponse           = game.LoginC2LResponse
	LoginNodeDisconnectRequest = game.LoginNodeDisconnectRequest

	LoginService interface {
		Login(ctx context.Context, in *LoginC2LRequest, opts ...grpc.CallOption) (*LoginC2LResponse, error)
		CreatePlayer(ctx context.Context, in *CreatePlayerC2LRequest, opts ...grpc.CallOption) (*CreatePlayerC2LResponse, error)
		EnterGame(ctx context.Context, in *EnterGameC2LRequest, opts ...grpc.CallOption) (*EnterGameC2LResponse, error)
		LeaveGame(ctx context.Context, in *LeaveGameC2LRequest, opts ...grpc.CallOption) (*Empty, error)
		Disconnect(ctx context.Context, in *LoginNodeDisconnectRequest, opts ...grpc.CallOption) (*Empty, error)
	}

	defaultLoginService struct {
		cli zrpc.Client
	}
)

func NewLoginService(cli zrpc.Client) LoginService {
	return &defaultLoginService{
		cli: cli,
	}
}

func (m *defaultLoginService) Login(ctx context.Context, in *LoginC2LRequest, opts ...grpc.CallOption) (*LoginC2LResponse, error) {
	client := game.NewLoginServiceClient(m.cli.Conn())
	return client.Login(ctx, in, opts...)
}

func (m *defaultLoginService) CreatePlayer(ctx context.Context, in *CreatePlayerC2LRequest, opts ...grpc.CallOption) (*CreatePlayerC2LResponse, error) {
	client := game.NewLoginServiceClient(m.cli.Conn())
	return client.CreatePlayer(ctx, in, opts...)
}

func (m *defaultLoginService) EnterGame(ctx context.Context, in *EnterGameC2LRequest, opts ...grpc.CallOption) (*EnterGameC2LResponse, error) {
	client := game.NewLoginServiceClient(m.cli.Conn())
	return client.EnterGame(ctx, in, opts...)
}

func (m *defaultLoginService) LeaveGame(ctx context.Context, in *LeaveGameC2LRequest, opts ...grpc.CallOption) (*Empty, error) {
	client := game.NewLoginServiceClient(m.cli.Conn())
	return client.LeaveGame(ctx, in, opts...)
}

func (m *defaultLoginService) Disconnect(ctx context.Context, in *LoginNodeDisconnectRequest, opts ...grpc.CallOption) (*Empty, error) {
	client := game.NewLoginServiceClient(m.cli.Conn())
	return client.Disconnect(ctx, in, opts...)
}