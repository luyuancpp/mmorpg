// Code generated by goctl. DO NOT EDIT.
// Source: player_locator.proto

package server

import (
	"context"

	"player_locator/internal/logic/playerlocator"
	"player_locator/internal/svc"
	"player_locator/pb/game"
)

type PlayerLocatorServer struct {
	svcCtx *svc.ServiceContext
	game.UnimplementedPlayerLocatorServer
}

func NewPlayerLocatorServer(svcCtx *svc.ServiceContext) *PlayerLocatorServer {
	return &PlayerLocatorServer{
		svcCtx: svcCtx,
	}
}

func (s *PlayerLocatorServer) SetLocation(ctx context.Context, in *game.PlayerLocation) (*game.Empty, error) {
	l := playerlocatorlogic.NewSetLocationLogic(ctx, s.svcCtx)
	return l.SetLocation(in)
}

func (s *PlayerLocatorServer) GetLocation(ctx context.Context, in *game.PlayerId) (*game.PlayerLocation, error) {
	l := playerlocatorlogic.NewGetLocationLogic(ctx, s.svcCtx)
	return l.GetLocation(in)
}

func (s *PlayerLocatorServer) MarkOffline(ctx context.Context, in *game.PlayerId) (*game.Empty, error) {
	l := playerlocatorlogic.NewMarkOfflineLogic(ctx, s.svcCtx)
	return l.MarkOffline(in)
}
