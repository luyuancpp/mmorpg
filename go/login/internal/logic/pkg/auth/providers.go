package auth

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"regexp"
	"strings"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"

	"login/internal/logic/pkg/token"
)

// defaultHTTPTimeout is used by third-party OAuth Providers when no client is injected.
const defaultHTTPTimeout = 5 * time.Second

func httpClientOrDefault(c *http.Client) *http.Client {
	if c != nil {
		return c
	}
	return &http.Client{Timeout: defaultHTTPTimeout}
}

// PasswordProvider authenticates using the account/password fields directly.
// The account is passed as the token parameter (no external validation needed).
type PasswordProvider struct{}

func (p *PasswordProvider) Validate(_ context.Context, account string) (*AuthResult, error) {
	return &AuthResult{Account: account}, nil
}

// AccessTokenProvider validates our own access token via Redis.
// Used for reconnect: client sends access_token instead of third-party credential.
type AccessTokenProvider struct {
	TokenManager *token.Manager
}

func (p *AccessTokenProvider) Validate(ctx context.Context, accessToken string) (*AuthResult, error) {
	data, err := p.TokenManager.ValidateAccess(ctx, accessToken)
	if err != nil {
		return nil, fmt.Errorf("access token invalid: %w", err)
	}
	logx.Infof("Access token validated: account=%s auth_type=%s", data.Account, data.AuthType)
	return &AuthResult{Account: data.Account}, nil
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

// WeChatProvider validates a WeChat OAuth2 authorization code.
//
// Flow (already done by the client SDK before calling us):
//   1. Client SDK pulls up WeChat auth UI (SendAuth.Req, scope=snsapi_userinfo).
//   2. WeChat returns a short-lived (~5min) `code` to the client.
//   3. Client sends LoginRequest{auth_type:"wechat", auth_token: code}.
//
// We exchange `code` for openid/unionid via the official endpoint:
//   GET https://api.weixin.qq.com/sns/oauth2/access_token
//
// Account key uses unionid when available (so the same player keeps the
// same account across multiple apps under the same WeChat Open Platform
// subject), otherwise falls back to openid.
type WeChatProvider struct {
	AppId     string
	AppSecret string
	HTTP      *http.Client // optional, injected for tests; nil -> default 5s client
}

type wechatTokenResp struct {
	AccessToken  string `json:"access_token"`
	ExpiresIn    int    `json:"expires_in"`
	RefreshToken string `json:"refresh_token"`
	OpenId       string `json:"openid"`
	Scope        string `json:"scope"`
	UnionId      string `json:"unionid,omitempty"`
	ErrCode      int    `json:"errcode,omitempty"`
	ErrMsg       string `json:"errmsg,omitempty"`
}

func (p *WeChatProvider) Validate(ctx context.Context, code string) (*AuthResult, error) {
	if code == "" {
		return nil, fmt.Errorf("wechat: empty code")
	}
	if p.AppId == "" || p.AppSecret == "" {
		return nil, fmt.Errorf("wechat: provider not configured (missing AppId/AppSecret)")
	}

	endpoint := "https://api.weixin.qq.com/sns/oauth2/access_token?" + url.Values{
		"appid":      {p.AppId},
		"secret":     {p.AppSecret},
		"code":       {code},
		"grant_type": {"authorization_code"},
	}.Encode()

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return nil, fmt.Errorf("wechat: build request: %w", err)
	}
	resp, err := httpClientOrDefault(p.HTTP).Do(req)
	if err != nil {
		return nil, fmt.Errorf("wechat: http error: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("wechat: read body: %w", err)
	}
	var r wechatTokenResp
	if err := json.Unmarshal(body, &r); err != nil {
		return nil, fmt.Errorf("wechat: bad json: %w (body=%s)", err, body)
	}
	if r.ErrCode != 0 {
		return nil, fmt.Errorf("wechat: errcode=%d msg=%s", r.ErrCode, r.ErrMsg)
	}
	if r.OpenId == "" {
		return nil, fmt.Errorf("wechat: empty openid")
	}

	id := r.UnionId
	if id == "" {
		id = r.OpenId
	}
	account := "wx_" + id
	logx.Infof("WeChat auth ok: account=%s openid=%s union=%v", account, r.OpenId, r.UnionId != "")
	return &AuthResult{Account: account}, nil
}

// QQProvider validates a QQ Connect access_token (NOT a code).
//
// Flow (already done by the client SDK before calling us):
//   1. Client SDK pulls up QQ auth UI (Tencent.login).
//   2. QQ returns access_token + openid to the client.
//   3. Client sends LoginRequest{auth_type:"qq", auth_token: access_token}.
//
// We verify the token + extract openid/unionid via:
//   GET https://graph.qq.com/oauth2.0/me?access_token=...&unionid=1&fmt=json
//
// We never trust openid sent by the client — we always re-derive it from
// the token. ClientId in the response must equal our AppId.
type QQProvider struct {
	AppId  string
	AppKey string        // Reserved for future server-side WebOAuth code->token exchange; unused here.
	HTTP   *http.Client  // optional, injected for tests
}

type qqMeResp struct {
	ClientId  string `json:"client_id"`
	OpenId    string `json:"openid"`
	UnionId   string `json:"unionid,omitempty"`
	Error     int    `json:"error,omitempty"`
	ErrorDesc string `json:"error_description,omitempty"`
}

// Old QQ endpoints return JSONP: `callback({...});`. With fmt=json this
// should be plain JSON, but we still strip the wrapper defensively.
var qqJsonpRe = regexp.MustCompile(`(?s)callback\s*\(\s*(\{.*\})\s*\)\s*;?`)

func (p *QQProvider) Validate(ctx context.Context, accessToken string) (*AuthResult, error) {
	if accessToken == "" {
		return nil, fmt.Errorf("qq: empty access_token")
	}
	if p.AppId == "" {
		return nil, fmt.Errorf("qq: provider not configured (missing AppId)")
	}

	endpoint := "https://graph.qq.com/oauth2.0/me?" + url.Values{
		"access_token": {accessToken},
		"unionid":      {"1"},
		"fmt":          {"json"},
	}.Encode()

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, endpoint, nil)
	if err != nil {
		return nil, fmt.Errorf("qq: build request: %w", err)
	}
	resp, err := httpClientOrDefault(p.HTTP).Do(req)
	if err != nil {
		return nil, fmt.Errorf("qq: http error: %w", err)
	}
	defer resp.Body.Close()

	raw, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("qq: read body: %w", err)
	}
	body := strings.TrimSpace(string(raw))
	if m := qqJsonpRe.FindStringSubmatch(body); len(m) == 2 {
		body = m[1]
	}

	var r qqMeResp
	if err := json.Unmarshal([]byte(body), &r); err != nil {
		return nil, fmt.Errorf("qq: bad json: %w (body=%s)", err, body)
	}
	if r.Error != 0 {
		return nil, fmt.Errorf("qq: error=%d desc=%s", r.Error, r.ErrorDesc)
	}
	if r.OpenId == "" {
		return nil, fmt.Errorf("qq: empty openid")
	}
	if r.ClientId != p.AppId {
		return nil, fmt.Errorf("qq: client_id mismatch: got=%s want=%s", r.ClientId, p.AppId)
	}

	id := r.UnionId
	if id == "" {
		id = r.OpenId
	}
	account := "qq_" + id
	logx.Infof("QQ auth ok: account=%s openid=%s union=%v", account, r.OpenId, r.UnionId != "")
	return &AuthResult{Account: account}, nil
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
