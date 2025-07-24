package accountdbservicelogic

import (
	"context"
	"db/internal/constants"
	"db/internal/svc"
	"db/pb/game"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
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
	cmd := l.svcCtx.RedisClient.Get(l.ctx, key)
	resp.Account = in.Account
	if len(cmd.Val()) > 0 {
		resp.Account = in.Account
		return resp, nil
	}

	msg := &game.UserAccounts{}
	data, err := proto.Marshal(msg)
	if err != nil {
		logx.Error(err)
		return nil, err
	}

	l.svcCtx.RedisClient.Set(l.ctx, key, data, time.Duration(12*time.Hour))
	return resp, nil
}
