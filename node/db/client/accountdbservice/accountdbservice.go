// Code generated by goctl. DO NOT EDIT.
// Source: db_service.proto

package accountdbservice

import (
	"context"

	"db/pb/game"

	"github.com/zeromicro/go-zero/zrpc"
	"google.golang.org/grpc"
)

type (
	LoadAccountRequest       = game.LoadAccountRequest
	LoadAccountResponse      = game.LoadAccountResponse
	LoadPlayerCentreRequest  = game.LoadPlayerCentreRequest
	LoadPlayerCentreResponse = game.LoadPlayerCentreResponse
	LoadPlayerRequest        = game.LoadPlayerRequest
	LoadPlayerResponse       = game.LoadPlayerResponse
	SaveAccountRequest       = game.SaveAccountRequest
	SaveAccountResponse      = game.SaveAccountResponse
	SavePlayerCentreRequest  = game.SavePlayerCentreRequest
	SavePlayerCentreResponse = game.SavePlayerCentreResponse
	SavePlayerRequest        = game.SavePlayerRequest
	SavePlayerResponse       = game.SavePlayerResponse

	AccountDBService interface {
		Load2Redis(ctx context.Context, in *LoadAccountRequest, opts ...grpc.CallOption) (*LoadAccountResponse, error)
		Save2Redis(ctx context.Context, in *SaveAccountRequest, opts ...grpc.CallOption) (*SaveAccountResponse, error)
	}

	defaultAccountDBService struct {
		cli zrpc.Client
	}
)

func NewAccountDBService(cli zrpc.Client) AccountDBService {
	return &defaultAccountDBService{
		cli: cli,
	}
}

func (m *defaultAccountDBService) Load2Redis(ctx context.Context, in *LoadAccountRequest, opts ...grpc.CallOption) (*LoadAccountResponse, error) {
	client := game.NewAccountDBServiceClient(m.cli.Conn())
	return client.Load2Redis(ctx, in, opts...)
}

func (m *defaultAccountDBService) Save2Redis(ctx context.Context, in *SaveAccountRequest, opts ...grpc.CallOption) (*SaveAccountResponse, error) {
	client := game.NewAccountDBServiceClient(m.cli.Conn())
	return client.Save2Redis(ctx, in, opts...)
}
