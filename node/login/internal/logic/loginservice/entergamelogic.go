package loginservicelogic

import (
	"context"
	"github.com/golang/protobuf/proto"
	"login/client/playerdbservice"
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
	defer data.SessionList.Remove(sessionId)

	//这里没有验证是不是自己账号里面的玩家

	playerIdStr := strconv.FormatUint(in.ClientMsgBody.PlayerId, 10)

	resp := &game.EnterGameC2LResponse{
		ClientMsgBody: &game.EnterGameResponse{ErrorMessage: &game.TipInfoMessage{}},
		SessionInfo:   in.SessionInfo,
	}

	session, ok := data.SessionList.Get(sessionId)

	// Check if session is valid
	if !ok {
		resp.ClientMsgBody.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	foundPlayer := false
	for _, v := range session.UserAccount.SimplePlayers.Players {
		if v.PlayerId != in.ClientMsgBody.PlayerId {
			continue
		}
		foundPlayer = true
	}

	if !foundPlayer {
		resp.ClientMsgBody.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	err := session.Fsm.Event(context.Background(), data.EventEnterGame) //开始创建角色状态
	if err != nil {
		logx.Error(err)
		return resp, nil
	}

	// Check if player data exists in Redis
	reflection := proto.MessageReflect(&game.PlayerDatabase{})
	key := string(reflection.Descriptor().FullName()) + playerIdStr
	playerData := l.svcCtx.Redis.Get(l.ctx, key).Val()

	if len(playerData) == 0 {
		// Session data not found in Redis, load it from the database
		service := playerdbservice.NewPlayerDBService(*l.svcCtx.DBClient)
		if _, err := service.Load2Redis(l.ctx, &game.LoadPlayerRequest{PlayerId: in.ClientMsgBody.PlayerId}); err != nil {
			resp.ClientMsgBody.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginPlayerGuidError)}
			return resp, err
		}
	}

	// Send request to the centre
	centreRequest := &game.CentrePlayerGameNodeEntryRequest{
		ClientMsgBody: in.ClientMsgBody,
		SessionInfo:   in.SessionInfo,
	}
	l.svcCtx.CentreClient.Send(centreRequest, game.CentreServiceLoginNodeEnterGameMessageId)

	return resp, nil
}
