package clientplayerloginlogic

import (
	"context"
	"google.golang.org/protobuf/proto"
	"login/data"
	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/dataloader"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsessionstore"
	"login/internal/logic/utils/sessioncleaner"
	"login/internal/svc"
	"login/pb/game"
	"strconv"
	"time"

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

	// 1. 获取 Session
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 2. 获取 LoginSessionInfo
	session, err := loginsessionstore.GetLoginSession(l.ctx, l.svcCtx.RedisClient, sessionDetails.SessionId)
	if err != nil {
		logx.Errorf("GetLoginSession failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginSessionNotFound)
		return resp, nil
	}

	// 3. 加锁防止同角色并发登录
	playerLocker := locker.NewRedisLocker(l.svcCtx.RedisClient)
	key := "player_locker:" + strconv.FormatUint(in.PlayerId, 10)
	tryLocker, err := playerLocker.TryLock(l.ctx, key, time.Duration(config.AppConfig.Locker.PlayerLockTTL)*time.Second)
	if err != nil {
		logx.Errorf("EnterGame lock acquire failed for playerId=%d: %v", in.PlayerId, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginInProgress)}
		return resp, nil
	}

	if !tryLocker.IsLocked() {
		logx.Errorf("EnterGame lock acquire failed for playerId=%d: %v", in.PlayerId, err)
		resp.ErrorMessage = &game.TipInfoMessage{Id: uint32(game.LoginError_kLoginInProgress)}
		return resp, nil
	}

	defer func() {
		ok, err := tryLocker.Release(l.ctx)
		if err != nil {
			logx.Errorf("Failed to release lock: %v", err)
		} else if !ok {
			logx.Infof("Lock was not held by us (possibly expired)")
		}
	}()

	// 4. 加载账号信息并验证角色归属
	accountKey := constants.GetAccountDataKey(session.Account)
	dataBytes, err := l.svcCtx.RedisClient.Get(l.ctx, accountKey).Bytes()
	if err != nil {
		logx.Errorf("RedisClient Get user account failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginAccountNotFound)
		return resp, nil
	}

	userAccount := &game.UserAccounts{}
	if err := proto.Unmarshal(dataBytes, userAccount); err != nil {
		logx.Errorf("Unmarshal user account failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginDataParseFailed)
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
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginEnterGameGuid)
		return resp, nil
	}

	// 5. FSM 状态管理（按 sessionId）
	sessionIdStr := strconv.FormatUint(sessionDetails.SessionId, 10)
	f := data.InitPlayerFSM()
	if err := fsmstore.LoadFSMState(l.ctx, l.svcCtx.RedisClient, f, sessionIdStr, ""); err != nil {
		logx.Errorf("FSM Load failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginFsmFailed)
		return resp, nil
	}

	if err := f.Event(context.Background(), data.EventEnterGame); err != nil {
		logx.Errorf("FSM Event failed: %v", err)
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginInProgress)
		return resp, nil
	}

	// 6. 加载 Player 数据（若不在 RedisClient）
	if err := l.ensurePlayerDataInRedis(in.PlayerId); err != nil {
		resp.ErrorMessage.Id = uint32(game.LoginError_kLoginPlayerGuidError)
		logx.Errorf("Load player data failed: %v", err)
		return resp, err
	}

	// 7. 通知中心服
	l.notifyCentreService(in)

	// 8. 清理 Session 和 FSM
	_ = sessioncleaner.CleanupSession(
		l.ctx,
		l.svcCtx.RedisClient,
		sessionDetails.SessionId,
		"enterGame",
	)

	resp.PlayerId = in.PlayerId
	return resp, nil
}

func (l *EnterGameLogic) ensurePlayerDataInRedis(playerId uint64) error {
	msgCentre := &game.PlayerCentreDatabase{PlayerId: playerId}
	err := dataloader.BatchLoadAndCache(
		l.ctx,
		l.svcCtx.RedisClient,
		l.svcCtx.AsynqClient,
		playerId,
		[]proto.Message{
			msgCentre,
		},
		l.svcCtx.TaskManager,
		l.svcCtx.TaskExecutor)

	if err != nil {
		logx.Errorf("BatchLoadAndCache error: %v", err)
		return err
	}

	playerAll := &game.PlayerAllData{}

	err = dataloader.LoadAggregateData(
		l.ctx,
		l.svcCtx.RedisClient,
		l.svcCtx.AsynqClient,
		playerId,
		playerAll,
		func(id uint64) []proto.Message {
			return []proto.Message{
				&game.PlayerDatabase{PlayerId: id},
				&game.PlayerDatabase_1{PlayerId: id},
			}
		},
		func(id uint64) string {
			return string(playerAll.ProtoReflect().Descriptor().FullName()) + ":" + strconv.FormatUint(id, 10)
		},
		l.svcCtx.TaskManager,
		l.svcCtx.TaskExecutor)

	return err
}

func (l *EnterGameLogic) notifyCentreService(in *game.EnterGameRequest) {
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok {
		logx.Error("Session not found in context during notify centre")
		return
	}

	req := &game.CentrePlayerGameNodeEntryRequest{
		ClientMsgBody: &game.CentreEnterGameRequest{
			PlayerId: in.PlayerId,
		},
		SessionInfo: sessionDetails,
	}

	node := l.svcCtx.GetCentreClient()
	if node != nil {
		node.Send(req, game.CentreLoginNodeEnterGameMessageId)
	}
}
