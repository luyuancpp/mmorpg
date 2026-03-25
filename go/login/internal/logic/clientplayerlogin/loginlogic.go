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
	login_proto_data_base "proto/common/database"
	login_proto "proto/login"
	"strconv"
	"time"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"

	"github.com/zeromicro/go-zero/core/logx"
)

type LoginLogic struct {
	ctx    context.Context
	svcCtx *svc.ServiceContext
	logx.Logger
}

func NewLoginLogic(ctx context.Context, svcCtx *svc.ServiceContext) *LoginLogic {
	return &LoginLogic{
		ctx:    ctx,
		svcCtx: svcCtx,
		Logger: logx.WithContext(ctx),
	}
}

func (l *LoginLogic) Login(in *login_proto.LoginRequest) (*login_proto.LoginResponse, error) {
	//todo Account login sessions stored in Redis immediately; consider memory pressure on day-1 mass registration and when to evict
	//todo If connection breaks mid-flow and a new gate is used, login should succeed on the new gate; messages must route to new gate, old gate follows normal disconnect flow
	//todo If async dual-gate login occurs and old gate's login arrives at controller after new gate's, could login fail? How to handle?
	resp := &login_proto.LoginResponse{}

	// 1. Distributed lock with retry
	accountLocker := locker.NewAccountLocker(l.svcCtx.RedisClient, time.Duration(config.AppConfig.Locker.AccountLockTTL)*time.Second)

	lockAcquired, err := accountLocker.AcquireLogin(l.ctx, in.Account)
	if err != nil || !lockAcquired {
		logx.Errorf("Login lock acquire failed for account=%s, err=%v", in.Account, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
		return resp, nil
	}
	defer func(accountLocker *locker.AccountLocker, ctx context.Context, account string) {
		err := accountLocker.ReleaseLogin(ctx, account)
		if err != nil {
			logx.Errorf("Login lock release failed for account=%s, err=%v", in.Account, err)
		}
	}(accountLocker, l.ctx, in.Account)

	// 2. Get Session
	sessionDetails, sessionFound := ctxkeys.GetSessionDetails(l.ctx)
	if !sessionFound || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found or empty in context during login")
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	sessionId := strconv.FormatUint(sessionDetails.SessionId, 10)
	logx.Infof("Start processing login for account=%s with sessionId=%s", in.Account, sessionId)

	// 3. FSM load + execute + save; return immediately on error
	playerFSM := data.InitPlayerFSM()

	if err := fsmstore.LoadFSMState(l.ctx, l.svcCtx.RedisClient, playerFSM, sessionId, ""); err != nil {
		logx.Errorf("FSM state load failed for sessionId=%s, account=%s, error: %v", sessionId, in.Account, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginFSMLoadFailed)}
		return resp, nil
	}

	// Execute FSM event
	if err := playerFSM.Event(l.ctx, data.EventProcessLogin); err != nil {
		logx.Errorf("FSM transition error for sessionId=%s, account=%s, event=process_login, error: %v", sessionId, in.Account, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginFSMEventFailed)}
		return resp, nil
	}

	// Save FSM state
	if err := fsmstore.SaveFSMState(l.ctx, l.svcCtx.RedisClient, playerFSM, sessionId, ""); err != nil {
		logx.Errorf("FSM save failed for sessionId=%s, account=%s, error: %v", sessionId, in.Account, err)
		// Non-blocking, but log the error
		//todo Have the client re-login
		return resp, nil
	}

	// 4. Limit device count

	// Add SessionId to the device set
	sessionKey := constants.GenerateSessionKey(in.Account)
	expire := time.Duration(config.AppConfig.Node.SessionExpireMin) * time.Minute

	_, err = l.svcCtx.RedisClient.TxPipelined(l.ctx, func(pipe redis.Pipeliner) error {
		pipe.SAdd(l.ctx, sessionKey, sessionDetails.SessionId)
		pipe.Expire(l.ctx, sessionKey, expire)
		return nil
	})
	if err != nil {
		logx.Errorf("Failed to SAdd + Expire in pipeline for sessionKey=%s: %v", sessionKey, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}

	// Enforce max N devices per account
	count, err := l.svcCtx.RedisClient.SCard(l.ctx, sessionKey).Result()
	if err != nil {
		logx.Errorf("RedisClient SCard error: %v", err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}

	if count > config.AppConfig.Account.MaxDevicesPerAccount {
		logx.Infof("Account %s exceeds device limit: %d > %d", in.Account, count, config.AppConfig.Account.MaxDevicesPerAccount)

		// Optional: remove old sessions (by TTL or FIFO)
		// Or prompt client "another device is logged in, force kick?" — requires client support.

		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kTooManyDevices)}
		return resp, nil
	}

	// 5. Build and save login session info
	sessionInfo := &login_proto.LoginSessionInfo{
		Account:   in.Account,
		RoleId:    0,
		SessionId: sessionDetails.SessionId,
		GateId:    0,
		DeviceId:  "",
		LoginTime: time.Now().Unix(),
		Fsm:       playerFSM.Current(),
	}
	if err := loginsessionstore.SaveLoginSession(l.ctx, l.svcCtx.RedisClient, sessionInfo, expire); err != nil {
		logx.Errorf("Failed to save login session for account=%s: %v", in.Account, err)
		// Non-blocking
	}

	// 6. Load account data
	userAccount, err := GetOrInitUserAccount(l.ctx, l.svcCtx.RedisClient, in.Account, config.AppConfig.Account.CacheExpire)
	if err != nil {
		return nil, err
	}

	// 7. Return player list
	if userAccount.SimplePlayers != nil {
		for _, v := range userAccount.SimplePlayers.Players {
			resp.Players = append(resp.Players, &login_proto.AccountSimplePlayerWrapper{Player: v})
		}
	}

	return resp, nil
}

func GetOrInitUserAccount(ctx context.Context, rdb *redis.Client, account string, ttl time.Duration) (*login_proto_data_base.UserAccounts, error) {
	key := constants.GetAccountDataKey(account)

	// Try Redis first
	cmd := rdb.Get(ctx, key)
	valueBytes, err := cmd.Bytes()

	if errors.Is(err, redis.Nil) {
		// Not in Redis; create empty default
		logx.Infof("UserAccounts not found for account=%s, initializing default", account)
		userAccount := &login_proto_data_base.UserAccounts{}

		valueBytes, err = proto.Marshal(userAccount)
		if err != nil {
			logx.Errorf("Marshal default UserAccounts failed: %v", err)
			return nil, err
		}

		// Save to Redis
		err = rdb.Set(ctx, key, valueBytes, ttl).Err()
		if err != nil {
			logx.Errorf("Failed to save default UserAccounts to Redis for account=%s: %v", account, err)
			return nil, err
		}

		return userAccount, nil
	}

	if err != nil {
		logx.Errorf("Failed to get UserAccounts from Redis: %v", err)
		return nil, err
	}

	// Deserialize and return
	userAccount := &login_proto_data_base.UserAccounts{}
	if err := proto.Unmarshal(valueBytes, userAccount); err != nil {
		logx.Errorf("Unmarshal user account failed for account=%s: %v", account, err)
		return nil, err
	}

	return userAccount, nil
}

// startLoginDurationTimer starts a timer to force logout after MaxLoginDuration
/*func startLoginDurationTimer(playerID string, loginTime time.Time, cfg NodeConfig) {
	expireTime := loginTime.Add(cfg.MaxLoginDuration)
	// Calculate reminder time (LogoutGraceTime before expiry)
	remindTime := expireTime.Add(-cfg.LogoutGraceTime)

	// Start the timer
	go func() {
		now := time.Now()
		// Wait until reminder time
		time.Sleep(remindTime.Sub(now))
		// Push logout reminder
		pushLogoutRemind(playerID, cfg.LogoutGraceTime)

		// Wait until expiry
		time.Sleep(cfg.LogoutGraceTime)
		// Force logout
		forceLogout(playerID, "login duration exceeded max limit")
	}()
}*/
