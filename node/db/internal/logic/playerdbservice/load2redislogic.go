package playerdbservicelogic

import (
	"context"
	"db/internal/logic/pkg/utils"
	"db/internal/svc"
	"db/pb/game"
	"fmt"
	"strconv"
	"time"

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

	err := utils.BatchLoadAndCache(
		l.ctx,
		l.svcCtx.Redis,
		in.PlayerId,
		[]proto.Message{
			msgCentre,
		},
	)
	if err != nil {
		logx.Errorf("BatchLoadAndCache error: %v", err)
		return nil, err
	}

	playerAll := &game.PlayerAllData{}

	err = utils.LoadAggregateData(
		l.ctx,
		l.svcCtx.Redis,
		in.PlayerId,
		playerAll,
		func(id uint64) []proto.Message {
			return []proto.Message{
				&game.PlayerDatabase{PlayerId: id},
				&game.PlayerDatabase_1{PlayerId: id},
			}
		},
		func(messages []proto.Message, target proto.Message) error {
			allData := target.(*game.PlayerAllData)
			for _, m := range messages {
				switch msg := m.(type) {
				case *game.PlayerDatabase:
					allData.PlayerDatabaseData = msg
				case *game.PlayerDatabase_1:
					allData.PlayerDatabase_1Data = msg
				default:
					return fmt.Errorf("unexpected type %T", msg)
				}
			}
			return nil
		},
		func(id uint64) string {
			return "PlayerAllData:" + strconv.FormatUint(id, 10)
		},
		time.Hour,
	)

	if err != nil {
		logx.Errorf("Load2Redis error: %v", err)
		return nil, err
	}

	return resp, nil

}
