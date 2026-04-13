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

func (s *LoginPreGateServer) AssignGate(ctx context.Context, in *login.AssignGateRequest) (*login.AssignGateResponse, error) {
	return pregate.AssignGate(ctx, s.svcCtx, in)
}
