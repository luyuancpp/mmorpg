package clientplayerloginlogic

import (
	"context"
	"github.com/looplab/fsm"
	"login/data"
	"login/internal/logic/pkg/ctxkeys"
	"time"

	"login/internal/svc"
	"login/pb/game"

	"github.com/golang/protobuf/proto"
	"github.com/zeromicro/go-zero/core/logx"
)

type CreatePlayerLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewCreatePlayerLogic(ctx context.Context, svcCtx *svc.ServiceContext) *CreatePlayerLogic {
	return &CreatePlayerLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *CreatePlayerLogic) CreatePlayer(in *game.CreatePlayerRequest) (*game.CreatePlayerResponse, error) {
	session, ok := ctxkeys.GetSession(l.ctx)
	resp := &game.CreatePlayerResponse{
		ErrorMessage: &game.TipInfoMessage{},
		Players:      make([]*game.AccountSimplePlayerWrapper, 0)}

	if !ok {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	defer func(Fsm *fsm.FSM, ctx context.Context, event string, args ...interface{}) {
		err := Fsm.Event(ctx, event, args)
		if err != nil {
			logx.Error(err)
		}
	}(session.Fsm, context.Background(), data.EventProcessLogin)

	key := "account" + session.Account
	cmd := l.svcCtx.Redis.Get(l.ctx, key)
	if cmd == nil {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginAccountNotFound)}
		return resp, nil
	}

	err := session.Fsm.Event(context.Background(), data.EventCreateChar) //开始创建角色状态
	if err != nil {
		logx.Error(err)
		return resp, nil
	}

	accountData := &game.UserAccounts{}
	err = proto.Unmarshal([]byte(cmd.Val()), accountData)

	if err != nil {
		return resp, nil
	}
	if len(resp.Players) >= 5 {
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginAccountPlayerFull)}
		return resp, nil
	}
	if nil == accountData.SimplePlayers {
		accountData.SimplePlayers =
			&game.AccountSimplePlayerList{Players: make([]*game.AccountSimplePlayer, 0)}
	}

	playerDb := &game.AccountSimplePlayer{}
	playerDb.PlayerId = uint64(l.svcCtx.SnowFlake.Generate())
	accountData.SimplePlayers.Players = append(accountData.SimplePlayers.Players, playerDb)
	for _, player := range accountData.SimplePlayers.Players {
		resp.Players = append(resp.Players, &game.AccountSimplePlayerWrapper{Player: player})
	}

	dataMessage, err := proto.Marshal(accountData)
	if err != nil {
		logx.Error(err)
		return resp, nil
	}

	l.svcCtx.Redis.Set(l.ctx, key, dataMessage, time.Duration(12*time.Hour))

	return resp, err
}
