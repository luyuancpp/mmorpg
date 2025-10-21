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

	// 基于外部上下文创建子上下文，确保资源可控

	// 仅处理指定的消息列表
	messagesToLoad := []proto.Message{
		&login_proto_database.PlayerAllData{
			PlayerDatabaseData:   &login_proto_database.PlayerDatabase{PlayerId: playerId},
			PlayerDatabase_1Data: &login_proto_database.PlayerDatabase_1{PlayerId: playerId},
		},
		&login_proto_database.PlayerCentreDatabase{PlayerId: playerId},
	}

	// 任务完成回调
	taskGroupCallback := func(taskKey string, taskSuccess bool, err error) {
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
	}

	// 调用加载接口，仅处理上述消息
	return dataloader.LoadWithPlayerId(
		l.svcCtx.RedisClient,
		l.svcCtx.KafkaClient,
		l.svcCtx.TaskExecutor,
		messagesToLoad,
		taskGroupCallback,
	)
}
