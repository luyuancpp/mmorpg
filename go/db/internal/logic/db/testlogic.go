package dblogic

import (
	"context"

	"game/internal/svc"
	"generated/pb/game/"

	"github.com/zeromicro/go-zero/core/logx"
)

type TestLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewTestLogic(ctx context.Context, svcCtx *svc.ServiceContext) *TestLogic {
	return &TestLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *TestLogic) Test(in *game.Empty) (*game.Empty, error) {
	// todo: add your logic here and delete this line

	return &game.Empty{}, nil
}
