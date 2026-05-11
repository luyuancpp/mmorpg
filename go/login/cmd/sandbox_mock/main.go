// Sandbox mock for WeChat Open Platform / QQ Connect.
//
// What this does:
//   Stands up a single HTTP server that mimics the two exact endpoints
//   the WeChatProvider / QQProvider call:
//
//     GET /sns/oauth2/access_token   (WeChat)
//     GET /oauth2.0/me               (QQ)
//
//   The response shape is byte-for-byte the same as api.weixin.qq.com /
//   graph.qq.com would return. The data is deterministic, derived from
//   the `code` / `access_token` query parameter, so the same input always
//   maps to the same openid / unionid — useful for verifying that the
//   provider correctly resolves "wx_<unionid>" account keys and that the
//   account-reuse path works without flipping accounts between runs.
//
// Why we need it (and why the existing httptest in providers_test.go
// doesn't suffice):
//   - httptest is per-test-function: it can't be reached by a separate
//     binary running `Validate` against a real go-zero login.rpc.
//   - The real e2e validation requires:
//        client -> Java Gateway POST /api/login {auth_type:"wechat"}
//             -> gRPC login -> WeChatProvider.Validate -> HTTP GET
//             -> some mocked api.weixin.qq.com -> openid/unionid
//             -> account key derived -> tokens issued -> JSON response.
//     The single missing link in network-restricted environments is the
//     "some mocked api.weixin.qq.com" hop. This binary fills it.
//
// Pointing the provider at this mock:
//   In go/login/etc/login.yaml:
//     AuthProviders:
//       WeChat:
//         AppId: "wx_sandbox_test"
//         AppSecret: "any_string_ok_here"
//         Endpoint: "http://127.0.0.1:18090"
//       QQ:
//         AppId: "100000001"
//         AppKey: "ignored"
//         Endpoint: "http://127.0.0.1:18090"
//
// Crafting deterministic codes:
//   For WeChat, the `code` query parameter is passed straight through to
//   build the openid: `openid = "openid_" + sha256(code)[:12]`. unionid
//   is derived the same way but with a different suffix, AND only emitted
//   when the code starts with "U" (mirrors the real "no unionid until the
//   app is bound to an Open Platform subject" quirk).
//
//   For QQ, the `access_token` query parameter drives both openid and
//   unionid via the same hash trick. The mock always sets unionid since
//   we passed `unionid=1` on the wire.
//
// Running:
//   go run ./cmd/sandbox_mock                    # default :18090
//   go run ./cmd/sandbox_mock -addr :19091       # override
//
// Failure-mode testing:
//   The mock returns errcode=40029 when code == "EXPIRED" so ops can
//   exercise the "expired code" branch end-to-end without waiting 5 min.
//   QQ error mode: pass access_token == "INVALID" -> error response.
package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net/http"
	"strings"
	"time"
)

func main() {
	addr := flag.String("addr", ":18090", "listen address; defaults to :18090 to match docs/ops/wechat-qq-sandbox-runbook.md examples")
	flag.Parse()

	mux := http.NewServeMux()
	mux.HandleFunc("/sns/oauth2/access_token", handleWeChatTokenExchange)
	mux.HandleFunc("/oauth2.0/me", handleQQTokenVerify)
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, _ *http.Request) {
		_, _ = w.Write([]byte("ok"))
	})

	log.Printf("sandbox mock listening on %s — point WeChat/QQ providers' Endpoint at http://127.0.0.1%s", *addr, *addr)
	log.Printf("  /sns/oauth2/access_token  (WeChat) — code starting with 'U' returns unionid, code 'EXPIRED' returns errcode 40029")
	log.Printf("  /oauth2.0/me              (QQ)     — access_token 'INVALID' returns error response")

	srv := &http.Server{
		Addr:              *addr,
		Handler:           mux,
		ReadHeaderTimeout: 5 * time.Second,
	}
	if err := srv.ListenAndServe(); err != nil {
		log.Fatalf("sandbox mock: %v", err)
	}
}

// hashTo12 derives a stable 12-char hex string from input. Used so the
// same `code` always maps to the same openid — important for the
// "account reuse on second OAuth" verification in
// docs/ops/wechat-qq-sandbox-runbook.md §C.4.
func hashTo12(seed, salt string) string {
	sum := sha256.Sum256([]byte(seed + "|" + salt))
	return hex.EncodeToString(sum[:6]) // 12 chars
}

func handleWeChatTokenExchange(w http.ResponseWriter, r *http.Request) {
	q := r.URL.Query()
	code := q.Get("code")
	appid := q.Get("appid")

	w.Header().Set("Content-Type", "application/json; charset=utf-8")

	// Empty / missing required fields -> mimic real errcodes.
	switch {
	case code == "":
		writeJSON(w, map[string]any{"errcode": 40029, "errmsg": "invalid code"})
		return
	case code == "EXPIRED":
		writeJSON(w, map[string]any{"errcode": 40029, "errmsg": "invalid code"})
		return
	case appid == "":
		writeJSON(w, map[string]any{"errcode": 40013, "errmsg": "invalid appid"})
		return
	}

	resp := map[string]any{
		"access_token":  "MOCK_ACCESS_" + hashTo12(code, "access"),
		"expires_in":    7200,
		"refresh_token": "MOCK_REFRESH_" + hashTo12(code, "refresh"),
		"openid":        "openid_" + hashTo12(code, "openid"),
		"scope":         "snsapi_userinfo",
	}
	// Mirror the "unionid only when app is bound to an Open Platform
	// subject" quirk: emit unionid only when code starts with "U".
	if strings.HasPrefix(code, "U") {
		resp["unionid"] = "unionid_" + hashTo12(code, "unionid")
	}
	writeJSON(w, resp)
}

func handleQQTokenVerify(w http.ResponseWriter, r *http.Request) {
	q := r.URL.Query()
	accessToken := q.Get("access_token")

	w.Header().Set("Content-Type", "application/json; charset=utf-8")

	switch {
	case accessToken == "":
		writeJSON(w, map[string]any{"error": 100002, "error_description": "missing access_token"})
		return
	case accessToken == "INVALID":
		writeJSON(w, map[string]any{"error": 100007, "error_description": "wrong access_token"})
		return
	}

	resp := map[string]any{
		"client_id": "100000001", // mirror the AppId in the sandbox login.yaml example
		"openid":    "qq_openid_" + hashTo12(accessToken, "openid"),
		"unionid":   "qq_unionid_" + hashTo12(accessToken, "unionid"),
	}
	writeJSON(w, resp)
}

func writeJSON(w http.ResponseWriter, v any) {
	if err := json.NewEncoder(w).Encode(v); err != nil {
		log.Printf("sandbox mock: encode: %v", err)
	}
}

// Compile-time sanity: keep the imports honest if someone later trims them.
var _ = fmt.Sprint
