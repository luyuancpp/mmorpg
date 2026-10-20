package auth

import (
	"context"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"
	"testing"

	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"github.com/zeromicro/go-zero/core/logx/logtest"
)

func TestSaTokenProviderDoesNotLogOrReturnCredential(t *testing.T) {
	server := miniredis.RunT(t)
	client := redis.NewClient(&redis.Options{Addr: server.Addr()})
	t.Cleanup(func() { _ = client.Close() })
	provider := &SaTokenProvider{RedisClient: client, TokenName: "probe", LoginType: "login"}
	logs := logtest.NewCollector(t)
	const sentinel = "SYNTHETIC_SA_TOKEN"
	if _, err := provider.Validate(context.Background(), sentinel); err == nil {
		t.Fatal("不存在的令牌应拒绝")
	} else {
		logx.Errorf("Auth failed: %v", err)
	}
	server.Set("probe:login:token:"+sentinel, "probe_account")
	result, err := provider.Validate(context.Background(), sentinel)
	if err != nil || result.Account != "probe_account" {
		t.Fatal("有效令牌未完成原验证")
	}
	if strings.Contains(logs.String(), sentinel) {
		t.Fatal("SA-Token 凭证进入了成功或失败日志")
	}
	if !strings.Contains(logs.String(), "SA-Token validated") || !strings.Contains(logs.String(), "not found or expired") {
		t.Fatal("SA-Token 验证结果诊断丢失")
	}
}

type credentialProbeTransport func(*http.Request) (*http.Response, error)

func (f credentialProbeTransport) RoundTrip(req *http.Request) (*http.Response, error) { return f(req) }

func TestOAuthProviderErrorsDoNotExposeCredentials(t *testing.T) {
	const sentinel = "SYNTHETIC_OAUTH_CREDENTIAL"
	for _, providerName := range []string{"wechat", "qq"} {
		for _, scenario := range []string{"http", "build", "body", "json", "provider", "client_id", "cancel", "deadline"} {
			t.Run(providerName+"/"+scenario, func(t *testing.T) {
				logs := logtest.NewCollector(t)
				client := &http.Client{Transport: credentialProbeTransport(func(*http.Request) (*http.Response, error) {
					if scenario == "http" {
						return nil, errors.New("transport echoed " + sentinel)
					}
					if scenario == "cancel" {
						return nil, context.Canceled
					}
					if scenario == "deadline" {
						return nil, context.DeadlineExceeded
					}
					var body io.ReadCloser = io.NopCloser(strings.NewReader("invalid " + sentinel))
					switch scenario {
					case "body":
						body = &credentialProbeBody{sentinel: sentinel}
					case "provider":
						if providerName == "wechat" {
							body = io.NopCloser(strings.NewReader(fmt.Sprintf(`{"errcode":40029,"errmsg":%q}`, sentinel)))
						} else {
							body = io.NopCloser(strings.NewReader(fmt.Sprintf(`{"error":100016,"error_description":%q}`, sentinel)))
						}
					case "client_id":
						if providerName == "wechat" {
							body = io.NopCloser(strings.NewReader(fmt.Sprintf(`{"errcode":40029,"errmsg":%q}`, sentinel)))
						} else {
							body = io.NopCloser(strings.NewReader(fmt.Sprintf(`{"client_id":%q,"openid":"probe"}`, sentinel)))
						}
					}
					return &http.Response{StatusCode: http.StatusOK, Body: body, Header: make(http.Header)}, nil
				})}
				endpoint := ""
				if scenario == "build" {
					endpoint = "://" + sentinel
				}
				var provider Provider
				if providerName == "wechat" {
					provider = &WeChatProvider{AppId: "probe", AppSecret: sentinel, HTTP: client, Endpoint: endpoint}
				} else {
					provider = &QQProvider{AppId: "probe", HTTP: client, Endpoint: endpoint}
				}
				_, err := provider.Validate(context.Background(), sentinel)
				if err == nil {
					t.Fatal("故障响应应拒绝验证")
				}
				if strings.Contains(err.Error(), sentinel) {
					t.Fatal("OAuth 返回错误泄露凭证")
				}
				logx.Errorf("Auth failed: %v", err)
				if strings.Contains(logs.String(), sentinel) {
					t.Fatal("OAuth 调用者日志泄露凭证")
				}
				if !strings.Contains(logs.String(), providerName) {
					t.Fatal("OAuth 固定提供者诊断丢失")
				}
				if scenario == "cancel" && !errors.Is(err, context.Canceled) {
					t.Fatal("取消语义丢失")
				}
				if scenario == "deadline" && !errors.Is(err, context.DeadlineExceeded) {
					t.Fatal("超时语义丢失")
				}
			})
		}
	}
}

type credentialProbeBody struct{ sentinel string }

func (b *credentialProbeBody) Read([]byte) (int, error) {
	return 0, errors.New("body echoed " + b.sentinel)
}
func (*credentialProbeBody) Close() error { return nil }
