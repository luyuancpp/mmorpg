package clientplayerloginlogic

import (
	"context"
	"errors"
	"fmt"
	"login/internal/config"
	"login/internal/constants"
	"login/internal/logic/pkg/auth"
	"login/internal/logic/pkg/ctxkeys"
	"login/internal/logic/pkg/locker"
	"login/internal/logic/pkg/loginsession"
	"login/internal/svc"
	login_proto_common "proto/common/base"
	login_proto_data_base "proto/common/database"
	login_proto "proto/login"
	"shared/generated/pb/table"
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
	resp := &login_proto.LoginResponse{}

	// Resolve account via auth provider
	account, err := l.resolveAccount(in)
	if err != nil {
		logx.Errorf("Auth failed: type=%s err=%v", in.AuthType, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginAccountNotFound)}
		return resp, nil
	}

	// 1. Distributed lock (UUID + Lua safe release)
	accountLock, err := locker.NewRedisLocker(l.svcCtx.RedisClient).TryLock(
		l.ctx, "account_lock:login:"+account,
		time.Duration(config.AppConfig.Locker.AccountLockTTL)*time.Second,
	)
	if err != nil || !accountLock.IsLocked() {
		logx.Errorf("Login lock failed for account=%s: %v", account, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginInProgress)}
		return resp, nil
	}
	defer func() {
		if _, err := accountLock.Release(l.ctx); err != nil {
			logx.Errorf("Login lock release failed for account=%s: %v", account, err)
		}
	}()

	// 2. Validate session
	sessionDetails, sessionFound := ctxkeys.GetSessionDetails(l.ctx)
	if !sessionFound || sessionDetails.SessionId <= 0 {
		logx.Error("SessionId not found in context during login")
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginSessionIdNotFound)}
		return resp, nil
	}

	logx.Infof("Login start: account=%s sessionId=%d", account, sessionDetails.SessionId)

	// 3. Save login session (stores account for this session in Redis)
	if err := loginsession.Save(l.ctx, l.svcCtx.RedisClient, sessionDetails.SessionId, account); err != nil {
		logx.Errorf("Login save failed: sessionId=%d account=%s err=%v", sessionDetails.SessionId, account, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}

	// 4. Enforce device limit
	sessionKey := constants.GenerateSessionKey(account)
	expire := time.Duration(config.AppConfig.Node.SessionExpireMin) * time.Minute

	_, err = l.svcCtx.RedisClient.TxPipelined(l.ctx, func(pipe redis.Pipeliner) error {
		pipe.SAdd(l.ctx, sessionKey, sessionDetails.SessionId)
		pipe.Expire(l.ctx, sessionKey, expire)
		return nil
	})
	if err != nil {
		logx.Errorf("Device set pipeline failed: sessionKey=%s err=%v", sessionKey, err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}

	count, err := l.svcCtx.RedisClient.SCard(l.ctx, sessionKey).Result()
	if err != nil {
		logx.Errorf("SCard error: %v", err)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginRedisSetFailed)}
		return resp, nil
	}
	if count > config.AppConfig.Account.MaxDevicesPerAccount {
		logx.Infof("Account %s exceeds device limit: %d > %d", account, count, config.AppConfig.Account.MaxDevicesPerAccount)
		resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kTooManyDevices)}
		return resp, nil
	}

	// 5. Load account data
	userAccount, err := GetOrInitUserAccount(l.ctx, l.svcCtx.RedisClient, account, config.AppConfig.Account.CacheExpire)
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

// resolveAccount determines the account identifier based on the auth type.
// For "password" (or empty), the account comes directly from the request.
// For third-party providers, the auth_token is validated via the registered provider.
func (l *LoginLogic) resolveAccount(in *login_proto.LoginRequest) (string, error) {
	// Dev mode: skip all auth provider validation
	if config.AppConfig.DevSkipAuth {
		logx.Infow("DevSkipAuth enabled, using account field directly", logx.Field("account", in.Account))
		return in.Account, nil
	}

	authType := in.AuthType
	if authType == "" || authType == "password" {
		return in.Account, nil
	}

	provider := auth.Get(authType)
	if provider == nil {
		return "", fmt.Errorf("unknown auth type: %s", authType)
	}

	result, err := provider.Validate(l.ctx, in.AuthToken)
	if err != nil {
		return "", err
	}
	return result.Account, nil
}
