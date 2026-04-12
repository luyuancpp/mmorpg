// Package loginsession manages login session state in Redis.
//
// Each session is stored as a Redis key:
//
//	login_session:{sessionID} → account string
//
// Operations:
//
//	Save:       creates the session key with account value + TTL.
//	GetAccount: returns the account for a session.
//	Cleanup:    deletes the session key and removes the device-set entry.
package loginsession

import (
	"context"
	"strconv"
	"time"

	"login/internal/config"
	"login/internal/constants"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

func sessionKey(sessionID uint64) string {
	return "login_session:" + strconv.FormatUint(sessionID, 10)
}

func sessionTTL() time.Duration {
	return time.Duration(config.AppConfig.Node.SessionExpireMin) * time.Minute
}

// Save stores the account for a session with a TTL.
// Idempotent: overwrites if already present (same account, protected by account lock).
func Save(ctx context.Context, rdb *redis.Client, sessionID uint64, account string) error {
	return rdb.Set(ctx, sessionKey(sessionID), account, sessionTTL()).Err()
}

// GetAccount returns the account name for a session.
func GetAccount(ctx context.Context, rdb *redis.Client, sessionID uint64) (string, error) {
	return rdb.Get(ctx, sessionKey(sessionID)).Result()
}

// Cleanup removes the session key and the device-set entry.
func Cleanup(ctx context.Context, rdb *redis.Client, sessionID uint64, logicTag string) {
	key := sessionKey(sessionID)
	account, err := rdb.Get(ctx, key).Result()
	if err != nil {
		logx.Errorf("[%s] get account failed for cleanup: sessionId=%d err=%v", logicTag, sessionID, err)
	} else {
		rdb.SRem(ctx, constants.GenerateSessionKey(account), sessionID)
	}
	rdb.Del(ctx, key)
	logx.Infof("[%s] session cleanup: sessionId=%d", logicTag, sessionID)
}
