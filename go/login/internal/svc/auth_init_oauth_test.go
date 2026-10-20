package svc

import (
	"fmt"
	"strings"
	"testing"

	"login/internal/config"
	"login/internal/logic/pkg/auth"
)

// 第三方 provider 的装配:密钥只来自 ResolveSecrets,unionid 策略按配置取反映射到 provider 的零值安全字段。
// 只测纯函数与「注册前就 panic」的路径,不往进程级 auth 注册表里写 wechat / qq(同名重复注册会 panic)。

func TestWeChatProviderFromConfigUsesResolvedSecretAndUnionIdPolicy(t *testing.T) {
	conf := &config.WeChatAuthConf{AppId: "wx0123456789abcdef", AppSecret: "inline-must-not-be-used",
		RequireUnionId: true, Endpoint: "https://wx.example.com"}
	p, err := weChatProviderFromConfig(conf, config.AuthProviderSecrets{WeChatAppSecret: "resolved-secret"})
	if err != nil {
		t.Fatalf("weChatProviderFromConfig: %v", err)
	}
	if p.AppSecret != "resolved-secret" {
		t.Fatal("provider 必须用 ResolveSecrets 解析出的密钥,不能读 yaml 里的内联值")
	}
	if p.AllowOpenIdFallback {
		t.Fatal("RequireUnionId=true 时不得允许回落 openid")
	}
	if p.AppId != conf.AppId || p.Endpoint != conf.Endpoint {
		t.Fatalf("AppId / Endpoint 未透传: %+v", p)
	}

	conf.RequireUnionId = false
	p, err = weChatProviderFromConfig(conf, config.AuthProviderSecrets{WeChatAppSecret: "resolved-secret"})
	if err != nil || !p.AllowOpenIdFallback {
		t.Fatalf("RequireUnionId=false 应允许回落 openid: provider=%+v err=%v", p, err)
	}
}

func TestWeChatProviderFromConfigRejectsUnresolvedSecret(t *testing.T) {
	conf := &config.WeChatAuthConf{AppId: "wx0123456789abcdef", AppSecret: "inline", RequireUnionId: true}
	if _, err := weChatProviderFromConfig(conf, config.AuthProviderSecrets{}); err == nil {
		t.Fatal("ResolveSecrets 没解析出密钥时必须拒绝,不能带空密钥或内联值上线")
	}
}

func TestQQProviderFromConfigMapsUnionIdSwitches(t *testing.T) {
	cases := []struct {
		request, require         bool
		wantDisable, wantAllowed bool
	}{
		{request: true, require: true, wantDisable: false, wantAllowed: false},
		{request: true, require: false, wantDisable: false, wantAllowed: true},
		{request: false, require: false, wantDisable: true, wantAllowed: true},
	}
	for _, tc := range cases {
		p := qqProviderFromConfig(&config.QQAuthConf{AppId: "101234567", RequestUnionId: tc.request, RequireUnionId: tc.require},
			config.AuthProviderSecrets{})
		if p.DisableUnionIdRequest != tc.wantDisable || p.AllowOpenIdFallback != tc.wantAllowed {
			t.Errorf("request=%v require=%v: got disable=%v allow=%v", tc.request, tc.require,
				p.DisableUnionIdRequest, p.AllowOpenIdFallback)
		}
	}
}

// 正常启动路径一定先 ResolveSecrets;漏了这一步时 InitAuthProviders 必须在注册前拒绝启动。
func TestInitAuthProvidersRejectsWeChatWithoutResolvedSecrets(t *testing.T) {
	previousConfig, previousSecrets := config.AppConfig, config.ActiveSecrets
	t.Cleanup(func() { config.AppConfig, config.ActiveSecrets = previousConfig, previousSecrets })

	config.AppConfig = config.Config{}
	config.AppConfig.AuthProviders.WeChat = &config.WeChatAuthConf{AppId: "wx0123456789abcdef",
		AppSecret: "inline-only", RequireUnionId: true}
	config.ActiveSecrets = nil

	defer func() {
		recovered := recover()
		if recovered == nil {
			t.Fatal("InitAuthProviders 在密钥未解析时注册了 wechat")
		}
		if !strings.Contains(fmt.Sprint(recovered), "ResolveSecrets") {
			t.Fatalf("panic = %v, want unresolved-secret validation", recovered)
		}
		if auth.Get("wechat") != nil {
			t.Fatal("拒绝启动前不得写入 auth 注册表")
		}
	}()
	InitAuthProviders()
}
