package clientplayerloginlogic

import (
	"context"
	"errors"
	"fmt"
	"login/internal/config"
	"login/internal/constants"
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

	account := in.Account

	// SA-Token validation: if sa_token is provided and SA-Token is enabled,
	// validate the token against SA-Token's Redis and use the loginId as account.
	if in.SaToken != "" {
		resolvedAccount, err := l.validateSaToken(in.SaToken)
		if err != nil {
			logx.Errorf("SA-Token validation failed: token=%s err=%v", in.SaToken, err)
			resp.ErrorMessage = &login_proto_common.TipInfoMessage{Id: uint32(table.LoginError_kLoginAccountNotFound)}
			return resp, nil
		}
		account = resolvedAccount
		logx.Infof("SA-Token validated: token=%s -> account=%s", in.SaToken, account)
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

// validateSaToken looks up the SA-Token value in the SA-Token Redis instance
// and returns the associated loginId (used as the account name).
// SA-Token Redis key format: {tokenName}:{loginType}:token:{tokenValue} -> loginId
func (l *LoginLogic) validateSaToken(tokenValue string) (string, error) {
	if !config.AppConfig.SaToken.Enabled {
		return "", fmt.Errorf("SA-Token validation is not enabled")
	}
	if l.svcCtx.SaTokenRedisClient == nil {
		return "", fmt.Errorf("SA-Token Redis client is not initialized")
	}

	tokenName := config.AppConfig.SaToken.TokenName
	loginType := config.AppConfig.SaToken.LoginType
	key := fmt.Sprintf("%s:%s:token:%s", tokenName, loginType, tokenValue)

	loginId, err := l.svcCtx.SaTokenRedisClient.Get(l.ctx, key).Result()
	if errors.Is(err, redis.Nil) {
		return "", fmt.Errorf("sa-token not found or expired: %s", tokenValue)
	}
	if err != nil {
		return "", fmt.Errorf("sa-token Redis lookup failed: %w", err)
	}
	if loginId == "" {
		return "", fmt.Errorf("sa-token maps to empty loginId")
	}

	return loginId, nil
}
