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
	"errors"
	"strconv"
	"time"

	"login/internal/config"
	"login/internal/constants"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// cleanupMissTotal counts Cleanup() calls where the session key was already
// absent in Redis (legitimate race: Disconnect arrived before BindSession
// committed, or a duplicate Disconnect). Surfaced as a metric so the previous
// Error-level spam can stay at Debug without losing observability.
var cleanupMissTotal = promauto.NewCounterVec(prometheus.CounterOpts{
	Name: "loginsession_cleanup_miss_total",
	Help: "Cleanup() called but session key absent (Disconnect/BindSession race).",
}, []string{"caller"})

func sessionKey(sessionID uint32) string {
	return "login_session:" + strconv.FormatUint(uint64(sessionID), 10)
}

func sessionTTL() time.Duration {
	return time.Duration(config.AppConfig.Node.SessionExpireMin) * time.Minute
}

// Save stores the account for a session with a TTL.
// Idempotent: overwrites if already present (same account, protected by account lock).
func Save(ctx context.Context, rdb *redis.Client, sessionID uint32, account string) error {
	return rdb.Set(ctx, sessionKey(sessionID), account, sessionTTL()).Err()
}

// PruneStaleFromDeviceSet scans the per-account device set and removes any
// sessionIDs whose backing login_session:{id} key no longer exists. Returns
// the number of entries removed.
//
// This is a best-effort self-heal called from Login to recover from the
// race where Gate's socket-closed → login.Disconnect RPC is still in
// flight when the same account reconnects — without it, a heavy-reconnect
// workload (stress robot) can trip MaxDevicesPerAccount on stale entries
// that Cleanup never got around to removing.
func PruneStaleFromDeviceSet(ctx context.Context, rdb *redis.Client, deviceSetKey string) int {
	members, err := rdb.SMembers(ctx, deviceSetKey).Result()
	if err != nil || len(members) == 0 {
		return 0
	}
	stale := make([]interface{}, 0, len(members))
	for _, sidStr := range members {
		exists, err := rdb.Exists(ctx, "login_session:"+sidStr).Result()
		if err != nil {
			continue
		}
		if exists == 0 {
			stale = append(stale, sidStr)
		}
	}
	if len(stale) == 0 {
		return 0
	}
	if err := rdb.SRem(ctx, deviceSetKey, stale...).Err(); err != nil {
		logx.Errorf("PruneStaleFromDeviceSet SRem failed: key=%s err=%v", deviceSetKey, err)
		return 0
	}
	logx.Infof("Pruned %d stale sessions from device set %s", len(stale), deviceSetKey)
	return len(stale)
}

// GetAccount returns the account name for a session.
func GetAccount(ctx context.Context, rdb *redis.Client, sessionID uint32) (string, error) {
	return rdb.Get(ctx, sessionKey(sessionID)).Result()
}

// Cleanup removes the session key and the device-set entry.
//
// `redis: nil` here is a legitimate race (Disconnect arriving before BindSession
// committed, or a second Cleanup for the same sessionId). Round 14 demotes that
// case to Debug and surfaces it via a Prometheus counter so the login ERROR log
// is no longer flooded under high reconnect churn (45k-bot stress saw tens of
// thousands of these per minute, drowning out real errors). Real Redis errors
// (connection drop, command timeout) stay at Error level.
func Cleanup(ctx context.Context, rdb *redis.Client, sessionID uint32, logicTag string) {
	key := sessionKey(sessionID)
	account, err := rdb.Get(ctx, key).Result()
	if err != nil {
		if errors.Is(err, redis.Nil) {
			cleanupMissTotal.WithLabelValues(logicTag).Inc()
			logx.Debugf("[%s] cleanup miss (session not in redis, expected race): sessionId=%d", logicTag, sessionID)
		} else {
			logx.Errorf("[%s] get account failed for cleanup: sessionId=%d err=%v", logicTag, sessionID, err)
		}
	} else {
		rdb.SRem(ctx, constants.GenerateSessionKey(account), sessionID)
	}
	rdb.Del(ctx, key)
	logx.Infof("[%s] session cleanup: sessionId=%d", logicTag, sessionID)
}
