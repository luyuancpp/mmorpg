package clientplayerloginlogic

import (
	"context"
	"errors"
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

	"github.com/redis/go-redis/v9"
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
		// Self-heal path: the authoritative ownership record is the reverse
		// index `player_id -> account` written by CreatePlayer alongside the
		// forward account_data write. If that reverse index agrees with the
		// caller's account, then account_data cache is stale (TTL expiry,
		// partial write, cross-instance replication lag, etc.) and we can
		// transparently restore the forward record rather than rejecting a
		// legitimately-owned player with kLoginEnterGameGuid.
		//
		// This eliminates the whole class of false-positive EnterGame
		// failures we see under stress load (~0.1-1% of robots), without
		// weakening ownership checks: the reverse key is only written in
		// CreatePlayer under the account_lock, so seeing it here proves
		// CreatePlayer succeeded for the same account at some point.
		reverseKey := constants.PlayerToAccountKey(in.PlayerId)
		ownerAccount, rErr := l.svcCtx.RedisClient.Get(ctx, reverseKey).Result()
		if rErr != nil || ownerAccount != account {
			if rErr != nil && !errors.Is(rErr, redis.Nil) {
				logx.Errorf("EnterGame reverse-index lookup failed: playerId=%d err=%v", in.PlayerId, rErr)
			} else {
				logx.Infof("EnterGame rejected: playerId=%d not owned by account=%s (owner=%q)",
					in.PlayerId, account, ownerAccount)
			}
			resp.ErrorMessage.Id = uint32(table.LoginError_kLoginEnterGameGuid)
			return resp, nil
		}

		// Reverse index confirms ownership. Patch account_data back into a
		// consistent state so subsequent flows see the player.
		logx.Infof("EnterGame self-heal: restoring playerId=%d into account_data for account=%s",
			in.PlayerId, account)
		if userAccount.SimplePlayers == nil {
			userAccount.SimplePlayers = &login_proto_common.AccountSimplePlayerList{
				Players: make([]*login_proto_common.AccountSimplePlayer, 0, 1),
			}
		}
		userAccount.SimplePlayers.Players = append(userAccount.SimplePlayers.Players,
			&login_proto_common.AccountSimplePlayer{PlayerId: in.PlayerId})
		if patched, mErr := proto.Marshal(userAccount); mErr == nil {
			if sErr := l.svcCtx.RedisClient.Set(ctx, accountKey, patched,
				config.AppConfig.Account.CacheExpire).Err(); sErr != nil {
				// Non-fatal: self-heal is best-effort; ownership is proven by
				// the reverse index, so we still proceed to EnterGame.
				logx.Errorf("EnterGame self-heal write-back failed: account=%s err=%v", account, sErr)
			}
		}
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

	// chainStart is the t0 for the total / preload latency histograms.
	// We start the clock here (not before TryLock) because the histograms
	// are measuring "what happens AFTER the lock is taken until we release
	// it", which is the exact window we need to attribute the err25 stalls
	// to a specific stage. See metrics.go for the bucket rationale.
	chainStart := time.Now()

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
			// Mark this chain as "lock_lost" for the total histogram.
			// observeTotal in onPreloadComplete won't run because the
			// dispatcher callback may never fire after the chain is
			// cancelled mid-flight; record it here so the histogram has
			// closure even on the lost-lock path.
			observeTotal(chainStart, ResultLockLost)
		},
	)

	onPreloadComplete := func(err error) {
		// Stop the heartbeat first so it cannot race with Release.
		stopHeartbeat()
		defer chainCancel()
		defer releaseLock()

		// preloadSeconds: from chain start to the moment the dispatcher
		// callback fires. Includes Kafka send + DB worker turnaround +
		// dispatcher Pub/Sub delivery.
		preloadResult := ResultSuccess
		if err != nil {
			preloadResult = ResultPreloadFailed
		}
		preloadSeconds.ObserveFloat(time.Since(chainStart).Seconds(), preloadResult)

		if err != nil {
			logx.Errorf("EnterGame preload failed [PlayerId=%d]: %v", playerID, err)
			observeTotal(chainStart, ResultPreloadFailed)
			return
		}

		// applySeconds wraps the whole apply phase (GetSession + persist
		// + BindGate + EnterScene). Sub-stages are recorded separately
		// inside applyLoadedPlayerSession.
		applyStart := time.Now()
		decision, applyErr := l.applyLoadedPlayerSession(chainCtx, enterCtx)
		applyResult := ResultSuccess
		if applyErr != nil {
			applyResult = ResultApplyFailed
		}
		applySeconds.ObserveFloat(time.Since(applyStart).Seconds(), applyResult)

		if applyErr != nil {
			logx.Errorf("EnterGame apply session failed [PlayerId=%d]: %v", playerID, applyErr)
			observeTotal(chainStart, ResultApplyFailed)
			return
		}
		logx.Infof("EnterGame complete (decision=%d) playerId=%d", decision, playerID)

		cleanupLoginSessionState(chainCtx, l.svcCtx, sessionID, "enterGame")
		observeTotal(chainStart, ResultSuccess)
	}

	// dispatcherTaskTTL caps how long the EnsurePlayerAllDataInRedisAsync
	// chain will wait for DB-task callbacks before declaring the preload
	// failed. The (f) instrumented 25k smoke (2026-05-28) showed:
	//   - successful preload avg = 3s
	//   - failed preload avg = 35s (was 30s + dispatcher overhead)
	//   - 2.7% failure rate consumed 25% of the preload pool's wall time
	// 30s was way too generous: while a failed preload sits in the wait,
	// player_locker:{playerId} stays held, every reconnect for that
	// account hits err25, and the failure cascades into a retry storm.
	// 5s lets the chain give up early — the client retry path is faster
	// than waiting for an unresponsive Kafka/dispatcher leg, and the
	// scene-side Redis NIL retry already compensates for genuinely lost
	// preloads.
	const dispatcherTaskTTL = 5 * time.Second
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

	// Post-merge one-shot signals (server-merge-gap-fixes.md §1 / §5).
	// Read flags stamped by tools/merge_zone/post_merge_stamp.go; if
	// either is set, populate the response and DELete the key so the
	// next login doesn't re-trigger the UI. Best-effort: read errors
	// don't block login (the player can still enter; they just miss
	// the one-shot notice this round and we'll catch them next time).
	consumePostMergeFlags(ctx, l.svcCtx.RedisClient, in.PlayerId, resp)

	// RPC returns success; client waits for Gate/Scene push to know they're in.
	resp.ErrorMessage = nil
	resp.PlayerId = in.PlayerId
	return resp, nil
}

// consumePostMergeFlags reads the two Redis flag keys written by
// tools/merge_zone/post_merge_stamp.go and:
//   • populates resp.PostMergeNoticeTs / resp.ForceRenameRequired
//   • DELetes the keys so subsequent logins don't re-fire the UI
//
// Best-effort: any Redis error is logged and swallowed. The semantic
// is "show notice once if possible"; missing it is recoverable, and
// blocking the player's login because we can't read a flag is worse
// than the alternative.
//
// Key prefixes are duplicated from tools/merge_zone/post_merge_stamp.go
// — keep them in sync. Crossing module boundaries (tools/merge_zone
// has its own go.mod) for a constants file isn't worth the build
// complexity for two short strings.
func consumePostMergeFlags(
	ctx context.Context,
	rdb *redis.Client,
	playerID uint64,
	resp *login_proto.EnterGameResponse,
) {
	const (
		mergeNoticeKey = "player_merge_notice:"
		forceRenameKey = "player_force_rename:"
	)
	pidStr := strconv.FormatUint(playerID, 10)

	// One pipeline read for both keys; one pipeline DEL for both. Two
	// pipelines instead of one because we don't want to DEL keys we
	// haven't yet read — Redis doesn't promise GET-then-DEL atomicity
	// in a single pipeline (it's two separate ops on the wire).
	pipe := rdb.Pipeline()
	noticeCmd := pipe.Get(ctx, mergeNoticeKey+pidStr)
	renameCmd := pipe.Get(ctx, forceRenameKey+pidStr)
	if _, err := pipe.Exec(ctx); err != nil && err != redis.Nil {
		// Pipeline-level errors (network) are non-fatal. Per-key Nil
		// errors are normal — most players don't have these flags.
		logx.Errorf("consumePostMergeFlags: pipeline get failed for player=%d: %v (continuing)", playerID, err)
		return
	}

	if v, err := noticeCmd.Result(); err == nil && v != "" {
		if ts, parseErr := strconv.ParseInt(v, 10, 64); parseErr == nil {
			resp.PostMergeNoticeTs = ts
		}
	}
	if _, err := renameCmd.Result(); err == nil {
		// Mere presence of the key is the signal; value is the merge
		// timestamp for ops audit but doesn't gate the flag itself.
		resp.ForceRenameRequired = true
	}

	if resp.PostMergeNoticeTs == 0 && !resp.ForceRenameRequired {
		// Nothing to consume — skip the DEL round-trip.
		return
	}

	delPipe := rdb.Pipeline()
	if resp.PostMergeNoticeTs != 0 {
		delPipe.Del(ctx, mergeNoticeKey+pidStr)
	}
	if resp.ForceRenameRequired {
		// NOTE: we do NOT DEL the rename flag here — clearing it must
		// happen only after the player has actually picked a new name.
		// Otherwise a player who dismisses the rename UI loses the gate
		// permanently. Today the project has no rename RPC at all
		// (CreatePlayer is empty, no nickname surface; see
		// docs/ops/merge-zone-runbook.md §4.4 reality note); when a
		// rename RPC eventually lands, *that* handler is the single
		// deleter of `player_force_rename:{player_id}`. Until then the
		// flag is dormant — force_rename_required will never be set
		// because nobody can stamp it without a name to clash on.
	}
	if _, err := delPipe.Exec(ctx); err != nil {
		logx.Errorf("consumePostMergeFlags: pipeline del failed for player=%d: %v (flags consumed but not cleared, will re-fire next login)", playerID, err)
	}
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
	// Stage observation: GetSession round-trip to player_locator.
	getSessionTimer := observeStage(applyGetSessionSeconds)
	existing, err := sessionmanager.GetSession(ctx, l.svcCtx.PlayerLocatorClient, state.playerID)
	getSessionTimer()
	if err != nil {
		return sessionmanager.FirstLogin, err
	}

	decision := sessionmanager.DecideEnterGame(existing, state.account)

	// Stage observation: persistEnterGameSession (SetSession or Reconnect).
	// Label by decision so the reconnect (CAS) tail doesn't get hidden by the
	// first/replace (write) tail.
	persistTimer := observeStage(applyPersistSessionSeconds, decisionLabel(decision))
	version, err := l.persistEnterGameSession(ctx, decision, existing, state)
	persistTimer()
	if err != nil {
		return decision, err
	}

	// Notify Gate: bind the session and carry the login decision so Gate can forward to Scene.
	enterGsType := sessionmanager.DecisionToEnterGsType(decision)
	bindTimer := observeStage(applyBindGateSeconds)
	bindErr := l.svcCtx.SendBindSessionToGate(
		state.gateID, state.gateInstanceID,
		state.sessionID, state.playerID, version, enterGsType,
	)
	bindTimer()
	if bindErr != nil {
		logx.Errorf("Failed to send BindSessionToGate for player %d: %v", state.playerID, bindErr)
		return decision, bindErr
	}

	// Route player to Scene node via SceneManager.
	// For reconnect/replace: use existing scene_id so player returns to same scene.
	// For first login: scene_id=0 tells SceneManager to pick a node via load balancing.
	var sceneID uint64
	if existing != nil {
		sceneID = existing.SceneID
	}
	smTimeout := time.Duration(config.AppConfig.SceneManagerRpc.Timeout) * time.Millisecond
	// Stage observation: SceneManager.EnterScene RPC. This is the prime
	// suspect under load — it routes through SceneManager → Scene
	// AssignScene + Kafka. If err25 root cause is "async chain stalls",
	// this histogram is where the long tail will appear.
	enterSceneTimer := observeStage(applyEnterSceneSeconds)
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
	enterSceneTimer()
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
