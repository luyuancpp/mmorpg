package clientplayerloginlogic

import (
	"context"
	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/dataloader"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsession"
	"login/internal/logic/pkg/sessionmanager"
	"login/internal/svc"
	login_proto_common "proto/common/base"
	login_proto_database "proto/common/database"
	login_proto "proto/login"
	smpb "proto/scene_manager"
	"shared/generated/pb/table"
	"strconv"
	"time"

	"google.golang.org/protobuf/proto"

	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/zrpc"
)

type EnterGameLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

type enterGameSessionState struct {
	playerID       uint64
	sessionID      uint32
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
	ctx := l.ctx

	// 1. Get Session
	sessionDetails, sessionFound := ctxkeys.GetSessionDetails(ctx)
	if !sessionFound || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 2. Get account
	account, err := loginsession.GetAccount(ctx, l.svcCtx.RedisClient, sessionDetails.SessionId)
	if err != nil {
		logx.Errorf("GetAccount failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginSessionNotFound)
		return resp, nil
	}

	// 3. Lock to prevent concurrent login for the same player.
	//    Single attempt — NO retry/sleep loop. The RPC handler must never
	//    block. On contention we return kLoginInProgress immediately and the
	//    client retries; the heartbeat below keeps the lock alive across the
	//    full async chain so retries see the contended state.
	playerLocker := locker.NewRedisLocker(l.svcCtx.RedisClient)
	key := "player_locker:" + strconv.FormatUint(in.PlayerId, 10)
	lockTTL := time.Duration(config.AppConfig.Locker.PlayerLockTTL) * time.Second
	tryLocker, err := playerLocker.TryLock(ctx, key, lockTTL)
	if err != nil {
		logx.Errorf("EnterGame lock error for playerId=%d: %v", in.PlayerId, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisError)}
		return resp, nil
	}
	if !tryLocker.IsLocked() {
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
		return resp, nil
	}

	flowState := buildEnterGameSessionState(in, sessionDetails, account)

	// Lock release is deferred until either the early-return paths below run
	// (validation failure / pool reject) or the background goroutine finishes
	// the async EnterGame chain. We track ownership via `lockHandedOff` so the
	// fallback defer here only fires when the goroutine never started.
	lockHandedOff := false
	defer func() {
		if lockHandedOff {
			return
		}
		releaseCtx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
		defer cancel()
		released, releaseErr := tryLocker.Release(releaseCtx)
		if releaseErr != nil {
			logx.Errorf("Failed to release lock for playerId=%d: %v", in.PlayerId, releaseErr)
		} else if !released {
			logx.Infof("Lock was not held by us (possibly expired) for playerId=%d", in.PlayerId)
		}
	}()

	// 4. Load account data and verify player ownership
	accountKey := constants.GetAccountDataKey(account)
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

	// 5. Kick off the EnterGame chain entirely event-driven. The RPC returns
	//    immediately; the client gets the actual "in scene" notification when
	//    Gate consumes the BindSession event and Scene pushes enter-scene.
	//
	//    Flow:
	//      a. EnsurePlayerAllDataInRedisAsync sends Kafka DB-read tasks and
	//         registers callbacks with the TaskResultDispatcher (Pub/Sub
	//         driven). NO goroutine waits on BLPOP.
	//      b. When all sub-results arrive, the dispatcher invokes our
	//         completion callback which runs applyLoadedPlayerSession +
	//         cleanup, then releases the player lock.
	//
	//    The Kafka send itself is the only sync I/O on this path; we wrap it
	//    in SubmitPreload so the gRPC handler thread never touches the
	//    SyncProducer mutex. The pool task exits as soon as Kafka send
	//    returns — no waiting on results.
	enterCtx := flowState // value-copied state for the closure
	playerID := in.PlayerId
	sessionID := sessionDetails.SessionId

	// Background context for the whole chain — independent of gRPC ctx (which
	// is cancelled the instant we return below). chainCancel both stops the
	// timeout and cancels manually from the lost-lock heartbeat path.
	const chainBudget = 5 * time.Minute
	chainCtx, chainCancel := context.WithTimeout(context.Background(), chainBudget)

	releaseLock := func() {
		releaseCtx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
		defer cancel()
		released, releaseErr := tryLocker.Release(releaseCtx)
		if releaseErr != nil {
			logx.Errorf("Failed to release lock for playerId=%d: %v", playerID, releaseErr)
		} else if !released {
			logx.Infof("Lock was not held by us (possibly expired) for playerId=%d", playerID)
		}
	}

	// Heartbeat: renew lock every TTL/3 so a slow EnterGame chain (DB cold,
	// dispatcher TTL retry, etc.) never lets the lock expire mid-flight. If
	// we lose ownership (Redis flap, manual kill, etc.) we cancel chainCtx
	// to abort applyLoadedPlayerSession before it can issue a stale
	// EnterScene against an already-replaced session.
	stopHeartbeat := tryLocker.StartHeartbeat(
		lockTTL/3,
		lockTTL,
		func(err error) {
			logx.Errorf("EnterGame lost lock mid-chain [PlayerId=%d]: %v", playerID, err)
			chainCancel()
		},
	)

	onPreloadComplete := func(err error) {
		// Stop the heartbeat first so it cannot race with Release.
		stopHeartbeat()
		defer chainCancel()
		defer releaseLock()

		if err != nil {
			logx.Errorf("EnterGame preload failed [PlayerId=%d]: %v", playerID, err)
			return
		}

		decision, applyErr := l.applyLoadedPlayerSession(chainCtx, enterCtx)
		if applyErr != nil {
			logx.Errorf("EnterGame apply session failed [PlayerId=%d]: %v", playerID, applyErr)
			return
		}
		logx.Infof("EnterGame complete (decision=%d) playerId=%d", decision, playerID)

		cleanupLoginSessionState(chainCtx, l.svcCtx, sessionID, "enterGame")
	}

	const dispatcherTaskTTL = 30 * time.Second
	ok := l.svcCtx.SubmitPreload(func() {
		dataloader.EnsurePlayerAllDataInRedisAsync(
			chainCtx,
			l.svcCtx.RedisClient,
			l.svcCtx.TaskResultDispatcher,
			l.svcCtx.KafkaClient,
			playerID,
			dispatcherTaskTTL,
			onPreloadComplete,
		)
	})

	if !ok {
		// Pool saturated — outer defer will release the lock since the
		// goroutine never ran. Surface back-pressure to the client.
		chainCancel()
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginInProgress)
		return resp, nil
	}

	// Ownership of the lock has now transferred to the async chain;
	// suppress the outer defer.
	lockHandedOff = true

	// RPC returns success; client waits for Gate/Scene push to know they're in.
	resp.ErrorMessage = nil
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
	version, err := l.persistEnterGameSession(ctx, decision, existing, state)
	if err != nil {
		return decision, err
	}

	// Notify Gate: bind the session and carry the login decision so Gate can forward to Scene.
	enterGsType := sessionmanager.DecisionToEnterGsType(decision)
	if err := l.svcCtx.SendBindSessionToGate(
		state.gateID, state.gateInstanceID,
		state.sessionID, state.playerID, version, enterGsType,
	); err != nil {
		logx.Errorf("Failed to send BindSessionToGate for player %d: %v", state.playerID, err)
		return decision, err
	}

	// Route player to Scene node via SceneManager.
	// For reconnect/replace: use existing scene_id so player returns to same scene.
	// For first login: scene_id=0 tells SceneManager to pick a node via load balancing.
	var sceneID uint64
	if existing != nil {
		sceneID = existing.SceneID
	}
	smTimeout := time.Duration(config.AppConfig.SceneManagerRpc.Timeout) * time.Millisecond
	enterResp, err := l.svcCtx.SceneManagerClient.EnterScene(ctx, &smpb.EnterSceneRequest{
		PlayerId:       state.playerID,
		SceneId:        sceneID,
		SessionId:      state.sessionID,
		RequestId:      state.requestID,
		GateId:         state.gateID,
		GateInstanceId: state.gateInstanceID,
		GateZoneId:     config.AppConfig.Node.ZoneId,
		ZoneId:         config.AppConfig.Node.ZoneId,
	}, zrpc.WithCallTimeout(smTimeout))
	if err != nil {
		logx.Errorf("SceneManager.EnterScene failed for player %d: %v", state.playerID, err)
		return decision, err
	}
	if enterResp.ErrorCode != 0 {
		logx.Errorf("SceneManager.EnterScene returned error for player %d: code=%d msg=%s",
			state.playerID, enterResp.ErrorCode, enterResp.ErrorMessage)
	}

	// Cross-zone redirect: SceneManager already pushed RedirectToGateEvent to the gate.
	// Skip idempotency — the player hasn't entered a scene yet; they'll re-login in the new zone.
	if enterResp.Redirect != nil {
		logx.Infof("Player %d cross-zone redirect to %s:%d",
			state.playerID, enterResp.Redirect.TargetGateIp, enterResp.Redirect.TargetGatePort)
		return decision, nil
	}

	if err := sessionmanager.SetIdempotency(ctx, l.svcCtx.RedisClient, state.playerID, state.requestID); err != nil {
		logx.Errorf("Failed to set idempotency for player %d: %v", state.playerID, err)
	}

	return decision, nil
}

// persistEnterGameSession persists the session and returns the new session version.
func (l *EnterGameLogic) persistEnterGameSession(
	ctx context.Context,
	decision sessionmanager.EnterGameDecision,
	existing *sessionmanager.PlayerSession,
	state enterGameSessionState,
) (uint32, error) {
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
		if err := sessionmanager.SetSession(ctx, l.svcCtx.PlayerLocatorClient, newSession); err != nil {
			return 0, err
		}
		return newSession.SessionVersion, nil
	case sessionmanager.ShortReconnect:
		updated, err := sessionmanager.Reconnect(
			ctx,
			l.svcCtx.PlayerLocatorClient,
			state.playerID,
			state.sessionID,
			state.gateID,
			state.gateInstanceID,
			state.account,
			state.requestID,
		)
		if err != nil {
			return 0, err
		}
		return updated.SessionVersion, nil
	default:
		return 0, nil
	}
}

func (l *EnterGameLogic) kickReplacedSession(existing *sessionmanager.PlayerSession, currentSessionID uint32) error {
	if existing == nil {
		return nil
	}
	if existing.SessionID == currentSessionID || existing.GateID == "" {
		return nil
	}

	return l.svcCtx.KickSessionOnGate(existing.GateID, existing.GateInstanceID, existing.SessionID, existing.PlayerID)
}
