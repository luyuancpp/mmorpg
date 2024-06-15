package loginservicelogic

import (
	"context"
	"login/data"
	"strconv"

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
	// todo: add your logic here and delete this line

	sessionId := strconv.FormatUint(in.SessionInfo.SessionId, 10)
	player, ok := data.SessionList.Get(sessionId)
	resp := &game.CreatePlayerC2LResponse{}

	if !ok {
		resp.ClientMsgBody.Error = &game.Tip{Id: 1}
		return resp, nil
	}

	rdKey := "account" + player.Account
	cmd := l.svcCtx.Rdb.Get(l.ctx, rdKey)
	if cmd == nil {
		resp.ClientMsgBody.Error = &game.Tip{Id: 1}
		return resp, nil
	}

	accountData := &game.AccountDatabase{}
	if len(resp.ClientMsgBody.Players) >= 3 {

		resp.ClientMsgBody.Error = &game.Tip{Id: 1001}
		return resp, nil
	}

	err := proto.Unmarshal([]byte(cmd.Val()), accountData)
	return resp, err
}
