package loginservicelogic

import (
	"context"
	"strconv"

	"github.com/golang/protobuf/proto"
	"github.com/zeromicro/go-zero/core/logx"

	"login/client/playerdbservice"
	"login/data"
	"login/internal/svc"
	"login/pb/game"
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
	// Convert session ID to string and ensure cleanup after processing
	sessionId := strconv.FormatUint(in.SessionInfo.SessionId, 10)
	defer data.SessionList.Remove(sessionId)

	// Initialize response structure
	resp := &game.EnterGameC2LResponse{
		ClientMsgBody: &game.EnterGameResponse{ErrorMessage: &game.TipInfoMessage{}},
		SessionInfo:   in.SessionInfo,
	}

	// Validate session
	session, ok := data.SessionList.Get(sessionId)
	if !ok {
		resp.ClientMsgBody.ErrorMessage.Id = uint32(game.LoginError_kLoginSessionIdNotFound)
		return resp, nil
	}

	// Validate player ID belongs to the session
	if !l.isPlayerInSession(session, in.ClientMsgBody.PlayerId) {
		resp.ClientMsgBody.ErrorMessage.Id = uint32(game.LoginError_kLoginSessionIdNotFound)
		return resp, nil
	}

	// Transition session state to "Enter Game"
	if err := session.Fsm.Event(context.Background(), data.EventEnterGame); err != nil {
		logx.Error(err)
		return resp, nil
	}

	// Ensure player data is loaded in Redis
	if err := l.ensurePlayerDataInRedis(in.ClientMsgBody.PlayerId); err != nil {
		resp.ClientMsgBody.ErrorMessage.Id = uint32(game.LoginError_kLoginPlayerGuidError)
		logx.Error(err)
		return resp, err
	}

	// Notify the central service about player entry
	l.notifyCentreService(in)

	return resp, nil
}

// Check if player exists in session
func (l *EnterGameLogic) isPlayerInSession(session *data.Session, playerId uint64) bool {
	if session.UserAccount == nil || session.UserAccount.SimplePlayers == nil || session.UserAccount.SimplePlayers.Players == nil {
		return false
	}

	for _, player := range session.UserAccount.SimplePlayers.Players {
		if player.PlayerId == playerId {
			return true
		}
	}
	return false
}

// Ensure player data is loaded in Redis
func (l *EnterGameLogic) ensurePlayerDataInRedis(playerId uint64) error {
	key := string(proto.MessageReflect(&game.PlayerDatabase{}).Descriptor().FullName()) + strconv.FormatUint(playerId, 10)
	playerData := l.svcCtx.Redis.Get(l.ctx, key).Val()

	if len(playerData) == 0 {
		// Load data from the database if not found in Redis
		dbService := playerdbservice.NewPlayerDBService(*l.svcCtx.DbClient)
		_, err := dbService.Load2Redis(l.ctx, &game.LoadPlayerRequest{PlayerId: playerId})
		return err
	}
	return nil
}

// Notify central service about player entry
func (l *EnterGameLogic) notifyCentreService(in *game.EnterGameC2LRequest) {
	centreRequest := &game.CentrePlayerGameNodeEntryRequest{
		ClientMsgBody: in.ClientMsgBody,
		SessionInfo:   in.SessionInfo,
	}
	l.svcCtx.GetCentreClient().Send(centreRequest, game.CentreServiceLoginNodeEnterGameMessageId)
}
