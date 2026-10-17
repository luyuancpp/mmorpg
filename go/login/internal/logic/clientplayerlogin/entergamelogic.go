package clientplayerloginlogic

import (
	"context"
	"errors"
	"fmt"
	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/dataloader"
	"login/internal/logic/pkg/homezone"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsession"
	"login/internal/logic/pkg/playernamereg"
	"login/internal/logic/pkg/sessionfence"
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
	// kicker 是 ReplaceLogin 踢旧会话用的发送能力;nil = 生产实现 l.svcCtx(见 replacedKicker)。只供单测替换。
	kicker replacedSessionKicker
}

type enterGameSessionState struct {
	playerID        uint64
	sessionID       uint32
	gateID          string
	gateInstanceID  string
	account         string
	requestID       string
	classID         uint32
	appearanceID    string
	gender          uint32
	playerLockKey   string
	playerLockToken string
	// ticketTargetZoneID 是本连接所持重定向票据的目标 zone(gate 验签后经 SessionDetails 透传);
	// 0 = 普通 AssignGate 票据。等于本 zone 时进场景不按 home_zone 弹回(CZ-8)。
	ticketTargetZoneID uint32
	// playerName 是要补进 PlayerProfileComp 的角色名副本,由 resolveEnterName 在入场前定好;
	// "" = 账号记录与名字注册表里都拿不到(旧角色 / 注册表不可用),本次入场不补名字。
	playerName string
	// gate 连接会话栅栏(docs/design/client-request-ordering.md §6.2):fenced = 发起本次 EnterGame 的 gate
	// 带了会话代次(旧版 gate 为 false,沿用旧行为);fenceGen 是入口处读到的 LeaveGame 计数,
	// 后台链绑定 / 进场前复查据此识别「期间已离开游戏」。
	fenced   bool
	fenceKey sessionfence.Key
	fenceGen uint64
}

// enterGameChainBudget 是 EnterGame 后台链(预加载 → 写会话 → 绑 gate → 进场)的总预算。
// gate 连接会话栅栏记录的寿命由它派生:记录必须比所有还可能读它的操作活得久(sessionfence.TTLFor)。
const enterGameChainBudget = 5 * time.Minute

var sessionFenceTTL = sessionfence.TTLFor(enterGameChainBudget)

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

	// 1b. 重定向票据的持票者绑定(cross-zone-scene-travel.md CZ-8 / §6 不变量 4)。
	//     跨 zone 传送的票据由 scene_manager 签给**某一个**玩家;这里是 player_id 已知、且尚未
	//     加锁 / 写任何会话状态的最早时刻,在此拒绝不留残留。0 = 普通 AssignGate 票据(签票时
	//     还没登录)/ dev 旁路 / 旧版 gate,一律放行。
	if holder := sessionDetails.GetTicketPlayerId(); holder != 0 && holder != in.PlayerId {
		logx.Errorf("EnterGame rejected: redirect ticket holder mismatch ticket_player=%d request_player=%d session=%d",
			holder, in.PlayerId, sessionDetails.SessionId)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginEnterGameGuid)}
		return resp, nil
	}

	// 2. Get account
	account, err := loginsession.GetAccount(ctx, l.svcCtx.RedisClient, sessionDetails.SessionId)
	if err != nil {
		logx.Errorf("GetAccount failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginSessionNotFound)
		return resp, nil
	}

	// 2b. gate 连接会话栅栏(docs/design/client-request-ordering.md §6.2):连接已断开就不再受理;
	//     这条连接已经绑定了别的角色(连发两次 EnterGame / 换角没先 LeaveGame)也拒绝,否则一个会话会绑两个角色。
	//     记下入口时的 LeaveGame 计数,后台链据此识别「受理之后已离开游戏」。在加玩家锁之前判,拒绝不留任何残留。
	fenceKey, fenced := sessionfence.KeyOf(sessionDetails)
	var fenceGen uint64
	if fenced {
		outcome, gen, fenceErr := sessionfence.CheckEnterGame(ctx, l.svcCtx.RedisClient, fenceKey, in.PlayerId, sessionFenceTTL)
		if fenceErr != nil {
			logx.Errorf("EnterGame session fence check failed: playerId=%d session=%d err=%v", in.PlayerId, sessionDetails.SessionId, fenceErr)
			resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisError)}
			return resp, nil
		}
		switch outcome {
		case sessionfence.OutcomeClosed:
			// 回包也送不到(连接已断),复用既有码。
			resp.ErrorMessage.Id = uint32(table.LoginError_kLoginSessionNotFound)
			return resp, nil
		case sessionfence.OutcomeOtherPlayer:
			logx.Infof("EnterGame rejected: another role is already bound on this connection playerId=%d session=%d", in.PlayerId, sessionDetails.SessionId)
			resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
			return resp, nil
		}
		fenceGen = gen
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
	flowState.playerLockKey = tryLocker.Key
	flowState.playerLockToken = tryLocker.Value
	flowState.fenced = fenced
	flowState.fenceKey = fenceKey
	flowState.fenceGen = fenceGen

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
	for _, p := range userAccount.GetSimplePlayers().GetPlayers() {
		if p.GetPlayerId() == in.PlayerId {
			found = true
			// 职业只能来自已验证归属的账号角色记录，不能由入场请求自由指定。
			flowState.classID = p.GetClassId()
			flowState.appearanceID = p.GetAppearanceId()
			flowState.gender = p.GetGender()
			// 名字与职业同源。账号记录有名字就直接用(不多一次 RPC);只有缺名的记录
			// (早于名字功能的旧角色、此前 self-heal 恢复出的空记录)才回源注册表。
			flowState.playerName = resolveEnterName(ctx, p.GetName(), l.svcCtx.PlayerNames, in.PlayerId)
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
		// 恢复出的记录原本只有 id,名字副本会就此永久丢失:角色列表显示空白,
		// 首次入场也补不进 PlayerProfileComp。所以这里回源注册表一次,把名字一并写回;
		// 查不到 / 查询失败就照旧只恢复 id(展示数据 fail-open,不阻断入场)。
		name := resolveEnterName(ctx, "", l.svcCtx.PlayerNames, in.PlayerId)
		flowState.playerName = name
		if userAccount.SimplePlayers == nil {
			userAccount.SimplePlayers = &login_proto_common.AccountSimplePlayerList{
				Players: make([]*login_proto_common.AccountSimplePlayer, 0, 1),
			}
		}
		userAccount.SimplePlayers.Players = append(userAccount.SimplePlayers.Players,
			&login_proto_common.AccountSimplePlayer{PlayerId: in.PlayerId, Name: name})
		if patched, mErr := proto.Marshal(userAccount); mErr == nil {
			if sErr := l.svcCtx.RedisClient.Set(ctx, accountKey, patched, 0).Err(); sErr != nil {
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
	//
	//    对客户端的契约:本 RPC 的应答**无错 = 已受理**,不等于已进场。之后链上任何一步
	//    没成(预加载失败 / applyLoadedPlayerSession 失败,含 EnterScene 被 scene_manager
	//    拒绝),服务端经 gate 推一条 SendTipToClient,tip = kEnterSceneFailed
	//    (notifyEnterGameFailed)。客户端收到即可收口,自身等 NotifyEnterScene 的超时只作兜底。
	// flowState 在这里按值拷进闭包:classID / playerName 这类入场前定好的字段必须在此之前赋完,
	// 之后再改 flowState 进不到 applyLoadedPlayerSession → backfillPlayerIdentity,而且不报任何错
	// (回归用例:TestEnterGame_PlayerNameReachesIdentityBackfill)。
	enterCtx := flowState // value-copied state for the closure
	playerID := in.PlayerId
	sessionID := sessionDetails.SessionId

	// Background context for the whole chain — independent of gRPC ctx (which
	// is cancelled the instant we return below). chainCancel both stops the
	// timeout and cancels manually from the lost-lock heartbeat path.
	const chainBudget = enterGameChainBudget
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
		// 失败出口(契约见 notifyEnterGameFailed):本函数的 gRPC 应答早已同步回了"成功",
		// 链上之后的失败只能经 gate 推 tip 告诉客户端。failedStage 非空 = 客户端会因此等不到
		// NotifyEnterScene。这个 defer 注册在最前、因而**最后**执行 —— 排在停心跳与释放玩家锁
		// 之后:推送是一次同步 Kafka 写,Kafka 抖动时可能阻塞数秒,不能让它拉长持锁窗口
		// (持锁期间该账号的每次重连都是 kLoginInProgress)。
		// 反过来不成立:failedStage 只在下面两个显式失败分支置位。本回调中途 panic(跑在 dispatcher
		// 的 worker / sweep goroutine 上,会被其 recover 吞掉)时 failedStage 仍为空,不通知,
		// 由客户端自己的 60s 进场超时兜底。
		failedStage := ""
		defer func() {
			if failedStage != "" {
				notifyEnterGameFailed(l.svcCtx, enterCtx, failedStage)
			}
		}()
		defer chainCancel()
		defer releaseLock()
		// 会话落盘和职业 / 名字补齐期间继续续租；退出时先停心跳，再释放锁。
		defer stopHeartbeat()

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
			failedStage = notifyStagePreload
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
			// applyLoadedPlayerSession 的每一个 error 返回点(GetSession / 身份补齐 / 铸登录代次 / 会话落盘 /
			// BindSession / EnterScene 调用失败 / EnterScene 被拒)都发生在 scene_manager 放行之前,
			// 客户端都等不到 NotifyEnterScene;而它自行处置后返回 nil 的路径(跨区重定向已推给
			// gate、ReplaceLogin 踢旧会话失败只记日志)走不到这里,不会误报。
			failedStage = notifyStageApply
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
	//
	// 2026-06-01 Round 14 §P6: after Round 14 P0 unblocked EnterScene the
	// EnterGame throughput jumped +15% and pushed the DB consumer→dispatch
	// chain past its critical point; cb_wait{ok} avg climbed 85ms → 275ms
	// and ~4% of preloads hit the 5s TTL with cb_wait{fail}=5.5s exactly.
	// Round 15 short-term stop-loss: bump TTL to 8s so transient queueing
	// inside db_rpc + task_result_dispatcher does NOT cascade into preload
	// failures. Real fix lives in P5 (db service sub-shard parallelism)
	// and P7 (dispatcher worker pool); 8s is just the absorbing buffer
	// while those land. Revisit downward once cb_wait{ok} drops back to
	// <100ms steady-state.
	const dispatcherTaskTTL = 8 * time.Second
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
		// 只记计数、不进 total_seconds 直方图:链没有启动,没有耗时可记(见 metrics.go 的 ResultPoolSaturated)。
		// 这是池掉单唯一进 Prometheus 的信号;svc.SubmitPreload 的进程内计数只进 [preload-pool] 日志。
		totalCounter.Inc(ResultPoolSaturated)
		chainCancel()
		stopHeartbeat()
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

// enterFailureTipPusher 是 notifyEnterGameFailed 唯一需要的能力:经 gate 给某个会话推一条 tip。
// 生产实现是 *svc.ServiceContext(PushTipToSession)。只收这一个方法而不是整个 ServiceContext,
// 是因为其 KafkaClient 是具体类型、单测里起不出来;与 resolveEnterName 收 roleNameLookup 同做法。
type enterFailureTipPusher interface {
	PushTipToSession(gateID string, gateInstanceID string, sessionID uint32, playerID uint64, tipID uint32) error
}

// replacedSessionKicker 是 ReplaceLogin 踢旧会话唯一需要的能力:经 gate 踢掉被替换的会话。
// 生产实现是 *svc.ServiceContext(KickSessionOnGate),做法同 enterFailureTipPusher:KafkaClient 是具体类型,
// 单测里起不出来,收成一个方法的接口才测得到「只在提交之后踢」(docs/design/scene-enter-chain-residuals.md §2.3)。
// playerID 与 loginGeneration 随 KickPlayerEvent payload 下发,供 gate 的 ClassifyKick 核对。
type replacedSessionKicker interface {
	KickSessionOnGate(gateID string, gateInstanceID string, sessionID uint32, playerID uint64, loginGeneration uint64) error
}

// replacedKicker 返回本次踢线用的 kicker:测试注入的优先,否则用生产的 l.svcCtx。
func (l *EnterGameLogic) replacedKicker() replacedSessionKicker {
	if l.kicker != nil {
		return l.kicker
	}
	return l.svcCtx
}

// notifyEnterGameFailed 是 EnterGame 异步链**唯一**的面向客户端的失败出口
// (只覆盖链上显式返回的失败;panic 路径不经过这里,见 onPreloadComplete 里 failedStage 的注释)。
//
// 契约(cross-zone-scene-travel.md §12.5.2 / §12.5.4 CL-1):EnterGame 的 gRPC 应答无错 = 已受理;
// 之后进场没成,服务端经 gate 推 SendTipToClient,tip 固定为 scene_error 的 kEnterSceneFailed。
// 所有失败(预加载失败、apply 失败含 EnterScene 被拒)共用这一个码:客户端要做的事只有一件 ——
// 别再等 NotifyEnterScene 了;具体原因(scene_manager 的拒绝码等)只进服务端日志,不外发。
// 这个洞不是跨 zone 传送独有,普通登录同样如此;传送只是因为源实体此刻已销毁而后果更重。
//
// 这是 best-effort 的**通知**,不是状态变更:
//   - 不清登录会话、不踢线、不动玩家锁,既有处置一概不变。链失败后保留登录会话是刻意的(供客户端
//     在同一连接上重试);踢线会逼客户端回选服走全新登录,而带着旧 location 的全新登录多半又被
//     换手门以 18 拒绝,比留在原连接上更糟。实情:当前 Unity 客户端收到该 tip 后直接拆连接
//     回选服,同连接重试只对 robot / 将来的客户端实现有意义(见 §12.5.4 CL-1);本函数按
//     "通知不改处置"不去动这份保留。
//   - 推送失败只记 ERROR + 计数,不向上传播、不重试:目标会话可能已经断开,重发没有意义;
//     客户端自己的 60s 超时仍是兜底。
//   - 已知的重复通知:EnterScene RPC 超时但 scene_manager 其实已放行时,客户端会先后收到
//     NotifyEnterScene 与这条 tip。客户端须按"已进场则忽略进场失败 tip"处理。
//
// state 里的 gate 寻址三元组在进入异步链之前就从 SessionDetails 按值抄好了(buildEnterGameSessionState),
// 所以预加载失败这种还没走到 apply 的分支同样拿得到。拿不到(dev 旁路 / 旧版 gate 不透传
// gate_instance_id,或 gate_node_id 没填 = "0")就不推:硬造寻址只会被 buildGateCommandMessage 的
// fail-closed 守卫拒掉,这里提前分流只是为了让计数区分"无处可推"与"推了但失败"。
//
// stage 只用于日志与计数 label(notifyStagePreload / notifyStageApply)。
func notifyEnterGameFailed(pusher enterFailureTipPusher, state enterGameSessionState, stage string) {
	if state.gateInstanceID == "" || state.gateID == "" || state.gateID == "0" {
		failureNotifyCounter.Inc(stage, notifyOutcomeSkippedNoGate)
		logx.Errorf("EnterGame failure tip not sent: session carries no gate address [PlayerId=%d session=%d stage=%s gate=%q]; client falls back to its own enter timeout",
			state.playerID, state.sessionID, stage, state.gateID)
		return
	}

	tipID := uint32(table.SceneError_kEnterSceneFailed)
	if err := pusher.PushTipToSession(state.gateID, state.gateInstanceID, state.sessionID, state.playerID, tipID); err != nil {
		failureNotifyCounter.Inc(stage, notifyOutcomeFailed)
		logx.Errorf("EnterGame failure tip push failed [PlayerId=%d session=%d stage=%s gate=%s tip=%d]: %v; client falls back to its own enter timeout",
			state.playerID, state.sessionID, stage, state.gateID, tipID, err)
		return
	}
	failureNotifyCounter.Inc(stage, notifyOutcomeSent)
	logx.Infof("EnterGame failure tip sent [PlayerId=%d session=%d stage=%s gate=%s tip=%d]",
		state.playerID, state.sessionID, stage, state.gateID, tipID)
}

// consumePostMergeFlags reads the two Redis flag keys written by
// tools/merge_zone/post_merge_stamp.go and:
//   - populates resp.PostMergeNoticeTs / resp.ForceRenameRequired
//   - DELetes the keys so subsequent logins don't re-fire the UI
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

		ticketTargetZoneID: sessionDetails.GetTicketTargetZoneId(),
	}
}

// resolveEnterName 决定本次入场要补进 PlayerProfileComp 的角色名(见 backfillPlayerIdentity)。
//
// accountName 来自已验证归属的账号角色记录:非空直接用,正常角色因此不多一次 RPC。
// 为空才回源名字注册表(data_service BatchGetPlayerName)一次,预算 DefaultLookupTimeout。
// 名字是展示数据,读侧 fail-open:查询失败(含注册表未配置的 ErrUnavailable)只记 Info
// 并返回 "",入场照常进行;本次不补名字,下一次无会话入场会再试。
//
// names 为 nil 接口时直接返回 "";l.svcCtx.PlayerNames 是 *playernamereg.Client,nil 指针
// 装进接口后**不等于** nil 接口,那种情况由 Client.Lookup 的 nil 接收者分支返回
// ErrUnavailable 兜住,走下面的错误分支。
func resolveEnterName(ctx context.Context, accountName string, names roleNameLookup, id uint64) string {
	if accountName != "" {
		return accountName
	}
	if names == nil {
		return ""
	}
	lookupCtx, cancel := context.WithTimeout(ctx, playernamereg.DefaultLookupTimeout)
	defer cancel()
	found, err := names.Lookup(lookupCtx, []uint64{id})
	if err != nil {
		logx.WithContext(ctx).Infof("[role-name] EnterGame name backfill skipped: player=%d failed to query name registry: %v", id, err)
		return ""
	}
	return found[id]
}

// homeZoneOverrideAllowed 决定这次 EnterGame 能不能把 ZoneId 换成归属 zone。
//
// 只有「首次登录且 player_locator 里没有在场 scene」才允许。理由:重连(ShortReconnect)
// 与顶号(ReplaceLogin)都带着旧 zone 的 player:{id}:location,而 scene_manager 对「有定位且
// 要离开所在 zone」的请求要过换手门:源 scene 没为当前 owner_epoch 写出「已落盘」标记之前一律回
// 可重试的 18 ErrHandoffPending(历史上这里回的是 14 ErrUnsafeCrossNodeHandoff,该码已不再发出,
// 见 scene_manager constants/errors.go)。18 要等源 scene 落盘写标记才会放行,而登录请求自己
// 催不动这件事 —— 强行覆盖只会把本来能回到原 scene 的玩家变成撞 18 进不去。
// 重连 / 顶号的落点由 resolveEnterSceneRoute 交给 scene_manager 按 location 决定(GO-5)。
// 合服后的引导由角色列表的 zone 刷新负责,玩家下一次从目标 zone 登录即可。
func homeZoneOverrideAllowed(decision sessionmanager.EnterGameDecision, existing *sessionmanager.PlayerSession) bool {
	if decision != sessionmanager.FirstLogin {
		return false
	}
	return existing == nil || existing.SceneID == 0
}

// enterSceneRoute 是 EnterGame 发给 scene_manager 的落点意图,即 EnterSceneRequest 的
// ZoneId / SceneId。GateZoneId 不在这里:它恒为 login 自己的 zone,是「连接在哪」而不是「去哪」。
type enterSceneRoute struct {
	// zoneID 为 0 = 本次不指定去向,由 scene_manager 按它自己的 player:{id}:location 决定。
	zoneID  uint32
	sceneID uint64
}

// resolveEnterSceneRoute 决定 EnterScene 请求的去向。优先级自上而下,命中即止:
//
//  1. 持重定向票据且票据 target_zone_id == 本 zone(跨 zone 传送 / 重定向的第二条腿,CZ-8)
//     → ZoneId = 本 zone,SceneId 清零。**必须排在第 2 条之前**:第二条腿的登录发生在客户端
//     刚从源 gate 断开、30s 内连上目标 gate 的时候,源 gate 断线已把会话置成 DISCONNECTING,
//     decision 多半就是 ShortReconnect;若先按第 2 条发 ZoneId=0,就绕过了「票据指明来本 zone」
//     这层访客识别,落点改由 location 决定。
//  2. decision ∈ {ShortReconnect, ReplaceLogin} → ZoneId = 0(GO-5 的决定,§12.3 / §12.6):
//     访客(归属 A、正在访问 B)短暂断线后重登,**在重连窗口内回到原处(B),超出窗口或主动登出回家(A)**;
//     顶号则是新设备接管角色当前所在的位置。
//     「窗口」就是服务端现成的断线租约,不另造:gate 断线时 login 以显式的 LeaseTtlSeconds=30
//     把会话置成 DISCONNECTING(sessionmanager.SetSessionDisconnecting);租约内同账号重登被
//     DecideEnterGame / CanReconnect 判成 ShortReconnect,别的在线会话判成 ReplaceLogin。租约到期
//     (挂机月卡会顺延,见 player_locator leasemonitor.go)由 player_locator 调 LeaveScene 删掉
//     location,之后的重登是 FirstLogin → 走第 3 / 4 条;主动登出(LeaveGame → MarkOffline)同理。
//     去向由服务端决定、不靠客户端记住「上次在哪个区」:客户端崩溃重开、换设备都会把这份记忆弄丢,
//     而 location 是归属的唯一权威。scene_manager 在 ZoneId==0 时只跟随**落在具体节点上**的 location
//     (node_id 非空);「等待落点」(跨 zone 传送途中、node_id 为空)不牵引、按 gate zone 落点,
//     避免第二条腿持续失败时每次登录都被来回重定向。location 在别的 zone 时 scene_manager 回
//     「只送连接」的重定向(Redirect 非空、ErrorCode 0),本链走成功出口,不推 3023。
//     SceneId 清零:scene_manager 在 ZoneId==0 且 SceneId!=0 时会先按 GetSceneZone(SceneId) 定目标 zone
//     (enterscenelogic.go「1. Determine the target zone」),绕过上面的 node_id 规则。会话里的 SceneID
//     今天恒为 0(player_locator leasemonitor.go resolveSceneManagerSceneID 的注释),但去向只能由
//     location 这一处权威决定,不给日后写进会话的陈旧 scene_id 留口子。
//  3. FirstLogin 且 RedirectOnEnter 命中(开关打开 + homeZoneOverrideAllowed + 归属 zone ≠ 本 zone)
//     → ZoneId = 归属 zone,SceneId 清零(合服后仍从源 zone 进来的玩家送去目标 zone)。
//  4. 其余 → ZoneId = 本 zone,SceneId = 0。能走到这里的只有 FirstLogin(DecideEnterGame 在
//     existing==nil 时才判 FirstLogin),没有会话可取 SceneID;同 zone 重连已由第 2 条接走。
//
// homeZones 为 nil 接口时第 3 条按「查不到」处理;*homezone.Resolver 的 nil 指针由其自身兜成
// ErrUnavailable,同样维持本 zone。
func resolveEnterSceneRoute(
	ctx context.Context,
	state enterGameSessionState,
	decision sessionmanager.EnterGameDecision,
	existing *sessionmanager.PlayerSession,
	ownZone uint32,
	redirectOnEnterEnabled bool,
	homeZones homezone.Lookup,
) enterSceneRoute {
	if homezone.TicketPinsZone(state.ticketTargetZoneID, ownZone) {
		// CZ-8:票据指明就是来本 zone 的(跨 zone 传送的第二条腿)。不查 home_zone、不弹回 ——
		// 否则打开 RedirectOnEnter 的目标 zone 会把访客再送回家,与客户端 RedirectFlow 的
		// 3 跳熔断互撞。existing.SceneID 若有也属于源 zone,清零让 scene_manager 在本 zone
		// 落点(它会读「等待落点」里记的目标地图;resolveScene 对跨 zone 的 scene 会直接拒)。
		logx.Infof("[travel] EnterGame player=%d arrived in zone=%d with redirect ticket; skipping home_zone redirect", state.playerID, ownZone)
		return enterSceneRoute{zoneID: ownZone, sceneID: 0}
	}

	if decision == sessionmanager.ShortReconnect || decision == sessionmanager.ReplaceLogin {
		logx.Infof("[travel] EnterGame player=%d decision=%s does not specify destination (ZoneId=0); scene_manager decides from location gate_zone=%d",
			state.playerID, decisionLabel(decision), ownZone)
		return enterSceneRoute{zoneID: 0, sceneID: 0}
	}

	if redirectOnEnterEnabled && homeZoneOverrideAllowed(decision, existing) {
		if home, redirected := homezone.ResolveEnterZone(ctx, homeZones, state.playerID, ownZone); redirected {
			// player_locator 里的 existing.SceneID 属于旧 zone,对目标 zone 没有意义,清零。
			// 现状(enterscenelogic.go「CROSS-ZONE CHECK」):scene_manager 的重定向判定**先于**
			// 场景解析,重定向分支既不读 SceneId 也不做任何预占,目标场景由第二条腿上目标 zone
			// 自己的 EnterScene 解析。早先是「先解析再重定向」,带着旧 zone 的 scene 会被
			// resolveScene 以「scene 属于 zone X、请求 zone Y」拒掉 —— 清零最初是为那个顺序加的,
			// 现在保留只是不把一个跨 zone 的 scene_id 递出去。
			return enterSceneRoute{zoneID: home, sceneID: 0}
		}
	}

	return enterSceneRoute{zoneID: ownZone, sceneID: 0}
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
	if err := l.backfillPlayerIdentity(ctx, existing, state); err != nil {
		return decision, err
	}

	// 登录代次:持锁、读到会话记录之后,写会话与踢线之前铸一次(scene-enter-session-fence.md §3.2 / §3.3)。
	// 踢线排在 SetSession 提交之后(persistEnterGameSession,scene-enter-chain-residuals.md §2.2 ①)。
	// 失败即链失败:带 0 继续会让 scene 按"旧版无代次"处理,已有代次的实体会拒掉这次合法登录。
	loginGeneration, err := mintEnterLoginGeneration(ctx, l.svcCtx.RedisClient, state.playerID, existing, time.Now())
	if err != nil {
		return decision, err
	}

	// Stage observation: persistEnterGameSession (SetSession or Reconnect).
	// Label by decision so the reconnect (CAS) tail doesn't get hidden by the
	// first/replace (write) tail.
	persistTimer := observeStage(applyPersistSessionSeconds, decisionLabel(decision))
	version, err := l.persistEnterGameSession(ctx, decision, existing, state, loginGeneration)
	persistTimer()
	if err != nil {
		return decision, err
	}

	// gate 连接会话栅栏的绑定点(docs/design/client-request-ordering.md §6.2):会话已写入、还没通知 gate 绑定。
	// 与 Disconnect 的 sessionfence.Close 构成双边握手 —— 先关闭的,这里看到 closed;先绑定的,Close 读到玩家。
	// 两种顺序都收敛到「带租约的 DISCONNECTING」,玩家不会因为「受理之后、绑定之前断线」永久卡在 ONLINE。
	if err := l.bindSessionFence(ctx, state); err != nil {
		return decision, err
	}

	// Notify Gate: bind the session and carry the login decision so Gate can forward to Scene.
	// 代次随 Bind 一起到 gate:Bind 先于本次路由到达时,gate 按旧路由补发也要带本次的代次(§4)。
	enterGsType := sessionmanager.DecisionToEnterGsType(decision)
	bindTimer := observeStage(applyBindGateSeconds)
	bindErr := l.svcCtx.SendBindSessionToGate(
		state.gateID, state.gateInstanceID,
		state.sessionID, state.playerID, version, enterGsType, loginGeneration,
	)
	bindTimer()
	if bindErr != nil {
		logx.Errorf("Failed to send BindSessionToGate for player %d: %v", state.playerID, bindErr)
		return decision, bindErr
	}

	// Route player to Scene node via SceneManager.
	// 去向(ZoneId / SceneId)的优先级见 resolveEnterSceneRoute;GateZoneId 保持 login 自己的 zone。
	// 两者不同时 scene_manager 的跨区重定向(enterscenelogic.go crossZoneRedirect)才会触发;
	// ZoneId=0(重连 / 顶号)时由 scene_manager 按 location 决定,location 在别的 zone 同样触发重定向。
	// 映射不可用 / 为 0 / 开关关闭 → 维持本 zone,不阻断进游戏。
	ownZone := config.AppConfig.Node.ZoneId
	route := resolveEnterSceneRoute(ctx, state, decision, existing, ownZone,
		config.AppConfig.HomeZone.RedirectOnEnterEnabled, l.svcCtx.HomeZone)
	smTimeout := time.Duration(config.AppConfig.SceneManagerRpc.Timeout) * time.Millisecond
	// 进场前复查栅栏:绑定之后连接断开了(Disconnect 已读到绑定的玩家、自己挂了断线租约),或离开了游戏
	// (LeaveGame 已删会话),就不再进场,免得在 DISCONNECTING / 已离线之后又写入位置与实例人数。
	if state.fenced {
		outcome, fenceErr := sessionfence.Recheck(ctx, l.svcCtx.RedisClient, state.fenceKey, state.fenceGen)
		if fenceErr != nil {
			return decision, fmt.Errorf("session fence recheck before EnterScene for player %d: %w", state.playerID, fenceErr)
		}
		if outcome != sessionfence.OutcomeOK {
			return decision, fmt.Errorf("session fence %s before EnterScene for player %d session %d; not entering scene",
				outcome, state.playerID, state.sessionID)
		}
	}
	// Stage observation: SceneManager.EnterScene RPC. This is the prime
	// suspect under load — it routes through SceneManager → Scene
	// AssignScene + Kafka. If err25 root cause is "async chain stalls",
	// this histogram is where the long tail will appear.
	enterSceneTimer := observeStage(applyEnterSceneSeconds)
	enterResp, err := l.svcCtx.SceneManagerClient.EnterScene(ctx,
		buildEnterSceneRequest(state, route, ownZone, loginGeneration),
		zrpc.WithCallTimeout(smTimeout))
	enterSceneTimer()
	if err != nil {
		logx.Errorf("SceneManager.EnterScene failed for player %d: %v", state.playerID, err)
		return decision, err
	}
	if enterResp.ErrorCode != 0 {
		// 必须当失败返回:调用方(EnterGame 的异步链)只有在 applyErr != nil 时才会
		// **跳过** cleanupLoginSessionState,把登录会话留给客户端重试。以前这里只记
		// 一条 ERROR 就继续走 SetIdempotency + 清会话,于是 scene_manager 拒绝
		// (ErrNoAvailableNode / 可重试的 18 ErrHandoffPending / 20 ErrHomeZoneUnavailable …;
		// 历史上的 14 ErrUnsafeCrossNodeHandoff 已不再发出)之后客户端既等不到
		// RoutePlayer,重试又撞 kLoginSessionNotFound —— 玩家卡死且日志之外无迹可寻。
		//
		// 现在的契约(见 notifyEnterGameFailed):返回 error → 异步链在释放玩家锁之后经 gate
		// 给客户端推 kEnterSceneFailed,客户端不必再干等 NotifyEnterScene 的超时。具体拒绝码
		// 只进这条 error 的日志,不外发。登录会话照旧保留、**不踢线**:被拒的多数是可重试码,
		// 踢线会逼客户端回选服走全新登录,而带着旧 location 的全新登录多半又是 18。
		return decision, fmt.Errorf("scene_manager rejected EnterScene for player %d: code=%d msg=%s",
			state.playerID, enterResp.ErrorCode, enterResp.ErrorMessage)
	}

	// Cross-zone redirect: SceneManager already pushed RedirectToGateEvent to the gate.
	// Skip idempotency — the player hasn't entered a scene yet; they'll re-login in the new zone.
	// 重连 / 顶号发 ZoneId=0 时,location 在别的 zone 也走这里(「只送连接」的重定向,ErrorCode 0):
	// 返回 nil = 链的成功出口,不推 3023(见 notifyEnterGameFailed)。
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

// bindSessionFence 在会话写入之后、通知 gate 绑定之前,把本连接登记为「绑定了 state.playerID」
// (docs/design/client-request-ordering.md §6.2)。旧版 gate(没有会话代次)不做。
// 绑定失败时由本函数补偿,并返回 error 让后台链按失败收尾(不发 BindSession、不进场):
//   - 连接已断开:Disconnect 先到时这条连接还没绑定玩家(它带的 player_id 为 0),没能挂断线租约 → 这里补挂;
//   - 期间离开了游戏 / 连接上已绑定别的角色:删掉本次写入的会话(MarkOffline 按 session_id + version CAS);
//   - 绑定本身报错(结果未知):同「连接已断开」补挂断线租约 —— 链不会再发 BindSession,gate 之后的断线通知
//     不带这个玩家,脚本若没执行 Close 也读不到它,不补挂就会永久停在 ONLINE。连接还活着时客户端重试 EnterGame,
//     走短线重连接回;租约到期之前不会误清。
//
// 补偿用 WithoutCancel 派生的独立 ctx:走到这里时链 ctx 可能已被心跳丢锁取消(chainCancel)或临近链预算截止,
// markPlayerSessionDisconnecting 在 ctx 已取消时直接放弃,只留一条 CRITICAL 日志(与 CreatePlayer 补偿同约定)。
func (l *EnterGameLogic) bindSessionFence(ctx context.Context, state enterGameSessionState) error {
	if !state.fenced {
		return nil
	}
	outcome, err := sessionfence.BindPlayer(ctx, l.svcCtx.RedisClient, state.fenceKey, state.playerID, state.fenceGen, sessionFenceTTL)
	if err != nil {
		l.compensateFenceBind(ctx, state, true)
		return fmt.Errorf("session fence bind for player %d: %w; disconnect lease placed by the enter chain", state.playerID, err)
	}
	switch outcome {
	case sessionfence.OutcomeOK:
		return nil
	case sessionfence.OutcomeClosed:
		l.compensateFenceBind(ctx, state, true)
		return fmt.Errorf("gate connection closed before binding player %d session %d; disconnect lease placed by the enter chain",
			state.playerID, state.sessionID)
	default:
		l.compensateFenceBind(ctx, state, false)
		return fmt.Errorf("session fence %s when binding player %d session %d; session record removed",
			outcome, state.playerID, state.sessionID)
	}
}

// fenceBindCompensationTimeout 覆盖 markPlayerSessionDisconnecting 的 4 次尝试(退避合计 1.4s)加每次 RPC。
const fenceBindCompensationTimeout = 10 * time.Second

// compensateFenceBind 撤销绑定失败时已写入的会话:placeLease = true 挂断线租约,否则删会话。
// ReplaceLogin 的 SetSession 报错、复读却确认已提交时也用它补挂断线租约(placeLease = true,
// scene-enter-chain-residuals.md §2.2 ②)。两处都带本链的 gate 实例号:本链写下的会话记录就是这个实例号,
// player_locator 的实例号校验因此放行(§6.2)。
func (l *EnterGameLogic) compensateFenceBind(ctx context.Context, state enterGameSessionState, placeLease bool) {
	cctx, cancel := context.WithTimeout(context.WithoutCancel(ctx), fenceBindCompensationTimeout)
	defer cancel()
	if placeLease {
		markPlayerSessionDisconnecting(cctx, l.svcCtx, state.playerID, state.sessionID, state.gateInstanceID)
		return
	}
	deletePlayerSession(cctx, l.svcCtx, state.playerID, state.sessionID, state.gateInstanceID)
}

// persistEnterGameSession persists the session and returns the new session version.
//
// loginGeneration 是本链铸出的登录代次,随 SetSession / Reconnect 的整字节 CAS 一起提交;
// player_locator 在记录与请求都非 0 时要求严格递增,失锁的并发登录链只有一条能提交(§3.4)。
//
// ReplaceLogin 的踢线排在 SetSession 提交**之后**、bindSessionFence 之前(scene-enter-chain-residuals.md §2.2 ①):
// 先踢后提交时,提交失败(失锁、ctx 取消、版本冲突)的链也会把当前合法会话踢掉。不能再晚:等 EnterScene 成功
// 再踢的话,EnterScene 被可重试码拒绝时记录已指向本链,旧设备却还在 scene 上照常游戏。
// SetSession 报错时结果未知,用独立短超时复读一次消歧(settleReplaceLoginAfterSetSessionError)。
func (l *EnterGameLogic) persistEnterGameSession(
	ctx context.Context,
	decision sessionmanager.EnterGameDecision,
	existing *sessionmanager.PlayerSession,
	state enterGameSessionState,
	loginGeneration uint64,
) (uint32, error) {
	switch decision {
	case sessionmanager.FirstLogin, sessionmanager.ReplaceLogin:
		newSession := sessionmanager.NewOnlineSession(existing, sessionmanager.OnlineSessionInput{
			PlayerID:        state.playerID,
			SessionID:       state.sessionID,
			GateID:          state.gateID,
			GateInstanceID:  state.gateInstanceID,
			Account:         state.account,
			RequestID:       state.requestID,
			Now:             time.Now(),
			LoginGeneration: loginGeneration,
		})
		if err := sessionmanager.SetSession(ctx, l.svcCtx.PlayerLocatorClient, newSession); err != nil {
			if decision == sessionmanager.ReplaceLogin {
				l.settleReplaceLoginAfterSetSessionError(ctx, existing, state, loginGeneration, err)
			}
			// 复读确认已提交时链也按失败收尾:bindSessionFence 还没执行,不能接着发 Bind / 进场。
			return 0, err
		}
		if decision == sessionmanager.ReplaceLogin {
			l.kickReplacedSessionAfterCommit(existing, state, loginGeneration)
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
			loginGeneration,
		)
		if err != nil {
			return 0, err
		}
		// 应答回带的是会话记录。旧版 locator 不认请求里的代次,会原样回出上一条会话的值;下游(Bind、
		// EnterScene)一律用本链铸出的值,这里只留痕。go-zero logx 没有 WARN 级别,按仓库惯例取 ERROR
		// 并带 WARN 前缀:升级顺序是 locator 先于 login,稳态下出现即说明混版或 locator 没写进去。
		if updated.LoginGeneration != loginGeneration {
			logx.Errorf("[login-generation] WARN reconnect reply generation mismatch: player=%d session=%d minted=%d reply=%d; downstream uses the minted value",
				state.playerID, state.sessionID, loginGeneration, updated.LoginGeneration)
		}
		return updated.SessionVersion, nil
	default:
		return 0, nil
	}
}

// mintEnterLoginGeneration 为本条 EnterGame 链铸登录代次:下限取锁内读到的会话记录代次
// (没有记录为 0),now 由调用方给出(生产 time.Now(),测试可注入)。
// 错误不在这里打日志:调用方作为链失败返回,由 onPreloadComplete 统一记 ERROR 并推 3023。
func mintEnterLoginGeneration(ctx context.Context, rdb *redis.Client, playerID uint64,
	existing *sessionmanager.PlayerSession, now time.Time,
) (uint64, error) {
	var sessionFloor uint64
	if existing != nil {
		sessionFloor = existing.LoginGeneration
	}
	return sessionmanager.MintLoginGeneration(ctx, rdb, playerID, sessionFloor, now)
}

// buildEnterSceneRequest 组装发给 scene_manager 的 EnterSceneRequest。纯函数,便于单测直接断言字段。
//
// 去向(ZoneId / SceneId)见 resolveEnterSceneRoute;GateZoneId 恒为 login 自己的 zone。
// LoginGeneration 是本链铸出的代次,scene_manager 原样透传进 RoutePlayerEvent(不进 request_id 指纹),
// gate 据此给 PlayerEnterGameNode 盖章,scene 入场栅栏据此拒绝跨会话的陈旧入场。
// CorrelationId 留 0:login 不按请求对应答(见 proto 注释)。
func buildEnterSceneRequest(state enterGameSessionState, route enterSceneRoute, ownZone uint32, loginGeneration uint64) *smpb.EnterSceneRequest {
	return &smpb.EnterSceneRequest{
		PlayerId:        state.playerID,
		SceneId:         route.sceneID,
		SessionId:       state.sessionID,
		RequestId:       state.requestID,
		GateId:          state.gateID,
		GateInstanceId:  state.gateInstanceID,
		GateZoneId:      ownZone,
		ZoneId:          route.zoneID,
		LoginGeneration: loginGeneration,
	}
}

// setSessionProbeTimeout 是 SetSession 结果未知时复读会话记录的预算。复读只为消歧,不重试:
// 读不到就按未提交处理(不踢、不挂租约),宁可旧设备多在线一会儿,也不在没提交时踢掉合法会话。
const setSessionProbeTimeout = 2 * time.Second

// kickReplacedSessionAfterCommit 在本链的会话记录确认已提交之后踢掉被替换的旧会话,并按结局计数。
// 踢线写 Kafka 失败只记 ERROR 并计数,不让链失败:提交已经发生,旧设备会因 scene 栅栏改绑而变哑(§2.2 ①)。
func (l *EnterGameLogic) kickReplacedSessionAfterCommit(existing *sessionmanager.PlayerSession, state enterGameSessionState, loginGeneration uint64) {
	kickTotal.Inc(kickReplacedSession(l.replacedKicker(), existing, state, loginGeneration))
}

// settleReplaceLoginAfterSetSessionError 处理 ReplaceLogin 的 SetSession 报错(应答丢失、链 ctx 取消、locator 拒绝)。
//
// 报错不等于没提交:用独立短超时 ctx 复读一次(setSessionCommittedDespiteError)。
//   - 复读确认本链已提交:照常踢旧会话,并补挂断线租约。此刻 bindSessionFence 还没执行,链又按失败收尾,
//     当前 Unity 客户端收到 3023 就拆连接,断线时栅栏没有绑定玩家,Disconnect 收不到本玩家;被踢的旧连接的
//     SetDisconnecting 又因会话号不符被忽略。不补挂,记录会停在 ONLINE、没有租约(§2.2 ②)。连接若还活着、
//     客户端同连接重试,走短线重连接回,与绑定失败补偿同一语义。
//   - 复读失败或对不上:按未提交处理,不踢、不挂租约。
//
// 调用方照旧返回 SetSession 的原错误。
func (l *EnterGameLogic) settleReplaceLoginAfterSetSessionError(
	ctx context.Context,
	existing *sessionmanager.PlayerSession,
	state enterGameSessionState,
	loginGeneration uint64,
	setErr error,
) {
	if !l.setSessionCommittedDespiteError(ctx, state, loginGeneration) {
		kickTotal.Inc(kickResultSkipUncommitted)
		logx.Infof("[login-kick] SetSession failed and probe did not confirm commit: player=%d session=%d generation=%d err=%v; not kicking replaced session",
			state.playerID, state.sessionID, loginGeneration, setErr)
		return
	}
	logx.Errorf("[login-kick] WARN SetSession outcome unknown, probe confirms commit: player=%d session=%d generation=%d err=%v; kicking replaced session and placing disconnect lease",
		state.playerID, state.sessionID, loginGeneration, setErr)
	l.kickReplacedSessionAfterCommit(existing, state, loginGeneration)
	// 等价于绑定失败的补偿:独立于链 ctx(WithoutCancel)、预算覆盖 markPlayerSessionDisconnecting 的退避重试,
	// 带本链的 gate 实例号(e:与本链写下的记录同一个实例号)。
	l.compensateFenceBind(ctx, state, true)
}

// setSessionCommittedDespiteError 在 SetSession 报错后复读一次会话记录,判断本链其实是否已经提交。
//
// ctx 从链 ctx 派生但去掉取消(WithoutCancel)、另设 setSessionProbeTimeout 截止:报错的常见原因之一正是链 ctx
// 已被心跳丢锁取消(onLost),沿用它复读必然失败。做法同 compensateFenceBind。
func (l *EnterGameLogic) setSessionCommittedDespiteError(ctx context.Context, state enterGameSessionState, loginGeneration uint64) bool {
	pctx, cancel := context.WithTimeout(context.WithoutCancel(ctx), setSessionProbeTimeout)
	defer cancel()
	record, err := sessionmanager.GetSession(pctx, l.svcCtx.PlayerLocatorClient, state.playerID)
	if err != nil {
		logx.Errorf("[login-kick] SetSession outcome probe failed: player=%d session=%d generation=%d err=%v; treating as uncommitted",
			state.playerID, state.sessionID, loginGeneration, err)
		return false
	}
	return setSessionProbeConfirmsCommit(record, state.sessionID, loginGeneration)
}

// setSessionProbeConfirmsCommit:复读到的记录是本链写下的,当且仅当会话号与本链相同、代次等于本链铸出值且非 0。
// 非 0 代次与会话一一对应(fence G2),所以代次相等足以排除「同号的另一次登录」;代次为 0 无从区分,按未提交处理。
func setSessionProbeConfirmsCommit(record *sessionmanager.PlayerSession, sessionID uint32, loginGeneration uint64) bool {
	return record != nil && loginGeneration != 0 &&
		record.SessionID == sessionID && record.LoginGeneration == loginGeneration
}

// kickReplacedSession 决定要不要踢被替换的旧会话,要踢就发出去;返回 kickTotal 的 result 供调用方计数。
// 只能在本链的会话记录确认已提交之后调用。按顺序判(scene-enter-chain-residuals.md §2.2 ⑤):
//
//  1. 旧记录缺 gate 寻址(GateID 或 GateInstanceID 为空)→ skip_no_target:buildGateCommandMessage 守卫一本来就拒空实例号。
//  2. 本链实例号为空 → skip_no_target,无论会话号:这条链必在 Bind 的守卫一失败、3023 也推不出去,
//     不能先踢掉旧设备再失败。
//  3. 会话号相同、实例号相同 → skip_same_session:同一连接重登。本链的 Bind 在踢线之后才发,本连接的 gate 会话
//     仍绑着上一次的更小代次,ClassifyKick 会判 kKick,所以必须在这里拦住自踢。
//  4. 会话号相同、两边实例号都非空且不同(gate 以同一 node_id 重启,同号落到新实例)→ skip_cross_instance:
//     按 §12.3 定论只计数不踢。踢旧实例会让它关闭旧连接时立刻向 scene 发 ExitGame(同号同玩家),晚于新会话的
//     入场到达就会退掉新实体;方案 C(scene 按 (gate 实例, session_id) 认会话)落地后再打开。
//  5. 会话号不同 → 踢,结局 sent / send_failed。
//
// 被踢会话所属玩家取本链玩家:会话记录按玩家键存取,被替换的就是本玩家的会话。不用记录里的 PlayerID,
// 免得字段缺失(0)时退化成 gate 不核对玩家的旧踢线。代次取本链铸出、已提交的值,gate 据此不踢承载更新登录的会话。
func kickReplacedSession(kicker replacedSessionKicker, existing *sessionmanager.PlayerSession, state enterGameSessionState, loginGeneration uint64) string {
	if existing == nil || existing.GateID == "" || existing.GateInstanceID == "" {
		return kickResultSkipNoTarget
	}
	if state.gateInstanceID == "" {
		return kickResultSkipNoTarget
	}
	if existing.SessionID == state.sessionID {
		if existing.GateInstanceID == state.gateInstanceID {
			return kickResultSkipSameSession
		}
		logx.Infof("[login-kick] replaced session shares the session id on another gate instance, not kicking (counted only): player=%d session=%d old_gate=%s old_instance=%s new_instance=%s generation=%d",
			state.playerID, existing.SessionID, existing.GateID, existing.GateInstanceID, state.gateInstanceID, loginGeneration)
		return kickResultSkipCrossInstance
	}

	if err := kicker.KickSessionOnGate(existing.GateID, existing.GateInstanceID, existing.SessionID, state.playerID, loginGeneration); err != nil {
		logx.Errorf("[login-kick] kick after commit failed: player=%d old_session=%d old_gate=%s old_instance=%s generation=%d err=%v; old device stays connected until the scene fence rebinds",
			state.playerID, existing.SessionID, existing.GateID, existing.GateInstanceID, loginGeneration, err)
		return kickResultSendFailed
	}
	logx.Infof("[login-kick] kicked replaced session after commit: player=%d old_session=%d old_gate=%s old_instance=%s new_session=%d generation=%d",
		state.playerID, existing.SessionID, existing.GateID, existing.GateInstanceID, state.sessionID, loginGeneration)
	return kickResultSent
}
