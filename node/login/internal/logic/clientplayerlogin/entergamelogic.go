package clientplayerloginlogic

import (
	"context"
	"github.com/golang/protobuf/proto"
	"login/client/playerdbservice"
	"login/data"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/svc"
	"login/pb/game"
	"strconv"

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

func (l *EnterGameLogic) EnterGame(in *game.EnterGameRequest) (*game.EnterGameResponse, error) {
	session, ok := ctxkeys.GetSession(l.ctx)

	resp := &game.EnterGameResponse{ErrorMessage: &game.TipInfoMessage{}}

	if !ok {
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginSessionIdNotFound)
		return resp, nil
	}

	sessionId, ok := ctxkeys.GetSessionID(l.ctx)
	if !ok {
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginSessionIdNotFound)
		return resp, nil
	}

	defer data.SessionList.Remove(sessionId)

	// Validate player ID belongs to the session
	if !l.isPlayerInSession(session, in.PlayerId) {
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginSessionIdNotFound)
		return resp, nil
	}

	// Transition session state to "Enter Game"
	if err := session.Fsm.Event(context.Background(), data.EventEnterGame); err != nil {
		logx.Error(err)
		return resp, nil
	}

	// Ensure player data is loaded in Redis
	if err := l.ensurePlayerDataInRedis(in.PlayerId); err != nil {
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginPlayerGuidError)
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
func (l *EnterGameLogic) notifyCentreService(in *game.EnterGameRequest) {
	centreRequest := &game.CentrePlayerGameNodeEntryRequest{
		ClientMsgBody: &game.CentreEnterGameRequest{
			PlayerId: in.PlayerId,
		},
	}
	l.svcCtx.GetCentreClient().Send(centreRequest, game.CentreServiceLoginNodeEnterGameMessageId)
}
