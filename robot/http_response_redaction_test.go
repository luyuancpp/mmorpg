package main

import (
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"
)

// HTTP 错误会直接进入冒烟日志；即使应答损坏，也不能带出其中的凭据。
func TestHTTPResponseErrorsRedactCredentials(t *testing.T) {
	const credentials = `{"access_token":"sentinel-access-token","refresh_token":"sentinel-refresh-token","token_payload":"sentinel-ticket-payload","token_signature":"sentinel-ticket-signature","queue_token":"sentinel-queue-token","password":"sentinel-development-password"}`
	const numericCredential = "123456789012345678901234567890"
	clients := []struct {
		name string
		call func(string) error
	}{
		{"assign-gate", func(url string) error {
			_, err := httpAssignGate(url, &httpAssignGateRequest{}, time.Second)
			return err
		}},
		{"login", func(url string) error {
			_, err := httpLogin(url, &httpLoginRequest{}, time.Second)
			return err
		}},
		{"refresh-token", func(url string) error {
			_, err := httpRefreshToken(url, "request-token", time.Second)
			return err
		}},
	}
	responses := []struct {
		name   string
		status int
		body   string
	}{
		{"http-error", http.StatusServiceUnavailable, credentials},
		{"invalid-json", http.StatusOK, credentials + " trailing-data"},
		{"invalid-field-type", http.StatusOK, `{"access_token":` + numericCredential + `,"token_payload":` + numericCredential + `}`},
	}
	for _, client := range clients {
		for _, response := range responses {
			t.Run(client.name+"/"+response.name, func(t *testing.T) {
				server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
					w.WriteHeader(response.status)
					_, _ = w.Write([]byte(response.body))
				}))
				defer server.Close()
				err := client.call(server.URL)
				if err == nil {
					t.Fatal("无效 HTTP 应答必须返回错误")
				}
				message := err.Error()
				for _, credential := range []string{"sentinel-access-token", "sentinel-refresh-token", "sentinel-ticket-payload", "sentinel-ticket-signature", "sentinel-queue-token", "sentinel-development-password", numericCredential, response.body} {
					if strings.Contains(message, credential) {
						t.Error("错误文本包含应答中的凭据或原始正文")
						break
					}
				}
				if !strings.Contains(message, "response_bytes=") {
					t.Error("错误文本应保留应答长度供诊断")
				}
			})
		}
	}
}
