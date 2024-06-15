package loginservicelogic

import (
	"context"
	"login/client/dbservice/playerdbservice"
	"login/data"
	"strconv"

	"login/internal/svc"
	"login/pb/game"

	"github.com/zeromicro/go-zero/core/logx"
)

type EnterGameLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewEnterGameLogic(ctx context.Context, svcCtx *svc.ServiceContext) *EnterGameLogic {
	return &EnterGameLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *EnterGameLogic) EnterGame(in *game.EnterGameC2LRequest) (*game.EnterGameC2LResponse, error) {
	sessionId := strconv.FormatUint(in.SessionInfo.SessionId, 10)
	_, ok := data.SessionList.Get(sessionId)
	resp := &game.EnterGameC2LResponse{}
	if ok {
		resp.ClientMsgBody.Error = &game.Tip{Id: 1005}
		return resp, nil
	}
	rdKey := "player" + strconv.FormatUint(in.ClientMsgBody.PlayerId, 10)
	cmd := l.svcCtx.Rdb.Get(l.ctx, rdKey)
	if cmd == nil {
		ps := playerdbservice.NewPlayerDBService(*l.svcCtx.DBCli)
		_, err := ps.Load2Redis(l.ctx, &game.LoadPlayerRequest{PlayerId: in.ClientMsgBody.PlayerId})
		if err != nil {
			resp.ClientMsgBody.Error = &game.Tip{Id: 1005}
			return resp, err
		}
		cmd = l.svcCtx.Rdb.Get(l.ctx, rdKey)
		if cmd == nil {
			logx.Error("cannot oad playerID:" + rdKey)
			resp.ClientMsgBody.Error = &game.Tip{Id: 1005}
			return resp, err
		}
	}
	return resp, nil
}
