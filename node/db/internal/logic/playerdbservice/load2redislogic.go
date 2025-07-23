package playerdbservicelogic

import (
	"context"
	"db/internal/logic/pkg/utils"
	"db/internal/svc"
	"db/pb/game"

	"github.com/golang/protobuf/proto"
	"github.com/zeromicro/go-zero/core/logx"
)

type Load2RedisLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewLoad2RedisLogic(ctx context.Context, svcCtx *svc.ServiceContext) *Load2RedisLogic {
	return &Load2RedisLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *Load2RedisLogic) Load2Redis(in *game.LoadPlayerRequest) (*game.LoadPlayerResponse, error) {
	resp := &game.LoadPlayerResponse{PlayerId: in.PlayerId}

	msgCentre := &game.PlayerCentreDatabase{PlayerId: in.PlayerId}
	msgPlayer := &game.PlayerDatabase{PlayerId: in.PlayerId}
	msgPlayer1 := &game.PlayerDatabase_1{PlayerId: in.PlayerId}

	err := utils.BatchLoadAndCache(
		l.ctx,
		l.svcCtx.Redis,
		in.PlayerId,
		[]proto.Message{
			msgPlayer,
			msgCentre,
			msgPlayer1,
		},
	)
	if err != nil {
		return nil, err
	}

	return resp, nil

}
