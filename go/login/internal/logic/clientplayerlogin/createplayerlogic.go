package clientplayerloginlogic

import (
	"context"
	"errors"
	"login/data"
	"login/generated/pb/table"
	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/fsmstore"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsessionstore"
	"login/internal/svc"
	login_proto_common "proto/common/base"
	login_data_base "proto/common/database"
	login_proto "proto/login"
	"strconv"
	"time"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"

	"github.com/zeromicro/go-zero/core/logx"
)

type CreatePlayerLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewCreatePlayerLogic(ctx context.Context, svcCtx *svc.ServiceContext) *CreatePlayerLogic {
	return &CreatePlayerLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *CreatePlayerLogic) CreatePlayer(in *login_proto.CreatePlayerRequest) (*login_proto.CreatePlayerResponse, error) {
	resp := &login_proto.CreatePlayerResponse{
		Players: make([]*login_proto.AccountSimplePlayerWrapper, 0),
	}

	// 1. Get Session details
	sessionDetails, sessionFound := ctxkeys.GetSessionDetails(l.ctx)
	if !sessionFound || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or invalid during player creation")
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	// 2. Get LoginSessionInfo (includes account)
	session, err := loginsessionstore.GetLoginSession(l.ctx, l.svcCtx.RedisClient, sessionDetails.SessionId)
	if err != nil {
		logx.Errorf("GetLoginSession failed: %v", err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginSessionNotFound)}
		return resp, nil
	}
	account := session.Account

	// 3. Lock (prevent concurrent character creation)
	accountLocker := locker.NewAccountLocker(l.svcCtx.RedisClient, time.Duration(config.AppConfig.Locker.AccountLockTTL)*time.Second)
	lockAcquired, err := accountLocker.AcquireCreate(l.ctx, account)
	if err != nil || !lockAcquired {
		logx.Errorf("CreatePlayer lock acquire failed for account=%s: %v", account, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
		return resp, nil
	}
	defer accountLocker.ReleaseCreate(l.ctx, account)

	// 4. Load account data
	accountDataKey := constants.GetAccountDataKey(account)
	cmd := l.svcCtx.RedisClient.Get(l.ctx, accountDataKey)
	if err := cmd.Err(); err != nil {
		if errors.Is(err, redis.Nil) {
			logx.Infof("Account not found in redis: %s", account)
			resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginAccountNotFound)}
			return resp, nil
		}
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisError)}
		logx.Errorf("RedisClient get failed, account: %s, err: %v", account, err)
		return resp, err
	}

	// 5. FSM state management (by sessionId)
	sessionIdStr := strconv.FormatUint(sessionDetails.SessionId, 10)
	playerFSM := data.InitPlayerFSM()
	if err := fsmstore.LoadFSMState(l.ctx, l.svcCtx.RedisClient, playerFSM, sessionIdStr, ""); err != nil {
		logx.Errorf("Failed to load FSM state for session %s: %v", sessionIdStr, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginFSMLoadFailed)}
		return resp, nil
	}
	if err := playerFSM.Event(l.ctx, data.EventCreateChar); err != nil {
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginFsmFailed)}
		logx.Errorf("FSM create_char failed, account: %s, err: %v", account, err)
		return resp, nil
	}

	// 6. Decode account data
	userAccount := &login_data_base.UserAccounts{}
	if err := proto.Unmarshal([]byte(cmd.Val()), userAccount); err != nil {
		logx.Errorf("Failed to unmarshal user account, err: %v", err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginDataParseFailed)}
		return resp, nil
	}
	if userAccount.SimplePlayers == nil {
		userAccount.SimplePlayers = &login_proto_common.AccountSimplePlayerList{Players: make([]*login_proto_common.AccountSimplePlayer, 0)}
	}

	// 7. Create character
	if len(userAccount.SimplePlayers.Players) >= 5 {
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginAccountPlayerFull)}
		logx.Infof("Account player limit reached: %s", account)
		return resp, nil
	}
	newPlayerId := uint64(l.svcCtx.SnowFlake.Generate())
	newPlayer := &login_proto_common.AccountSimplePlayer{PlayerId: newPlayerId}
	userAccount.SimplePlayers.Players = append(userAccount.SimplePlayers.Players, newPlayer)

	// 8. Write back to Redis
	dataBytes, err := proto.Marshal(userAccount)
	if err != nil {
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginDataSerializeFailed)}
		logx.Errorf("Failed to marshal user account, err: %v", err)
		return resp, nil
	}
	if err := l.svcCtx.RedisClient.Set(l.ctx, accountDataKey, dataBytes, config.AppConfig.Account.CacheExpire).Err(); err != nil {
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisSetFailed)}
		logx.Errorf("Failed to set user account in RedisClient, account: %s, err: %v", account, err)
		return resp, nil
	}

	// 9. Save FSM state (by sessionId)
	if err := fsmstore.SaveFSMState(l.ctx, l.svcCtx.RedisClient, playerFSM, sessionIdStr, ""); err != nil {
		logx.Errorf("Failed to save FSM state, sessionId: %s, err: %v", sessionIdStr, err)
	}

	// 10. Return player info
	for _, p := range userAccount.SimplePlayers.Players {
		resp.Players = append(resp.Players, &login_proto.AccountSimplePlayerWrapper{Player: p})
	}

	logx.Infof("Player created successfully, account: %s, playerId: %d", account, newPlayerId)
	return resp, nil
}
