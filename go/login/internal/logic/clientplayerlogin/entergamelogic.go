package clientplayerloginlogic

import (
	"context"
	"errors"
	"fmt"
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
	"sync"
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

	// 7. 清理 Session 和 FSM
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
	// 获取会话信息
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok {
		logx.Error("Session not found in context during notify centre")
		return errors.New("session not found")
	}

	// 配置：需要加载的任务总数
	const totalTasksToLoad = 2
	// 状态跟踪：已完成的任务数、是否全部成功、并发安全控制
	var (
		completedTasks int        // 已完成的任务数量（替代 automatic）
		mu             sync.Mutex // 保护计数器的互斥锁
		notifyOnce     sync.Once  // 确保通知只执行一次
	)

	// 任务完成回调：累加计数并检查是否所有任务都完成
	callback := func(taskKey string, taskSuccess bool, err error) {
		mu.Lock()
		defer mu.Unlock()

		// 累加已完成任务数（无论成功失败都计数）
		// 只要有一个任务失败，整体标记为失败
		if !taskSuccess {
			completedTasks++
		}

		// 所有任务都完成后，触发通知（只执行一次）
		if completedTasks >= totalTasksToLoad {
			notifyOnce.Do(func() {
				// 所有任务成功，通知中心
				req := &game.CentrePlayerGameNodeEntryRequest{
					ClientMsgBody: &game.CentreEnterGameRequest{
						PlayerId: playerId,
					},
					SessionInfo: sessionDetails,
				}

				node := l.svcCtx.GetCentreClient()
				if node != nil {
					node.Send(req, game.CentreLoginNodeEnterGameMessageId)
				}
				logx.Infof("All %d tasks completed successfully, notified centre", totalTasksToLoad)
			})
		}
	}

	// 第一个任务：加载中心数据库数据
	msgCentre := &game.PlayerCentreDatabase{PlayerId: playerId}
	if err := dataloader.BatchLoadAndCache(
		l.ctx,
		l.svcCtx.RedisClient,
		l.svcCtx.AsynqClient,
		playerId,
		[]proto.Message{msgCentre},
		l.svcCtx.TaskExecutor,
		callback,
	); err != nil {
		logx.Errorf("Failed to start BatchLoadAndCache: %v", err)
		return err
	}

	// 第二个任务：加载聚合数据
	playerAll := &game.PlayerAllData{}
	if err := dataloader.LoadAggregateData(
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
			return fmt.Sprintf("%s:%d", playerAll.ProtoReflect().Descriptor().FullName(), id)
		},
		l.svcCtx.TaskExecutor,
		callback,
	); err != nil {
		logx.Errorf("Failed to start LoadAggregateData: %v", err)
		return err
	}

	return nil
}
