package accountdbservicelogic

import (
	"context"
	"db/internal/constants"
	"db/internal/logic/pkg/db"
	"db/internal/logic/pkg/queue"
	"db/internal/svc"
	"db/pb/game"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"hash/fnv"
	"time"
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

func (l *Load2RedisLogic) Load2Redis(in *game.LoadAccountRequest) (*game.LoadAccountResponse, error) {
	//todo 如果这时候存回数据库呢,读存读存
	resp := &game.LoadAccountResponse{}
	key := constants.GetAccountDataKey(in.Account)
	cmd := l.svcCtx.Redis.Get(l.ctx, key)
	resp.Account = in.Account
	if len(cmd.Val()) > 0 {
		resp.Account = in.Account
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
	msg := &game.UserAccounts{}
	msgChannel.Body = msg
	msgChannel.Chan = make(chan bool)
	msgChannel.WhereCase = "where account='" + in.Account + "'"
	db.DB.MsgQueue.Put(msgChannel)
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

	l.svcCtx.Redis.Set(l.ctx, key, data, time.Duration(12*time.Hour))
	return resp, nil
}
