package clientplayerloginlogic

import (
	"context"
	"fmt"
	"strings"
	"sync/atomic"
	"testing"

	"login/internal/logic/pkg/auth"
	login_proto_common "proto/common/base"
)

// TestShouldRejectLegacyRequest 逐格钉住 ARCH §12 T+2 kill switch 的真值表。
// 把 and/or 写反,要么旧路径悄悄一直开着(运维看不出来),要么连新流程一起拒掉(全服进不了游戏)——
// 后者正是 gate 上 access_token 绑定曾被误判为旧路径的事故形态。
//
// 用例名直接写期望,CI 失败信息自解释,不必回头数第几行。
func TestShouldRejectLegacyRequest(t *testing.T) {
	cases := []struct {
		name        string
		path        loginPath
		flagEnabled bool
		want        bool
	}{
		{
			name:        "new HTTP path is never rejected by the legacy kill switch",
			path:        loginPathHTTP,
			flagEnabled: true,
			want:        false,
		},
		{
			name:        "new HTTP path stays allowed even when flag is off",
			path:        loginPathHTTP,
			flagEnabled: false,
			want:        false,
		},
		{
			name:        "gate access_token session bind served while flag default-on",
			path:        loginPathGateBind,
			flagEnabled: true,
			want:        false,
		},
		{
			name:        "gate access_token session bind stays allowed when flag is off (new flow depends on it)",
			path:        loginPathGateBind,
			flagEnabled: false,
			want:        false,
		},
		{
			name:        "legacy gate credential login served while flag default-on",
			path:        loginPathGateLegacy,
			flagEnabled: true,
			want:        false,
		},
		{
			name:        "legacy gate credential login rejected once ops flips flag off (T+2)",
			path:        loginPathGateLegacy,
			flagEnabled: false,
			want:        true,
		},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := shouldRejectLegacyRequest(tc.path, tc.flagEnabled); got != tc.want {
				t.Errorf("shouldRejectLegacyRequest(path=%s, flagEnabled=%v) = %v, want %v",
					tc.path.metricLabel(), tc.flagEnabled, got, tc.want)
			}
		})
	}
}

// TestClassifyLoginPath 钉住入口分类:只有 gate 上的 auth_type=access_token 是会话绑定,
// gate 上其余一切(口令的两种拼法、第三方、未知类型、大小写不符)都归旧路径。
func TestClassifyLoginPath(t *testing.T) {
	gate := &login_proto_common.SessionDetails{SessionId: 7101}
	cases := []struct {
		name     string
		details  *login_proto_common.SessionDetails
		found    bool
		authType string
		want     loginPath
	}{
		{name: "no session header is the HTTP entry", details: nil, found: false, authType: "password", want: loginPathHTTP},
		{name: "nil session header is the HTTP entry", details: nil, found: true, authType: "wechat", want: loginPathHTTP},
		{name: "session id 0 is the HTTP entry", details: &login_proto_common.SessionDetails{}, found: true, authType: "", want: loginPathHTTP},
		{name: "HTTP access_token re-auth stays HTTP", details: nil, found: false, authType: "access_token", want: loginPathHTTP},
		{name: "gate access_token is the session bind", details: gate, found: true, authType: "access_token", want: loginPathGateBind},
		{name: "gate empty auth_type is legacy password", details: gate, found: true, authType: "", want: loginPathGateLegacy},
		{name: "gate password is legacy", details: gate, found: true, authType: "password", want: loginPathGateLegacy},
		{name: "gate wechat first auth is legacy", details: gate, found: true, authType: "wechat", want: loginPathGateLegacy},
		{name: "gate qq first auth is legacy", details: gate, found: true, authType: "qq", want: loginPathGateLegacy},
		{name: "gate satoken first auth is legacy", details: gate, found: true, authType: "satoken", want: loginPathGateLegacy},
		{name: "gate access_token match is case sensitive", details: gate, found: true, authType: "ACCESS_TOKEN", want: loginPathGateLegacy},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := classifyLoginPath(tc.details, tc.found, tc.authType); got != tc.want {
				t.Errorf("classifyLoginPath(auth_type=%q) = %s, want %s", tc.authType, got.metricLabel(), tc.want.metricLabel())
			}
		})
	}
}

// TestLoginPathMetricLabels 钉住 login_auth_path_total 的 path 取值:面板与告警按字面量查询,
// 改名等于让 ARCH §12 的退出判据静默失效。
func TestLoginPathMetricLabels(t *testing.T) {
	for path, want := range map[loginPath]string{
		loginPathHTTP:       "new",
		loginPathGateBind:   "gate_bind",
		loginPathGateLegacy: "legacy",
	} {
		if got := path.metricLabel(); got != want {
			t.Errorf("loginPath(%d).metricLabel() = %q, want %q", path, got, want)
		}
	}
}

// labelTestAuthSequence 给本文件注册的认证器取不重名的名字:注册表重名即 panic、没有注销接口,
// 固定名字在 -count=2 时会撞。
var labelTestAuthSequence atomic.Uint64

type labelTestProvider struct{}

func (labelTestProvider) Validate(context.Context, string) (*auth.AuthResult, error) {
	return &auth.AuthResult{Account: "label-test"}, nil
}

// TestAuthTypeMetricLabel 钉住 auth_type 的收敛规则。弃用遥测与 kill switch 在认证之前,auth_type
// 还是未认证的客户端输入;任何一条「原样透传」的漏网分支都会让 login_auth_path_total 的基数没有上限。
func TestAuthTypeMetricLabel(t *testing.T) {
	registered := fmt.Sprintf("label-test-registered-%d", labelTestAuthSequence.Add(1))
	auth.Register(registered, labelTestProvider{})

	cases := []struct {
		name     string
		authType string
		want     string
	}{
		{name: "empty is the legacy spelling of password", authType: "", want: "password"},
		{name: "password is kept even without a registered password authenticator", authType: "password", want: "password"},
		{name: "access_token is kept", authType: "access_token", want: "access_token"},
		{name: "registered provider name is kept", authType: registered, want: registered},
		{name: "unregistered name collapses to unknown", authType: "no-such-provider", want: "unknown"},
		{name: "case mismatch of access_token collapses to unknown", authType: "ACCESS_TOKEN", want: "unknown"},
		{name: "case mismatch of password collapses to unknown", authType: "Password", want: "unknown"},
		{name: "case mismatch of a registered name collapses to unknown", authType: strings.ToUpper(registered), want: "unknown"},
		{name: "registered name with trailing garbage collapses to unknown", authType: registered + "\n", want: "unknown"},
		{name: "oversized input collapses to unknown", authType: strings.Repeat("x", 4096), want: "unknown"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := authTypeMetricLabel(tc.authType); got != tc.want {
				t.Errorf("authTypeMetricLabel(%.40q) = %.40q, want %q", tc.authType, got, tc.want)
			}
		})
	}
}

// TestAuthTypeMetricLabelIsBounded 用一批互不相同的未注册取值打收敛函数:不管客户端送多少种字符串,
// 落到 label 上的只有 unknown 这一个值。
func TestAuthTypeMetricLabelIsBounded(t *testing.T) {
	seen := map[string]struct{}{}
	for i := 0; i < 1000; i++ {
		seen[authTypeMetricLabel(fmt.Sprintf("client-chosen-%d", i))] = struct{}{}
	}
	if len(seen) != 1 {
		t.Fatalf("1000 distinct unregistered auth_type values produced %d labels %v, want only %q",
			len(seen), seen, authTypeLabelUnknown)
	}
	if _, ok := seen[authTypeLabelUnknown]; !ok {
		t.Fatalf("labels = %v, want only %q", seen, authTypeLabelUnknown)
	}
}
