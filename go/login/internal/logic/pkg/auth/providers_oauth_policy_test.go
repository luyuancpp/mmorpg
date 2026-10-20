package auth

import (
	"context"
	"errors"
	"net/http"
	"strings"
	"testing"

	"github.com/zeromicro/go-zero/core/logx/logtest"
)

// 本文件钉住第三方认证的三条上线前约束:
//   - unionid 缺失默认 fail-closed(回落 openid 会把同一玩家分裂成多个账号);
//   - 渠道 / 本服务配置侧的失败标 ErrProviderUnavailable,玩家凭证无效不标(调用方据此选故障码还是业务拒绝);
//   - 应答先看状态码、再限长,成功日志不整段落 openid。

func serveOAuth(t *testing.T, path string, handler http.HandlerFunc) *http.Client {
	t.Helper()
	mux := http.NewServeMux()
	mux.HandleFunc(path, handler)
	cli, srv := newTestClient(mux)
	t.Cleanup(srv.Close)
	return cli
}

func writeBody(body string) http.HandlerFunc {
	return func(w http.ResponseWriter, _ *http.Request) { _, _ = w.Write([]byte(body)) }
}

func TestWeChatProviderMissingUnionIdFailsClosedByDefault(t *testing.T) {
	cli := serveOAuth(t, "/sns/oauth2/access_token", writeBody(`{"access_token":"AT","openid":"OPEN1"}`))
	p := &WeChatProvider{AppId: "wxid", AppSecret: "secret", HTTP: cli}
	res, err := p.Validate(context.Background(), "abc")
	if err == nil {
		t.Fatalf("缺 unionid 时应拒绝,实际得到账号 %q", res.Account)
	}
	if !errors.Is(err, ErrProviderUnavailable) {
		t.Fatalf("缺 unionid 是应用配置问题,应标 ErrProviderUnavailable,实际 %v", err)
	}
}

func TestQQProviderMissingUnionIdFailsClosedByDefault(t *testing.T) {
	cli := serveOAuth(t, "/oauth2.0/me", writeBody(`{"client_id":"10001","openid":"QQOPEN"}`))
	p := &QQProvider{AppId: "10001", HTTP: cli}
	if _, err := p.Validate(context.Background(), "AT"); !errors.Is(err, ErrProviderUnavailable) {
		t.Fatalf("缺 unionid 应拒绝并标 ErrProviderUnavailable,实际 %v", err)
	}
}

func TestQQProviderUnionIdRequestIsConfigurable(t *testing.T) {
	for _, tc := range []struct {
		name        string
		disable     bool
		wantUnionId string
	}{
		{name: "default requests unionid", disable: false, wantUnionId: "1"},
		{name: "disabled omits unionid", disable: true, wantUnionId: ""},
	} {
		t.Run(tc.name, func(t *testing.T) {
			var got string
			var present bool
			cli := serveOAuth(t, "/oauth2.0/me", func(w http.ResponseWriter, r *http.Request) {
				got = r.URL.Query().Get("unionid")
				_, present = r.URL.Query()["unionid"]
				_, _ = w.Write([]byte(`{"client_id":"10001","openid":"QQOPEN"}`))
			})
			p := &QQProvider{AppId: "10001", HTTP: cli, AllowOpenIdFallback: true, DisableUnionIdRequest: tc.disable}
			res, err := p.Validate(context.Background(), "AT")
			if err != nil {
				t.Fatalf("unexpected error: %v", err)
			}
			if res.Account != "qq_QQOPEN" {
				t.Fatalf("account = %q, want qq_QQOPEN", res.Account)
			}
			if got != tc.wantUnionId || present == tc.disable {
				t.Fatalf("unionid 参数 = %q(present=%v),want %q", got, present, tc.wantUnionId)
			}
		})
	}
}

func TestWeChatProviderErrCodeClassification(t *testing.T) {
	cases := []struct {
		errcode      string
		providerSide bool
	}{
		{errcode: "40029", providerSide: false}, // code 无效:玩家凭证问题
		{errcode: "40163", providerSide: false}, // code 已被使用
		{errcode: "42003", providerSide: false}, // code 超时
		{errcode: "-1", providerSide: true},     // 微信系统繁忙
		{errcode: "40125", providerSide: true},  // AppSecret 无效
		{errcode: "40013", providerSide: true},  // AppId 无效
		{errcode: "40164", providerSide: true},  // 出口 IP 不在白名单
		{errcode: "45011", providerSide: true},  // 调用频率超限
	}
	for _, tc := range cases {
		t.Run(tc.errcode, func(t *testing.T) {
			cli := serveOAuth(t, "/sns/oauth2/access_token", writeBody(`{"errcode":`+tc.errcode+`,"errmsg":"x"}`))
			p := &WeChatProvider{AppId: "wxid", AppSecret: "secret", HTTP: cli}
			_, err := p.Validate(context.Background(), "abc")
			if err == nil {
				t.Fatal("errcode 非 0 必须拒绝")
			}
			if got := errors.Is(err, ErrProviderUnavailable); got != tc.providerSide {
				t.Fatalf("errcode=%s ErrProviderUnavailable=%v,want %v(err=%v)", tc.errcode, got, tc.providerSide, err)
			}
		})
	}
}

func TestQQProviderPlayerSideRejectionsAreNotProviderFaults(t *testing.T) {
	for name, body := range map[string]string{
		"api error":          `{"error":100016,"error_description":"access_token check failed"}`,
		"client_id mismatch": `{"client_id":"OTHER","openid":"QQOPEN","unionid":"U"}`,
	} {
		t.Run(name, func(t *testing.T) {
			cli := serveOAuth(t, "/oauth2.0/me", writeBody(body))
			p := &QQProvider{AppId: "10001", HTTP: cli}
			_, err := p.Validate(context.Background(), "AT")
			if err == nil {
				t.Fatal("应拒绝")
			}
			if errors.Is(err, ErrProviderUnavailable) {
				t.Fatalf("玩家侧拒绝不应标 ErrProviderUnavailable:%v", err)
			}
		})
	}
}

// 非 2xx、超长应答、坏 JSON、errcode=0 却无 openid:都不是玩家凭证的问题。
func TestOAuthProvidersTransportAndShapeFailuresAreProviderFaults(t *testing.T) {
	oversized := `{"openid":"` + strings.Repeat("a", maxOAuthResponseBytes) + `"}`
	cases := map[string]http.HandlerFunc{
		"http 502": func(w http.ResponseWriter, _ *http.Request) {
			w.WriteHeader(http.StatusBadGateway)
			_, _ = w.Write([]byte(`{"openid":"OPEN1","unionid":"U","client_id":"10001"}`))
		},
		"http 429":        func(w http.ResponseWriter, _ *http.Request) { w.WriteHeader(http.StatusTooManyRequests) },
		"oversized body":  writeBody(oversized),
		"bad json":        writeBody(`<html>gateway error</html>`),
		"no openid in ok": writeBody(`{"client_id":"10001","unionid":"U"}`),
	}
	for name, handler := range cases {
		for _, providerName := range []string{"wechat", "qq"} {
			t.Run(providerName+"/"+name, func(t *testing.T) {
				var p Provider
				if providerName == "wechat" {
					p = &WeChatProvider{AppId: "wxid", AppSecret: "secret",
						HTTP: serveOAuth(t, "/sns/oauth2/access_token", handler)}
				} else {
					p = &QQProvider{AppId: "10001", HTTP: serveOAuth(t, "/oauth2.0/me", handler)}
				}
				_, err := p.Validate(context.Background(), "AT")
				if !errors.Is(err, ErrProviderUnavailable) {
					t.Fatalf("应标 ErrProviderUnavailable,实际 %v", err)
				}
			})
		}
	}
}

// 恰好 64 KiB 的应答不算超长(LimitReader 多读 1 字节的边界)。
func TestOAuthResponseAtLimitIsAccepted(t *testing.T) {
	prefix := `{"access_token":"AT","openid":"OPEN1","unionid":"UNION1","scope":"`
	suffix := `"}`
	body := prefix + strings.Repeat("s", maxOAuthResponseBytes-len(prefix)-len(suffix)) + suffix
	if len(body) != maxOAuthResponseBytes {
		t.Fatalf("测试数据长度 %d,want %d", len(body), maxOAuthResponseBytes)
	}
	cli := serveOAuth(t, "/sns/oauth2/access_token", writeBody(body))
	p := &WeChatProvider{AppId: "wxid", AppSecret: "secret", HTTP: cli}
	res, err := p.Validate(context.Background(), "abc")
	if err != nil {
		t.Fatalf("64 KiB 应答应被接受:%v", err)
	}
	if res.Account != "wx_UNION1" {
		t.Fatalf("account = %q, want wx_UNION1", res.Account)
	}
}

func TestOAuthSuccessLogMasksOpenId(t *testing.T) {
	const openid = "oPENID_full_value_0123456789"
	logs := logtest.NewCollector(t)
	cli := serveOAuth(t, "/sns/oauth2/access_token",
		writeBody(`{"access_token":"AT","openid":"`+openid+`","unionid":"UNION1"}`))
	p := &WeChatProvider{AppId: "wxid", AppSecret: "secret", HTTP: cli}
	if _, err := p.Validate(context.Background(), "abc"); err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	out := logs.String()
	if strings.Contains(out, openid) {
		t.Fatal("成功日志整段输出了 openid")
	}
	if !strings.Contains(out, "WeChat auth ok") || !strings.Contains(out, maskOAuthId(openid)) {
		t.Fatalf("成功日志缺少诊断信息:%s", out)
	}
}

func TestMaskOAuthId(t *testing.T) {
	cases := map[string]string{
		"":                 "***",
		"short":            "***",
		"12345678":         "***",
		"123456789":        "1234***89",
		"oABCDEFGHIJKLMNO": "oABC***NO",
	}
	for in, want := range cases {
		if got := maskOAuthId(in); got != want {
			t.Errorf("maskOAuthId(%q) = %q, want %q", in, got, want)
		}
	}
}
