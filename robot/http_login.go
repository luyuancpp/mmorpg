package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"
)

// httpLoginResponse mirrors java/gateway_node LoginResponse.
//
// Field naming uses snake_case because the Spring gateway is configured with
// PropertyNamingStrategy.SNAKE_CASE — see gateway_node application.yaml.
type httpLoginResponse struct {
	Code               int    `json:"code"`
	Message            string `json:"message"`
	RetryAfterMs       int64  `json:"retry_after_ms"`
	QueuePos           int64  `json:"queue_pos"`
	AccessToken        string `json:"access_token"`
	RefreshToken       string `json:"refresh_token"`
	AccessTokenExpire  int64  `json:"access_token_expire"`
	RefreshTokenExpire int64  `json:"refresh_token_expire"`
}

type httpLoginRequest struct {
	ZoneID    uint32 `json:"zone_id"`
	Account   string `json:"account"`
	Password  string `json:"password,omitempty"`
	AuthType  string `json:"auth_type,omitempty"`
	AuthToken string `json:"auth_token,omitempty"`
	DeviceID  string `json:"device_id,omitempty"`
}

// httpLogin calls POST /api/login on the Java Gateway.
//
// On HTTP success (status 200) the response body still encodes a logical
// success/queue/deny via the {@code code} field. We return:
//
//   - resp.Code == 0          : success — caller uses access_token to skip
//                               the Login RPC on the gate TCP path.
//   - resp.Code == 100        : queueing — caller honours retry_after_ms
//                               and tries again.
//   - resp.Code == 101        : queue timeout — caller bubbles up to the
//                               normal exponential backoff loop.
//   - resp.Code == 401        : auth rejected — bubble up.
//   - resp.Code == 429        : rate limited — same as queue but no
//                               retry_after_ms.
//   - other / network error   : returned via err so the caller can log and
//                               fall back to the legacy gate path.
func httpLogin(gatewayAddr string, req *httpLoginRequest, timeout time.Duration) (*httpLoginResponse, error) {
	if gatewayAddr == "" {
		return nil, fmt.Errorf("gateway_addr empty")
	}
	body, err := json.Marshal(req)
	if err != nil {
		return nil, fmt.Errorf("encode login req: %w", err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, gatewayAddr+"/api/login", bytes.NewReader(body))
	if err != nil {
		return nil, err
	}
	httpReq.Header.Set("Content-Type", "application/json")

	httpResp, err := sharedHTTPClient.Do(httpReq)
	if err != nil {
		return nil, fmt.Errorf("POST /api/login: %w", err)
	}
	defer httpResp.Body.Close()

	respBody, err := io.ReadAll(httpResp.Body)
	if err != nil {
		return nil, fmt.Errorf("read login resp: %w", err)
	}
	if httpResp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("HTTP %d: %s", httpResp.StatusCode, string(respBody))
	}

	var out httpLoginResponse
	if err := json.Unmarshal(respBody, &out); err != nil {
		return nil, fmt.Errorf("decode login resp: %w (body=%s)", err, string(respBody))
	}
	return &out, nil
}

// httpRefreshToken calls POST /api/refresh-token on the Java Gateway.
// Mirrors httpLogin's contract: code==0 is success, otherwise the caller
// treats the token as expired and walks the primary auth path.
func httpRefreshToken(gatewayAddr, refreshToken string, timeout time.Duration) (*httpLoginResponse, error) {
	if gatewayAddr == "" {
		return nil, fmt.Errorf("gateway_addr empty")
	}
	body, _ := json.Marshal(map[string]string{"refresh_token": refreshToken})
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, gatewayAddr+"/api/refresh-token", bytes.NewReader(body))
	if err != nil {
		return nil, err
	}
	httpReq.Header.Set("Content-Type", "application/json")

	httpResp, err := sharedHTTPClient.Do(httpReq)
	if err != nil {
		return nil, fmt.Errorf("POST /api/refresh-token: %w", err)
	}
	defer httpResp.Body.Close()

	respBody, err := io.ReadAll(httpResp.Body)
	if err != nil {
		return nil, err
	}
	if httpResp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("HTTP %d: %s", httpResp.StatusCode, string(respBody))
	}
	var out httpLoginResponse
	if err := json.Unmarshal(respBody, &out); err != nil {
		return nil, fmt.Errorf("decode refresh resp: %w", err)
	}
	return &out, nil
}
