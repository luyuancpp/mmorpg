package playerdbservicelogic

import (
	"context"
	"db/internal/logic/pkg/db"
	"db/internal/logic/pkg/queue"
	"hash/fnv"
	"strconv"

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
	resp := &game.LoadPlayerResponse{}
	resp.PlayerId = in.PlayerId

	msgPlayer := &game.PlayerDatabase{}
	msgPlayer.PlayerId = in.PlayerId

	msgCentrePlayer := &game.PlayerCentreDatabase{}
	msgCentrePlayer.PlayerId = in.PlayerId

	playerIdStr := strconv.FormatUint(in.PlayerId, 10)
	keyPlayer := string(proto.MessageReflect(msgPlayer).Descriptor().FullName()) + ":" + playerIdStr
	cmdPlayer := l.svcCtx.Redis.Get(l.ctx, keyPlayer)
	if len(cmdPlayer.Val()) > 0 {
		resp.PlayerId = in.PlayerId
		return resp, nil
	}

	hash64 := fnv.New64a()
	_, err := hash64.Write([]byte(playerIdStr))
	if err != nil {
		logx.Error(err)
		return nil, err
	}
	hashKey := hash64.Sum64()

	put := func(message proto.Message) *queue.MsgChannel {
		ch := queue.MsgChannel{}
		ch.Key = hashKey
		ch.Body = message
		ch.Chan = make(chan bool)
		ch.WhereCase = "where player_id='" + playerIdStr + "'"
		db.DB.MsgQueue.Put(ch)
		return &ch
	}

	channelScenePlayer := put(msgPlayer)

	channelCentrePlayer := put(msgCentrePlayer)

	keyCentrePlayer := string(proto.MessageReflect(msgCentrePlayer).Descriptor().FullName()) + ":" + playerIdStr
	cmd := l.svcCtx.Redis.Get(l.ctx, keyCentrePlayer)
	if len(cmd.Val()) > 0 {
		resp.PlayerId = in.PlayerId
		return resp, nil
	}

	<-channelScenePlayer.Chan
	<-channelCentrePlayer.Chan

	save2Redis := func(message proto.Message, key string) error {
		data, err := proto.Marshal(msgPlayer)
		if err != nil {
			logx.Error(err)
			return err
		}
		l.svcCtx.Redis.Set(l.ctx, key, data, 0)
		return nil
	}

	err = save2Redis(msgPlayer, keyPlayer)
	if err != nil {
		logx.Error(err)
		return nil, err
	}
	err = save2Redis(msgCentrePlayer, keyCentrePlayer)
	if err != nil {
		logx.Error(err)
		return nil, err
	}

	return resp, nil
}
