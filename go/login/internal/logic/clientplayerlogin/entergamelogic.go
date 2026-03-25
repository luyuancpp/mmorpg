package clientplayerloginlogic

import (
	"context"
	"login/data"
	"login/generated/pb/table"
	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/dataloader"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsessionstore"
	"login/internal/logic/pkg/sessionmanager"
	"login/internal/svc"
	login_proto_common "proto/common/base"
	login_proto_database "proto/common/database"
	login_proto "proto/login"
	"strconv"
	"time"

	"google.golang.org/protobuf/proto"

	"github.com/zeromicro/go-zero/core/logx"
)

type EnterGameLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

type enterGameSessionState struct {
	playerID       uint64
	sessionID      uint64
	gateID         string
	gateInstanceID string
	account        string
	requestID      string
}

func NewEnterGameLogic(ctx context.Context, svcCtx *svc.ServiceContext) *EnterGameLogic {
	return &EnterGameLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *EnterGameLogic) EnterGame(in *login_proto.EnterGameRequest) (*login_proto.EnterGameResponse, error) {
	resp := &login_proto.EnterGameResponse{ErrorMessage: &login_proto_common.TipInfoMessage{}}
	// 1. Create timeout context (supports cancellation, preserves existing timeout config)
	ctx := l.ctx

	// 1. Get Session
	sessionDetails, sessionFound := ctxkeys.GetSessionDetails(ctx)
	if !sessionFound || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 2. Get LoginSessionInfo
	session, err := loginsessionstore.GetLoginSession(ctx, l.svcCtx.RedisClient, sessionDetails.SessionId)
	if err != nil {
		logx.Errorf("GetLoginSession failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginSessionNotFound)
		return resp, nil
	}

	// 3. Lock to prevent concurrent login for the same player
	playerLocker := locker.NewRedisLocker(l.svcCtx.RedisClient)
	key := "player_locker:" + strconv.FormatUint(in.PlayerId, 10)
	tryLocker, err := playerLocker.TryLock(ctx, key, time.Duration(config.AppConfig.Locker.PlayerLockTTL)*time.Second)
	if err != nil {
		logx.Errorf("EnterGame lock acquire failed for playerId=%d: %v", in.PlayerId, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
		return resp, nil
	}

	if !tryLocker.IsLocked() {
		logx.Errorf("EnterGame lock acquire failed for playerId=%d: lock not held", in.PlayerId)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
		return resp, nil
	}

	flowState := buildEnterGameSessionState(in, sessionDetails, session.Account)
	taskCallback := func(taskKey string, taskSuccess bool, err error) {
		decision, applyErr := l.applyLoadedPlayerSession(ctx, flowState)
		if applyErr != nil {
			logx.Errorf("Failed to apply loaded player session for player %d: %v", in.PlayerId, applyErr)
			return
		}

		logx.Infof("All tasks completed (decision=%d). playerId=%d", decision, in.PlayerId)
	}

	defer func() {
		released, releaseErr := tryLocker.Release(ctx)
		if releaseErr != nil {
			logx.Errorf("Failed to release lock: %v", releaseErr)
		} else if !released {
			logx.Infof("Lock was not held by us (possibly expired)")
		}
	}()

	// 4. Load account data and verify player ownership
	accountKey := constants.GetAccountDataKey(session.Account)
	dataBytes, err := l.svcCtx.RedisClient.Get(ctx, accountKey).Bytes()
	if err != nil {
		logx.Errorf("RedisClient Get user account failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginAccountNotFound)
		return resp, nil
	}

	userAccount := &login_proto_database.UserAccounts{}
	if err := proto.Unmarshal(dataBytes, userAccount); err != nil {
		logx.Errorf("Unmarshal user account failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginDataParseFailed)
		return resp, nil
	}

	found := false
	for _, p := range userAccount.SimplePlayers.GetPlayers() {
		if p.PlayerId == in.PlayerId {
			found = true
			break
		}
	}
	if !found {
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginEnterGameGuid)
		return resp, nil
	}

	// 5. FSM state management (by sessionId)
	sessionIdStr := strconv.FormatUint(sessionDetails.SessionId, 10)
	playerFSM := data.InitPlayerFSM()
	if err := fsmstore.LoadFSMState(ctx, l.svcCtx.RedisClient, playerFSM, sessionIdStr, ""); err != nil {
		logx.Errorf("FSM Load failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginFsmFailed)
		return resp, nil
	}

	if err := playerFSM.Event(ctx, data.EventEnterGame); err != nil {
		logx.Errorf("FSM Event failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginInProgress)
		return resp, nil
	}

	// 6. Load Player data (core refactor: sync wait for task completion)

	err = l.ensurePlayerDataInRedis(ctx, in.PlayerId, taskCallback)
	if err != nil {
		logx.Errorf("failed to ensure player data in redis [PlayerId=%d, error=%v]", in.PlayerId, err)
	} else {
		logx.Debugf("succeeded in ensuring player data in redis [PlayerId=%d]", in.PlayerId)
	}

	// 7. Clean up Session and FSM
	cleanupLoginSessionState(ctx, l.svcCtx, sessionDetails.SessionId, "enterGame")

	resp.PlayerId = in.PlayerId
	return resp, nil
}

func buildEnterGameSessionState(in *login_proto.EnterGameRequest, sessionDetails *login_proto_common.SessionDetails, account string) enterGameSessionState {
	return enterGameSessionState{
		playerID:       in.PlayerId,
		sessionID:      sessionDetails.SessionId,
		gateID:         strconv.FormatUint(uint64(sessionDetails.GetGateNodeId()), 10),
		gateInstanceID: sessionDetails.GetGateInstanceId(),
		account:        account,
		requestID:      in.GetRequestId(),
	}
}

func (l *EnterGameLogic) applyLoadedPlayerSession(ctx context.Context, state enterGameSessionState) (sessionmanager.EnterGameDecision, error) {
	existing, err := sessionmanager.GetSession(ctx, l.svcCtx.PlayerLocatorClient, state.playerID)
	if err != nil {
		return sessionmanager.FirstLogin, err
	}

	decision := sessionmanager.DecideEnterGame(existing, state.account)
	if err := l.persistEnterGameSession(ctx, decision, existing, state); err != nil {
		return decision, err
	}

	if err := sessionmanager.SetIdempotency(ctx, l.svcCtx.RedisClient, state.playerID, state.requestID); err != nil {
		logx.Errorf("Failed to set idempotency for player %d: %v", state.playerID, err)
	}

	return decision, nil
}

func (l *EnterGameLogic) persistEnterGameSession(
	ctx context.Context,
	decision sessionmanager.EnterGameDecision,
	existing *sessionmanager.PlayerSession,
	state enterGameSessionState,
) error {
	switch decision {
	case sessionmanager.FirstLogin, sessionmanager.ReplaceLogin:
		if decision == sessionmanager.ReplaceLogin {
			if err := l.kickReplacedSession(existing, state.sessionID); err != nil {
				logx.Errorf("Failed to kick old session for player %d: %v", state.playerID, err)
			}
		}

		newSession := sessionmanager.NewOnlineSession(existing, sessionmanager.OnlineSessionInput{
			PlayerID:       state.playerID,
			SessionID:      state.sessionID,
			GateID:         state.gateID,
			GateInstanceID: state.gateInstanceID,
			Account:        state.account,
			RequestID:      state.requestID,
			Now:            time.Now(),
		})
		return sessionmanager.SetSession(ctx, l.svcCtx.PlayerLocatorClient, newSession)
	case sessionmanager.ShortReconnect:
		_, err := sessionmanager.Reconnect(
			ctx,
			l.svcCtx.PlayerLocatorClient,
			state.playerID,
			state.sessionID,
			state.gateID,
			state.gateInstanceID,
			state.account,
			state.requestID,
		)
		return err
	default:
		return nil
	}
}

func (l *EnterGameLogic) kickReplacedSession(existing *sessionmanager.PlayerSession, currentSessionID uint64) error {
	if existing == nil {
		return nil
	}
	if existing.SessionID == currentSessionID || existing.GateID == "" {
		return nil
	}

	return l.svcCtx.KickSessionOnGate(existing.GateID, existing.GateInstanceID, existing.SessionID)
}

// ensurePlayerDataInRedis loads player data using the task-based callback pattern.
func (l *EnterGameLogic) ensurePlayerDataInRedis(
	ctx context.Context,
	playerId uint64,
	taskCallback func(taskKey string, taskSuccess bool, err error),
) error {
	messagesToLoad := []proto.Message{
		&login_proto_database.PlayerAllData{
			PlayerDatabaseData:   &login_proto_database.PlayerDatabase{PlayerId: playerId},
			PlayerDatabase_1Data: &login_proto_database.PlayerDatabase_1{PlayerId: playerId},
		},
		&login_proto_database.PlayerCentreDatabase{PlayerId: playerId},
	}

	// Invoke data load with the provided callback
	return dataloader.LoadWithPlayerId(
		ctx,
		l.svcCtx.RedisClient,
		l.svcCtx.KafkaClient,
		l.svcCtx.TaskExecutor,
		messagesToLoad,
		taskCallback,
	)
}
