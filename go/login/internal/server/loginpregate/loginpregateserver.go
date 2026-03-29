package loginpregate

import (
	"context"

	"login/internal/logic/pregate"
	"login/internal/svc"
	login "proto/login"
)

type LoginPreGateServer struct {
	svcCtx *svc.ServiceContext
	login.UnimplementedLoginPreGateServer
}

func NewLoginPreGateServer(svcCtx *svc.ServiceContext) *LoginPreGateServer {
	return &LoginPreGateServer{svcCtx: svcCtx}
}

func (s *LoginPreGateServer) GetGateList(ctx context.Context, in *login.GetGateListRequest) (*login.GetGateListResponse, error) {
	return pregate.GetGateList(ctx, s.svcCtx)
}
