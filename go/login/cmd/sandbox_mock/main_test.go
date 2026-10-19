package main

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"

	"github.com/zeromicro/go-zero/core/conf"

	"login/internal/config"
)

// linux 登录栈(deploy/login-stack.linux)的 login 指向本 mock:QQ AppId 必须等于 mock 返回的 client_id,
// 否则 QQProvider 每次都按 client_id mismatch 拒绝,沙箱端到端永远跑不通(2026-10-18 前就是这样)。
func TestLinuxLoginStackQQAppIdMatchesSandboxClientId(t *testing.T) {
	raw, err := os.ReadFile("../../../../deploy/login-stack.linux/login.yaml")
	if err != nil {
		t.Fatalf("read linux login stack config: %v", err)
	}
	var probe struct {
		AuthProviders config.AuthConfig `json:"AuthProviders,optional"`
	}
	if err := conf.LoadFromYamlBytes(raw, &probe); err != nil {
		t.Fatalf("load linux login stack config: %v", err)
	}
	if probe.AuthProviders.QQ == nil {
		t.Fatal("linux login stack config no longer configures QQ; update this test together with the stack")
	}
	if probe.AuthProviders.QQ.AppId != sandboxQQClientID {
		t.Fatalf("QQ.AppId = %q, want sandbox client_id %q", probe.AuthProviders.QQ.AppId, sandboxQQClientID)
	}
}

func TestQQHandlerReturnsSandboxClientId(t *testing.T) {
	rec := httptest.NewRecorder()
	handleQQTokenVerify(rec, httptest.NewRequest(http.MethodGet, "/oauth2.0/me?access_token=t1&unionid=1&fmt=json", nil))
	var body map[string]any
	if err := json.Unmarshal(rec.Body.Bytes(), &body); err != nil {
		t.Fatalf("decode: %v", err)
	}
	if body["client_id"] != sandboxQQClientID {
		t.Fatalf("client_id = %v, want %q", body["client_id"], sandboxQQClientID)
	}
}
