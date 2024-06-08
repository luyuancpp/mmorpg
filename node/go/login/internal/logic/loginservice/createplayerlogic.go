package loginservicelogic

import (
	"context"
	"login_server/data"
	"strconv"

	"github.com/golang/protobuf/proto"
	"login_server/internal/svc"
	"login_server/pb/game"

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

func (l *CreatePlayerLogic) CreatePlayer(in *game.CreatePlayerC2LRequest) (*game.LoginNodeCreatePlayerResponse, error) {
	sessionId := strconv.FormatUint(in.SessionInfo.SessionId, 10)
	player, ok := data.SessionList.Get(sessionId)
	if !ok {
		return &game.LoginNodeCreatePlayerResponse{Error: &game.Tips{Id: 1}}, nil
	}

	rdKey := "account" + player.Account
	cmd := l.svcCtx.Rdb.Get(l.ctx, rdKey)
	if cmd == nil {
		return &game.LoginNodeCreatePlayerResponse{Error: &game.Tips{Id: 1}}, nil
	}

	resp := &game.LoginNodeCreatePlayerResponse{Players: &game.AccountDatabase{}}
	if len(resp.Players.SimplePlayers.Players) >= 3 {
		return &game.LoginNodeCreatePlayerResponse{Error: &game.Tips{Id: 1001}}, nil
	}

	err := proto.Unmarshal([]byte(cmd.Val()), resp.Players)
	return resp, err
}
