// Package token manages access/refresh token lifecycle in Redis.
//
// Token architecture (WeChat/QQ game login style):
//
//	Access Token:  opaque, 2h TTL, used for reconnect without re-auth from third-party.
//	Refresh Token: opaque, 30d TTL, used to obtain new access token after expiry.
//
// Redis keys:
//
//	access_token:{token}   → JSON{account, auth_type, device_id, created_at}   TTL=AccessTokenTTL
//	refresh_token:{token}  → JSON{account, auth_type, device_id, created_at}   TTL=RefreshTokenTTL
//	account_refresh:{account} → SET of active refresh tokens (for bulk revocation)
package token

import (
	"context"
	"crypto/rand"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

const (
	accessTokenPrefix  = "access_token:"
	refreshTokenPrefix = "refresh_token:"
	accountRefreshKey  = "account_refresh:"

	tokenByteLen = 32 // 256-bit random, base64url encoded → 43 chars
)

// Config holds token TTL settings.
type Config struct {
	AccessTokenTTL  time.Duration // e.g. 2h
	RefreshTokenTTL time.Duration // e.g. 30d (720h)
}

// TokenData is the payload stored in Redis for each token.
type TokenData struct {
	Account   string `json:"account"`
	AuthType  string `json:"auth_type"`
	DeviceID  string `json:"device_id,omitempty"`
	CreatedAt int64  `json:"created_at"`
}

// TokenPair is returned after issuing or refreshing tokens.
type TokenPair struct {
	AccessToken        string
	RefreshToken       string
	AccessTokenExpire  int64 // unix seconds
	RefreshTokenExpire int64 // unix seconds
}

// Manager handles token CRUD operations.
type Manager struct {
	rdb *redis.Client
	cfg Config
}

// NewManager creates a token manager.
func NewManager(rdb *redis.Client, cfg Config) *Manager {
	return &Manager{rdb: rdb, cfg: cfg}
}

// Issue generates a new access+refresh token pair and stores them in Redis.
func (m *Manager) Issue(ctx context.Context, account, authType, deviceID string) (*TokenPair, error) {
	now := time.Now()
	data := &TokenData{
		Account:   account,
		AuthType:  authType,
		DeviceID:  deviceID,
		CreatedAt: now.Unix(),
	}

	accessToken, err := generateToken()
	if err != nil {
		return nil, fmt.Errorf("generate access token: %w", err)
	}
	refreshToken, err := generateToken()
	if err != nil {
		return nil, fmt.Errorf("generate refresh token: %w", err)
	}

	dataBytes, err := json.Marshal(data)
	if err != nil {
		return nil, fmt.Errorf("marshal token data: %w", err)
	}

	accessExpire := now.Add(m.cfg.AccessTokenTTL)
	refreshExpire := now.Add(m.cfg.RefreshTokenTTL)

	pipe := m.rdb.TxPipeline()
	pipe.Set(ctx, accessTokenPrefix+accessToken, dataBytes, m.cfg.AccessTokenTTL)
	pipe.Set(ctx, refreshTokenPrefix+refreshToken, dataBytes, m.cfg.RefreshTokenTTL)
	pipe.SAdd(ctx, accountRefreshKey+account, refreshToken)
	pipe.Expire(ctx, accountRefreshKey+account, m.cfg.RefreshTokenTTL)
	if _, err := pipe.Exec(ctx); err != nil {
		return nil, fmt.Errorf("store tokens in Redis: %w", err)
	}

	logx.Infof("[Token] Issued tokens for account=%s auth_type=%s access_ttl=%v refresh_ttl=%v",
		account, authType, m.cfg.AccessTokenTTL, m.cfg.RefreshTokenTTL)

	return &TokenPair{
		AccessToken:        accessToken,
		RefreshToken:       refreshToken,
		AccessTokenExpire:  accessExpire.Unix(),
		RefreshTokenExpire: refreshExpire.Unix(),
	}, nil
}

// ValidateAccess validates an access token, returns the account if valid.
func (m *Manager) ValidateAccess(ctx context.Context, accessToken string) (*TokenData, error) {
	return m.validate(ctx, accessTokenPrefix+accessToken)
}

// Refresh validates a refresh token, invalidates it, and issues a new token pair (rotation).
func (m *Manager) Refresh(ctx context.Context, refreshToken string) (*TokenPair, error) {
	key := refreshTokenPrefix + refreshToken
	data, err := m.validate(ctx, key)
	if err != nil {
		return nil, err
	}

	// Atomically delete old refresh token (one-time use)
	deleted, err := m.rdb.Del(ctx, key).Result()
	if err != nil {
		return nil, fmt.Errorf("delete old refresh token: %w", err)
	}
	if deleted == 0 {
		// Race: another request already consumed this token
		return nil, fmt.Errorf("refresh token already consumed")
	}

	// Remove from account set
	m.rdb.SRem(ctx, accountRefreshKey+data.Account, refreshToken)

	// Issue new pair
	return m.Issue(ctx, data.Account, data.AuthType, data.DeviceID)
}

// RevokeAll revokes all refresh tokens for an account (e.g. password change).
func (m *Manager) RevokeAll(ctx context.Context, account string) error {
	setKey := accountRefreshKey + account
	tokens, err := m.rdb.SMembers(ctx, setKey).Result()
	if err != nil {
		return fmt.Errorf("list refresh tokens: %w", err)
	}

	if len(tokens) > 0 {
		keys := make([]string, 0, len(tokens)+1)
		for _, t := range tokens {
			keys = append(keys, refreshTokenPrefix+t)
		}
		keys = append(keys, setKey)
		m.rdb.Del(ctx, keys...)
	}

	logx.Infof("[Token] Revoked all refresh tokens for account=%s count=%d", account, len(tokens))
	return nil
}

func (m *Manager) validate(ctx context.Context, key string) (*TokenData, error) {
	val, err := m.rdb.Get(ctx, key).Bytes()
	if err == redis.Nil {
		return nil, fmt.Errorf("token not found or expired")
	}
	if err != nil {
		return nil, fmt.Errorf("Redis lookup: %w", err)
	}

	var data TokenData
	if err := json.Unmarshal(val, &data); err != nil {
		return nil, fmt.Errorf("unmarshal token data: %w", err)
	}
	return &data, nil
}

func generateToken() (string, error) {
	b := make([]byte, tokenByteLen)
	if _, err := rand.Read(b); err != nil {
		return "", err
	}
	return base64.RawURLEncoding.EncodeToString(b), nil
}
