package logic

import (
	"context"
	"fmt"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	"player_locator/internal/svc"
	common "player_locator/proto/common"
	pb "player_locator/proto/player_locator"
)

type SetSessionLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewSetSessionLogic(ctx context.Context, svcCtx *svc.ServiceContext) *SetSessionLogic {
	return &SetSessionLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *SetSessionLogic) SetSession(in *pb.SetSessionRequest) (*common.Empty, error) {
	s := in.GetSession()
	if s == nil {
		return nil, fmt.Errorf("session is required")
	}

	key := sessionKey(s.PlayerId)
	data, err := proto.Marshal(s)
	if err != nil {
		return nil, err
	}

	// Online sessions have no TTL — persisted until SetDisconnecting or explicit delete
	if err := l.svcCtx.RedisClient.Set(l.ctx, key, data, 0).Err(); err != nil {
		return nil, err
	}

	return &common.Empty{}, nil
}
