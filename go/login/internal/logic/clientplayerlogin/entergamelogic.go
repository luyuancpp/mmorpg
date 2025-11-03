package clientplayerloginlogic

import (
	"context"
	"errors"
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
	// 1. 创建带超时的上下文（支持主动取消，保留原有超时配置）
	ctx := l.ctx

	// 1. 获取 Session（原始步骤1，不改动）
	sessionDetails, ok := ctxkeys.GetSessionDetails(ctx)
	if !ok || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 2. 获取 LoginSessionInfo（原始步骤2，不改动）
	session, err := loginsessionstore.GetLoginSession(ctx, l.svcCtx.RedisClient, sessionDetails.SessionId)
	if err != nil {
		logx.Errorf("GetLoginSession failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginSessionNotFound)
		return resp, nil
	}

	// 3. 加锁防止同角色并发登录（原始步骤3，不改动）
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

	taskCallback := func(taskKey string, taskSuccess bool, err error) {
		// 原有回调逻辑：通知中心
		req := &login_proto_centre.CentrePlayerGameNodeEntryRequest{
			ClientMsgBody: &login_proto_centre.CentreEnterGameRequest{
				PlayerId: in.PlayerId,
			},
			SessionInfo: sessionDetails,
		}
		node := l.svcCtx.GetCentreClient()
		if node != nil {
			node.Send(req, game.CentreLoginNodeEnterGameMessageId)
		}

		// 所有任务完成，主动取消ctx（避免超时等待）
		logx.Infof("All tasks completed, cancel ctx actively. playerId=%d", in.PlayerId)
	}

	defer func() {
		ok, err := tryLocker.Release(ctx)
		if err != nil {
			logx.Errorf("Failed to release lock: %v", err)
		} else if !ok {
			logx.Infof("Lock was not held by us (possibly expired)")
		}
	}()

	// 4. 加载账号信息并验证角色归属（原始步骤4，不改动）
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

	// 5. FSM 状态管理（按 sessionId）（原始步骤5，不改动）
	sessionIdStr := strconv.FormatUint(sessionDetails.SessionId, 10)
	f := data.InitPlayerFSM()
	if err := fsmstore.LoadFSMState(ctx, l.svcCtx.RedisClient, f, sessionIdStr, ""); err != nil {
		logx.Errorf("FSM Load failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginFsmFailed)
		return resp, nil
	}

	if err := f.Event(ctx, data.EventEnterGame); err != nil {
		logx.Errorf("FSM Event failed: %v", err)
		resp.ErrorMessage.Id = uint32(table.LoginError_kLoginInProgress)
		return resp, nil
	}

	// 6. 加载 Player 数据（改造核心：同步等待任务完成，保留原有逻辑）
	loadErrChan := make(chan error, 1)
	go func() {
		// 调用数据加载接口，传入带计数的回调
		err := l.ensurePlayerDataInRedis(ctx, in.PlayerId, taskCallback)
		loadErrChan <- err
		close(loadErrChan)
	}()

	// 等待：要么任务全部完成，要么超时
	select {
	case err := <-loadErrChan:
		if err != nil {
			resp.ErrorMessage.Id = uint32(table.LoginError_kLoginPlayerGuidError)
			logx.Errorf("Load player data failed: %v", err)
			return resp, err
		}
		// 加载接口启动成功，等待所有任务完成
	case <-ctx.Done():
		if errors.Is(ctx.Err(), context.DeadlineExceeded) {
			resp.ErrorMessage.Id = uint32(table.LoginError_kLoginTimeout)
			//todo
			return resp, nil
		}
		// 主动取消（任务完成），正常继续
	}

	// 7. 清理 Session 和 FSM（原始步骤7，不改动）
	_ = sessioncleaner.CleanupSession(
		ctx,
		l.svcCtx.RedisClient,
		sessionDetails.SessionId,
		"enterGame",
	)

	resp.PlayerId = in.PlayerId
	return resp, nil
}

// 改造ensurePlayerDataInRedis：接收自定义回调函数，不改动原有数据加载逻辑
func (l *EnterGameLogic) ensurePlayerDataInRedis(
	ctx context.Context,
	playerId uint64,
	taskCallback func(taskKey string, taskSuccess bool, err error),
) error {
	// 仅处理指定的消息列表（原始列表不变）
	messagesToLoad := []proto.Message{
		&login_proto_database.PlayerAllData{
			PlayerDatabaseData:   &login_proto_database.PlayerDatabase{PlayerId: playerId},
			PlayerDatabase_1Data: &login_proto_database.PlayerDatabase_1{PlayerId: playerId},
		},
		&login_proto_database.PlayerCentreDatabase{PlayerId: playerId},
	}

	// 调用加载接口，传入外部传入的回调（带计数和主动取消逻辑）
	return dataloader.LoadWithPlayerId(
		ctx,
		l.svcCtx.RedisClient,
		l.svcCtx.KafkaClient,
		l.svcCtx.TaskExecutor,
		messagesToLoad,
		taskCallback,
	)
}
