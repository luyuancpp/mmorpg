package dblogic

import (
	"context"

	"db/internal/svc"
	"db/proto/aproto/service/go/grpc/db_proto"

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

func (l *TestLogic) Test(in *db_proto.TestRequest) (*db_proto.TestResponse, error) {
	// todo: add your logic here and delete this line

	return &db_proto.TestResponse{}, nil
}
