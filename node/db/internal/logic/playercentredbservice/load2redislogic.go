package playercentredbservicelogic

import (
	"context"
	"db/internal/logic/pkg/db"
	"db/internal/logic/pkg/queue"
	"hash/fnv"
	"strconv"

	"db/internal/svc"
	"db/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
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

func (l *Load2RedisLogic) Load2Redis(in *game.LoadPlayerCentreRequest) (*game.LoadPlayerCentreResponse, error) {
	resp := &game.LoadPlayerCentreResponse{}
	playerIdStr := strconv.FormatUint(in.PlayerId, 10)
	key := "player_center" + playerIdStr
	cmd := l.svcCtx.Redis.Get(l.ctx, key)
	if len(cmd.Val()) > 0 {
		resp.PlayerId = in.PlayerId
		return resp, nil
	}
	hash64 := fnv.New64a()
	_, err := hash64.Write([]byte(key))
	if err != nil {
		logx.Error(err)
		return nil, err
	}

	msgChannel := queue.MsgChannel{}
	msgChannel.Key = hash64.Sum64()
	msg := &game.PlayerCentreDatabase{}
	msgChannel.Body = msg
	msgChannel.Chan = make(chan bool)
	msgChannel.WhereCase = "where player_id='" + playerIdStr + "'"
	db.NodeDB.MsgQueue.Put(msgChannel)
	_, ok := <-msgChannel.Chan
	if !ok {
		logx.Error("channel closed")
		return nil, err
	}

	data, err := proto.Marshal(msg)
	if err != nil {
		logx.Error(err)
		return nil, err
	}

	l.svcCtx.Redis.Set(l.ctx, key, data, 0)
	resp.PlayerId = in.PlayerId
	return resp, nil
}
