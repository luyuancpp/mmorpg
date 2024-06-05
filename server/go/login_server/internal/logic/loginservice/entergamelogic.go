package loginservicelogic

import (
	"context"
	"github.com/zeromicro/go-zero/core/logx"
	"login_server/client/dbservice/playerdbservice"
	"login_server/data"
	"login_server/internal/svc"
	"login_server/pb/game"
	"strconv"
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

func (l *EnterGameLogic) EnterGame(in *game.EnterGameC2LRequest) (*game.EnterGameResponse, error) {
	sessionId := strconv.FormatUint(in.SessionInfo.SessionId, 10)
	_, ok := data.SessionList.Get(sessionId)
	if ok {
		return &game.EnterGameResponse{Error: &game.Tips{Id: 1005}}, nil
	}
	rdKey := "player" + strconv.FormatUint(in.PlayerId, 10)
	cmd := l.svcCtx.Rdb.Get(l.ctx, rdKey)
	if cmd == nil {
		ps := playerdbservice.NewPlayerDBService(*l.svcCtx.DBCli)
		_, err := ps.Load2Redis(l.ctx, &game.LoadPlayerRequest{PlayerId: in.PlayerId})
		if err != nil {
			return &game.EnterGameResponse{Error: &game.Tips{Id: 1005}}, err
		}
		cmd = l.svcCtx.Rdb.Get(l.ctx, rdKey)
		if cmd == nil {
			logx.Error("cannot oad playerID:" + rdKey)
			return &game.EnterGameResponse{Error: &game.Tips{Id: 1005}}, err
		}
	}
	return &game.EnterGameResponse{}, nil
}
