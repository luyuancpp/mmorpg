package auth

import (
	"context"
	"fmt"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// PasswordProvider authenticates using the account/password fields directly.
// The account is passed as the token parameter (no external validation needed).
type PasswordProvider struct{}

func (p *PasswordProvider) Validate(_ context.Context, account string) (*AuthResult, error) {
	return &AuthResult{Account: account}, nil
}

// SaTokenProvider validates SA-Token via Redis lookup.
type SaTokenProvider struct {
	RedisClient *redis.Client
	TokenName   string // SA-Token key prefix (e.g. "satoken")
	LoginType   string // SA-Token login type (e.g. "login")
}

func (p *SaTokenProvider) Validate(ctx context.Context, tokenValue string) (*AuthResult, error) {
	key := fmt.Sprintf("%s:%s:token:%s", p.TokenName, p.LoginType, tokenValue)
	loginId, err := p.RedisClient.Get(ctx, key).Result()
	if err == redis.Nil {
		return nil, fmt.Errorf("sa-token not found or expired: %s", tokenValue)
	}
	if err != nil {
		return nil, fmt.Errorf("sa-token Redis lookup failed: %w", err)
	}
	if loginId == "" {
		return nil, fmt.Errorf("sa-token maps to empty loginId")
	}
	logx.Infof("SA-Token validated: token=%s -> account=%s", tokenValue, loginId)
	return &AuthResult{Account: loginId}, nil
}

// WeChatProvider validates WeChat OAuth code.
// TODO: implement when integrating WeChat login.
type WeChatProvider struct {
	AppId     string
	AppSecret string
}

func (p *WeChatProvider) Validate(_ context.Context, code string) (*AuthResult, error) {
	// TODO: call WeChat OAuth API: code -> access_token -> openid
	return nil, fmt.Errorf("wechat auth not implemented")
}

// QQProvider validates QQ OAuth code.
// TODO: implement when integrating QQ login.
type QQProvider struct {
	AppId  string
	AppKey string
}

func (p *QQProvider) Validate(_ context.Context, code string) (*AuthResult, error) {
	// TODO: call QQ OAuth API: code -> access_token -> openid
	return nil, fmt.Errorf("qq auth not implemented")
}

// NeteaseProvider validates NetEase login token.
// TODO: implement when integrating NetEase login.
type NeteaseProvider struct {
	AppKey    string
	AppSecret string
}

func (p *NeteaseProvider) Validate(_ context.Context, token string) (*AuthResult, error) {
	// TODO: call NetEase auth API to verify token
	return nil, fmt.Errorf("netease auth not implemented")
}
