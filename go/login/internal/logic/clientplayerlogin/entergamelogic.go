package clientplayerloginlogic

import (
	"context"
	"errors"
	"fmt"
	"google.golang.org/protobuf/proto"
	"login/data"
	"login/generated/pb/game"
	"login/generated/pb/table"
	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/dataloader"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsessionstore"
	"login/internal/logic/utils/sessioncleaner"
	"login/internal/svc"
	login_proto_common "login/proto/common"
	login_proto_database "login/proto/logic/database"
	login_proto_centre "login/proto/service/cpp/rpc/centre"
	login_proto "login/proto/service/go/grpc/login"
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

func (l *EnterGameLogic) EnterGame(in *login_proto.EnterGameRequest) (*login_proto.EnterGameResponse, error) {
	resp := &login_proto.EnterGameResponse{ErrorMessage: &login_proto_common.TipInfoMessage{}}
	customCtx, _ := context.WithCancel(context.Background())

	// 1. 获取 Session
	sessionDetails, ok := ctxkeys.GetSessionDetails(l.ctx)
	if !ok || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 2. 获取 LoginSessionInfo
	session, err := loginsessionstore.GetLoginSession(customCtx, l.svcCtx.RedisClient, sessionDetails.SessionId)
	if err != nil {
		logx.Errorf("GetLoginSession failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginSessionNotFound)
		return resp, nil
	}

	// 3. 加锁防止同角色并发登录
	playerLocker := locker.NewRedisLocker(l.svcCtx.RedisClient)
	key := "player_locker:" + strconv.FormatUint(in.PlayerId, 10)
	tryLocker, err := playerLocker.TryLock(customCtx, key, time.Duration(config.AppConfig.Locker.PlayerLockTTL)*time.Second)
	if err != nil {
		logx.Errorf("EnterGame lock acquire failed for playerId=%d: %v", in.PlayerId, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
		return resp, nil
	}

	if !tryLocker.IsLocked() {
		logx.Errorf("EnterGame lock acquire failed for playerId=%d: %v", in.PlayerId, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
		return resp, nil
	}

	defer func() {
		ok, err := tryLocker.Release(customCtx)
		if err != nil {
			logx.Errorf("Failed to release lock: %v", err)
		} else if !ok {
			logx.Infof("Lock was not held by us (possibly expired)")
		}
	}()

	// 4. 加载账号信息并验证角色归属
	accountKey := constants.GetAccountDataKey(session.Account)
	dataBytes, err := l.svcCtx.RedisClient.Get(customCtx, accountKey).Bytes()
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

	// 5. FSM 状态管理（按 sessionId）
	sessionIdStr := strconv.FormatUint(sessionDetails.SessionId, 10)
	f := data.InitPlayerFSM()
	if err := fsmstore.LoadFSMState(customCtx, l.svcCtx.RedisClient, f, sessionIdStr, ""); err != nil {
		logx.Errorf("FSM Load failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginFsmFailed)
		return resp, nil
	}

	if err := f.Event(context.Background(), data.EventEnterGame); err != nil {
		logx.Errorf("FSM Event failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginInProgress)
		return resp, nil
	}

	// 6. 加载 Player 数据（若不在 RedisClient）
	if err := l.ensurePlayerDataInRedis(in.PlayerId); err != nil {
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginPlayerGuidError)
		logx.Errorf("Load player data failed: %v", err)
		return resp, err
	}

	// 7. 清理 Session 和 FSM
	_ = sessioncleaner.CleanupSession(
		customCtx,
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

	customCtx, _ := context.WithCancel(context.Background())

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
		completedTasks++

		hasFailedTask := false

		// 只要有一个任务失败，整体标记为失败
		if !taskSuccess {
			hasFailedTask = true
		}

		// 所有任务都完成后，触发通知（只执行一次）
		if completedTasks >= totalTasksToLoad {
			notifyOnce.Do(func() {
				req := &login_proto_centre.CentrePlayerGameNodeEntryRequest{
					ClientMsgBody: &login_proto_centre.CentreEnterGameRequest{
						PlayerId: playerId,
					},
					SessionInfo: sessionDetails,
				}

				node := l.svcCtx.GetCentreClient()
				if node != nil {
					node.Send(req, game.CentreLoginNodeEnterGameMessageId)
				}

				// 根据实际情况输出日志
				if hasFailedTask {
					logx.Errorf("All %d tasks completed with some failures, notified centre", totalTasksToLoad)
				} else {
					logx.Infof("All %d tasks completed successfully, notified centre", totalTasksToLoad)
				}
			})
		}
	}

	// 第一个任务：加载中心数据库数据
	msgCentre := &login_proto_database.PlayerCentreDatabase{PlayerId: playerId}
	if err := dataloader.BatchLoadAndCache(
		customCtx,
		l.svcCtx.RedisClient,
		l.svcCtx.KafkaClient,
		playerId,
		[]proto.Message{msgCentre},
		l.svcCtx.TaskExecutor,
		callback,
	); err != nil {
		logx.Errorf("Failed to start BatchLoadAndCache: %v", err)
		return err
	}

	// 第二个任务：加载聚合数据
	playerAll := &login_proto_database.PlayerAllData{}
	if err := dataloader.LoadAggregateData(
		customCtx,
		l.svcCtx.RedisClient,
		l.svcCtx.KafkaClient,
		playerId,
		playerAll,
		func(id uint64) []proto.Message {
			return []proto.Message{
				&login_proto_database.PlayerDatabase{PlayerId: id},
				&login_proto_database.PlayerDatabase_1{PlayerId: id},
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
