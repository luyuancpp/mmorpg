package dblogic

import (
	"context"

	"db/db/proto/service/go/grpc/db"
	"db/internal/svc"

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

func (l *TestLogic) Test(in *db_db.Empty) (*db_db.Empty, error) {
	// todo: add your logic here and delete this line

	return &db_db.Empty{}, nil
}
