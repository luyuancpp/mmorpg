package loginservicelogic

import (
	"context"
	"login/data"
	"strconv"
	"time"

	"github.com/golang/protobuf/proto"
	"login/internal/svc"
	"login/pb/game"

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

func (l *CreatePlayerLogic) CreatePlayer(in *game.CreatePlayerC2LRequest) (*game.CreatePlayerC2LResponse, error) {
	sessionId := strconv.FormatUint(in.SessionInfo.SessionId, 10)
	session, ok := data.SessionList.Get(sessionId)
	resp := &game.CreatePlayerC2LResponse{
		ClientMsgBody: &game.CreatePlayerResponse{
			Error:   &game.Tip{},
			Players: make([]*game.CAccountSimplePlayer, 0)},
	}
	resp.SessionInfo = in.SessionInfo
	if !ok {
		resp.ClientMsgBody.Error = &game.Tip{Id: 1}
		return resp, nil
	}

	key := "account" + session.Account
	cmd := l.svcCtx.Redis.Get(l.ctx, key)
	if cmd == nil {
		resp.ClientMsgBody.Error = &game.Tip{Id: 1}
		return resp, nil
	}

	accountData := &game.AccountDatabase{}
	err := proto.Unmarshal([]byte(cmd.Val()), accountData)
	if err != nil {
		return resp, nil
	}
	if len(resp.ClientMsgBody.Players) >= 3 {
		resp.ClientMsgBody.Error = &game.Tip{Id: 1001}
		return resp, nil
	}
	if nil == accountData.SimplePlayers {
		accountData.SimplePlayers =
			&game.AccountSimplePlayers{Players: make([]*game.AccountSimplePlayer, 0)}
	}

	playerDb := &game.AccountSimplePlayer{}
	playerDb.PlayerId = 1
	accountData.SimplePlayers.Players = append(accountData.SimplePlayers.Players, playerDb)
	for _, player := range accountData.SimplePlayers.Players {
		resp.ClientMsgBody.Players = append(resp.ClientMsgBody.Players, &game.CAccountSimplePlayer{Player: player})
	}

	dataMessage, err := proto.Marshal(accountData)
	if err != nil {
		logx.Error(err)
		return resp, nil
	}
	l.svcCtx.Redis.Set(l.ctx, key, dataMessage, time.Duration(12*time.Millisecond))
	return resp, err
}
