// Package sessionmanager replaces Centre's in-memory session management.
// All state is stored in Redis (no single point of failure).
// It handles: login decision (first/reconnect/replace), idempotency, session lifecycle.
package sessionmanager

import (
	"context"
	"encoding/json"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// PlayerSession mirrors the proto PlayerSession but lives in Redis.
type PlayerSession struct {
	PlayerID       uint64 `json:"player_id"`
	SessionID      uint64 `json:"session_id"`
	GateID         string `json:"gate_id"`
	GateInstanceID string `json:"gate_instance_id"`
	SceneNodeID    string `json:"scene_node_id"`
	SceneID        uint64 `json:"scene_id"`
	Account        string `json:"account"` // account name (identity for reconnect decisions)
	SessionVersion uint64 `json:"session_version"`
	State          int32  `json:"state"` // 0=unknown, 1=online, 2=disconnecting, 3=offline
	LastActiveTs   int64  `json:"last_active_ts"`
	RequestID      string `json:"request_id"`
}

// EnterGameDecision mirrors Centre's enum.
type EnterGameDecision int

const (
	FirstLogin     EnterGameDecision = 0
	ShortReconnect EnterGameDecision = 1
	ReplaceLogin   EnterGameDecision = 2
)

const (
	StateOnline        int32 = 1
	StateDisconnecting int32 = 2
	StateOffline       int32 = 3

	sessionKeyPrefix     = "player_session:"
	idempotencyKeyPrefix = "login_idempotent:"
	idempotencyTTL       = 5 * time.Minute
	disconnectLeaseTTL   = 30 * time.Second
)

func sessionKey(playerID uint64) string {
	return fmt.Sprintf("%s%d", sessionKeyPrefix, playerID)
}

func idempotencyKey(playerID uint64, requestID string) string {
	return fmt.Sprintf("%s%d:%s", idempotencyKeyPrefix, playerID, requestID)
}

// GetSession retrieves the player session from Redis. Returns nil if not found.
func GetSession(ctx context.Context, rdb *redis.Client, playerID uint64) (*PlayerSession, error) {
	key := sessionKey(playerID)
	data, err := rdb.Get(ctx, key).Bytes()
	if err == redis.Nil {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	// We store as protobuf-friendly JSON, but let's use a simple proto-compatible approach.
	// Actually, let's store as protobuf binary for consistency with the rest of the codebase.
	// We'll use a generated proto message eventually, but for now use a simple approach.
	session := &PlayerSession{}
	if err := unmarshalSession(data, session); err != nil {
		return nil, err
	}
	return session, nil
}

// SetSession writes/overwrites a player session in Redis (no TTL — online sessions are permanent).
func SetSession(ctx context.Context, rdb *redis.Client, session *PlayerSession) error {
	key := sessionKey(session.PlayerID)
	data, err := marshalSession(session)
	if err != nil {
		return err
	}
	return rdb.Set(ctx, key, data, 0).Err()
}

// SetSessionDisconnecting marks the session as disconnecting with a TTL lease.
// When the TTL expires, Redis key is auto-deleted (effectively = lease expired).
func SetSessionDisconnecting(ctx context.Context, rdb *redis.Client, playerID uint64, sessionID uint64) error {
	session, err := GetSession(ctx, rdb, playerID)
	if err != nil || session == nil {
		return err
	}
	// Only the current session can start disconnect
	if session.SessionID != sessionID {
		logx.Infof("SetSessionDisconnecting: session mismatch player=%d current=%d requested=%d, ignoring",
			playerID, session.SessionID, sessionID)
		return nil
	}
	session.State = StateDisconnecting
	data, err := marshalSession(session)
	if err != nil {
		return err
	}
	// Set with TTL — if not reconnected, key auto-expires
	return rdb.Set(ctx, sessionKey(playerID), data, disconnectLeaseTTL).Err()
}

// Reconnect cancels the disconnect lease and restores the session to online.
func Reconnect(ctx context.Context, rdb *redis.Client, playerID uint64,
	newSessionID uint64, gateID string, gateInstanceID string,
	account string, requestID string,
) (*PlayerSession, error) {
	session, err := GetSession(ctx, rdb, playerID)
	if err != nil {
		return nil, err
	}
	if session == nil {
		return nil, fmt.Errorf("no session found for player %d", playerID)
	}

	session.SessionID = newSessionID
	session.GateID = gateID
	session.GateInstanceID = gateInstanceID
	session.Account = account
	session.SessionVersion++
	session.State = StateOnline
	session.LastActiveTs = time.Now().UnixMilli()
	session.RequestID = requestID

	if err := SetSession(ctx, rdb, session); err != nil {
		return nil, err
	}
	return session, nil
}

// DeleteSession removes the session (player fully logged off).
func DeleteSession(ctx context.Context, rdb *redis.Client, playerID uint64) error {
	return rdb.Del(ctx, sessionKey(playerID)).Err()
}

// DecideEnterGame implements Centre's login decision logic.
// Same account reconnecting within the disconnect lease window → ShortReconnect.
// Different account or no lease → ReplaceLogin.
func DecideEnterGame(existing *PlayerSession, incomingAccount string) EnterGameDecision {
	if existing == nil {
		return FirstLogin
	}

	// Reconnect only if: session is in disconnecting state AND same account
	if existing.State == StateDisconnecting && existing.Account == incomingAccount {
		return ShortReconnect
	}

	return ReplaceLogin
}

// CheckIdempotency checks if a request_id was already processed for a player.
func CheckIdempotency(ctx context.Context, rdb *redis.Client, playerID uint64, requestID string) (bool, error) {
	if requestID == "" {
		return false, nil
	}
	key := idempotencyKey(playerID, requestID)
	exists, err := rdb.Exists(ctx, key).Result()
	if err != nil {
		return false, err
	}
	return exists > 0, nil
}

// SetIdempotency marks a request_id as processed.
func SetIdempotency(ctx context.Context, rdb *redis.Client, playerID uint64, requestID string) error {
	if requestID == "" {
		return nil
	}
	key := idempotencyKey(playerID, requestID)
	return rdb.Set(ctx, key, "1", idempotencyTTL).Err()
}

// marshalSession serialises a PlayerSession to bytes using JSON.
func marshalSession(s *PlayerSession) ([]byte, error) {
	return json.Marshal(s)
}

func unmarshalSession(data []byte, s *PlayerSession) error {
	return json.Unmarshal(data, s)
}
