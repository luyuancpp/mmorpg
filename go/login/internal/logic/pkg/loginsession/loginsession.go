// Package loginsession manages login session state in Redis.
//
// Each session is stored as a single Redis HASH:
//
//	login_session:{sessionID} → { account: "...", step: "..." }
//
// Step transitions:
//
//	Save:    (key absent)    → step = logged_in      (Login)
//	Advance: logged_in      → creating_char          (CreatePlayer)
//	Advance: creating_char  → logged_in              (CreatePlayer done)
//	Advance: logged_in      → entering_game          (EnterGame)
//	Advance: creating_char  → entering_game          (EnterGame)
//	entering_game is a dead end — no transitions allowed until Cleanup.
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

const (
	StepLoggedIn     = "logged_in"
	StepCreatingChar = "creating_char"
	StepEnteringGame = "entering_game"
)

func hashKey(sessionID uint64) string {
	return "login_session:" + strconv.FormatUint(sessionID, 10)
}

func sessionTTLSeconds() int {
	return int(time.Duration(config.AppConfig.Node.SessionExpireMin) * time.Minute / time.Second)
}

// saveLua atomically creates a session: validates no existing step, then stores account + step.
var saveLua = redis.NewScript(`
local key = KEYS[1]
local cur = redis.call("HGET", key, "step")
if cur ~= false then
    return redis.error_reply("already logged in: step=" .. cur)
end
redis.call("HSET", key, "account", ARGV[1], "step", "logged_in")
redis.call("EXPIRE", key, tonumber(ARGV[2]))
return "OK"
`)

// Save atomically creates a login session with account and step=logged_in.
// Returns an error if the session already exists (prevents double-login on same session).
func Save(ctx context.Context, rdb *redis.Client, sessionID uint64, account string) error {
	return saveLua.Run(ctx, rdb, []string{hashKey(sessionID)}, account, sessionTTLSeconds()).Err()
}

// advanceLua atomically validates a step transition and updates the step.
var advanceLua = redis.NewScript(`
local key = KEYS[1]
local cur = redis.call("HGET", key, "step")
if cur == false then cur = "" end
local nxt = ARGV[1]
local ok = false
if cur == "logged_in" then
    ok = (nxt == "creating_char" or nxt == "entering_game")
elseif cur == "creating_char" then
    ok = (nxt == "logged_in" or nxt == "entering_game")
end
if not ok then
    return redis.error_reply("invalid step: " .. cur .. " -> " .. nxt)
end
redis.call("HSET", key, "step", nxt)
redis.call("EXPIRE", key, tonumber(ARGV[2]))
return "OK"
`)

// Advance atomically validates the step transition and sets the new step.
func Advance(ctx context.Context, rdb *redis.Client, sessionID uint64, nextStep string) error {
	return advanceLua.Run(ctx, rdb, []string{hashKey(sessionID)}, nextStep, sessionTTLSeconds()).Err()
}

// GetAccount returns the account name for a session.
func GetAccount(ctx context.Context, rdb *redis.Client, sessionID uint64) (string, error) {
	return rdb.HGet(ctx, hashKey(sessionID), "account").Result()
}

// Cleanup removes the session hash and the device-set entry.
func Cleanup(ctx context.Context, rdb *redis.Client, sessionID uint64, logicTag string) {
	key := hashKey(sessionID)
	account, err := rdb.HGet(ctx, key, "account").Result()
	if err != nil {
		logx.Errorf("[%s] get account failed for cleanup: sessionId=%d err=%v", logicTag, sessionID, err)
	} else {
		rdb.SRem(ctx, constants.GenerateSessionKey(account), sessionID)
	}
	rdb.Del(ctx, key)
	logx.Infof("[%s] session cleanup: sessionId=%d", logicTag, sessionID)
}

// isValidTransition checks if a step transition is valid.
// This mirrors the Lua logic in saveLua and advanceLua (used for testing).
func isValidTransition(from, to string) bool {
	switch from {
	case "":
		return to == StepLoggedIn
	case StepLoggedIn:
		return to == StepCreatingChar || to == StepEnteringGame
	case StepCreatingChar:
		return to == StepLoggedIn || to == StepEnteringGame
	default:
		return false
	}
}
