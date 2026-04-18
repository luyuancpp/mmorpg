package loginpregatelogic

import (
	"context"

	"login/internal/svc"
	"login/proto/login/proto/login"

	"github.com/zeromicro/go-zero/core/logx"
)

type AssignGateLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewAssignGateLogic(ctx context.Context, svcCtx *svc.ServiceContext) *AssignGateLogic {
	return &AssignGateLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

// Login picks the least-loaded Gate for the given zone, signs a one-time
func (l *AssignGateLogic) AssignGate(in *login.AssignGateRequest) (*login.AssignGateResponse, error) {
	// todo: add your logic here and delete this line

	return &login.AssignGateResponse{}, nil
}
