package clientplayerloginlogic

import (
	"context"
	"github.com/golang/protobuf/proto"
	"login/client/playerdbservice"
	"login/data"
	"login/internal/constants"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/logic/pkg/loginsessionstore"
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
	resp := &game.EnterGameResponse{ErrorMessage: &game.TipInfoMessage{}}

	// 2. 获取 Session
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 1. 获取 LoginSessionInfo（含 Account）
	session, err := loginsessionstore.GetLoginSession(l.ctx, l.svcCtx.Redis, sessionDetails.SessionId)
	if err != nil {
		logx.Errorf("GetLoginSession failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginSessionNotFound)
		return resp, nil
	}

	// 2. 获取 UserAccount
	accountKey := constants.GetAccountDataKey(session.Account)
	cmd := l.svcCtx.Redis.Get(l.ctx, accountKey)
	if err := cmd.Err(); err != nil {
		logx.Errorf("Redis Get user account failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginAccountNotFound)
		return resp, nil
	}

	userAccount := &game.UserAccounts{}
	if err := proto.Unmarshal([]byte(cmd.Val()), userAccount); err != nil {
		logx.Errorf("Unmarshal user account failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginDataParseFailed)
		return resp, nil
	}

	// 3. 校验角色归属
	found := false
	for _, p := range userAccount.SimplePlayers.Players {
		if p.PlayerId == in.PlayerId {
			found = true
			break
		}
	}
	if !found {
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginEnterGameGuid)
		return resp, nil
	}

	// 4. FSM 状态变更
	f := data.InitPlayerFSM()
	if err := fsmstore.LoadFSMState(l.ctx, l.svcCtx.Redis, f, session.Account, ""); err != nil {
		logx.Errorf("FSM Load failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginFsmFailed)
		return resp, nil
	}
	if err := f.Event(context.Background(), data.EventEnterGame); err != nil {
		logx.Errorf("FSM Event failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginInProgress)
		return resp, nil
	}
	_ = fsmstore.SaveFSMState(l.ctx, l.svcCtx.Redis, f, session.Account, "")

	// 5. 加载 Player 数据
	if err := l.ensurePlayerDataInRedis(in.PlayerId); err != nil {
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginPlayerGuidError)
		logx.Errorf("Load player data failed: %v", err)
		return resp, err
	}

	// 6. 通知中心服
	l.notifyCentreService(in)

	resp.PlayerId = in.PlayerId
	return resp, nil
}

// Ensure player data is loaded in Redis
func (l *EnterGameLogic) ensurePlayerDataInRedis(playerId uint64) error {
	key := string(proto.MessageReflect(&game.PlayerDatabase{}).Descriptor().FullName()) + ":" + strconv.FormatUint(playerId, 10)
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
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok {
		logx.Error("Session not found in context during enter centre ")
		return
	}

	centreRequest := &game.CentrePlayerGameNodeEntryRequest{
		ClientMsgBody: &game.CentreEnterGameRequest{
			PlayerId: in.PlayerId,
		},
		SessionInfo: sessionDetails,
	}
	node := l.svcCtx.GetCentreClient()
	if nil == node {
		return
	}
	node.Send(centreRequest, game.CentreLoginNodeEnterGameMessageId)
}
