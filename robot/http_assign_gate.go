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

// httpAssignGateRequest mirrors java/gateway_node AssignGateRequest. Snake
// case matches the gateway's Jackson configuration.
type httpAssignGateRequest struct {
	ZoneID     uint32 `json:"zone_id"`
	Account    string `json:"account,omitempty"`
	DeviceID   string `json:"device_id,omitempty"`
	QueueToken string `json:"queue_token,omitempty"`
}

// httpAssignGateResponse mirrors AssignGateResponse from Java side. The DTO
// is shared between the initial /assign-gate response and the polling
// /queue-status response, so a single shape covers both.
type httpAssignGateResponse struct {
	Code           int    `json:"code"`
	GateIP         string `json:"gate_ip"`
	GatePort       int    `json:"gate_port"`
	TokenPayload   []byte `json:"token_payload"`
	TokenSignature []byte `json:"token_signature"`
	TokenDeadline  int64  `json:"token_deadline"`
	Error          string `json:"error"`

	// queue layer (code=100)
	RetryAfterMs int64  `json:"retry_after_ms"`
	QueuePos     int64  `json:"queue_pos"`
	QueueSource  string `json:"queue_source"`
	QueueToken   string `json:"queue_token"`
	QueueRank    int64  `json:"queue_rank"`
	QueueTotal   int64  `json:"queue_total"`
}

// httpAssignGate calls POST /api/assign-gate on the Java Gateway.
func httpAssignGate(gatewayAddr string, req *httpAssignGateRequest, timeout time.Duration) (*httpAssignGateResponse, error) {
	return postJSON(gatewayAddr+"/api/assign-gate", req, timeout)
}

// httpQueueStatus polls POST /api/queue-status with an existing queue_token.
// Used by queue-aware robots when AssignGate returns code=100 + queueSource="login".
//
// zoneId is a routing hint: in 3-zone deployments the Java Gateway uses it to
// dispatch the poll to the same login.rpc instance that issued the token (each
// instance only watches its own zone's queue state). A 0 falls back to
// round-robin, which is fine for single-zone setups but produces sporadic
// EXPIRED responses across multi-zone configs.
func httpQueueStatus(gatewayAddr, queueToken string, zoneId uint32, timeout time.Duration) (*httpAssignGateResponse, error) {
	return postJSON(gatewayAddr+"/api/queue-status",
		map[string]any{"queue_token": queueToken, "zone_id": zoneId}, timeout)
}

func postJSON(url string, payload any, timeout time.Duration) (*httpAssignGateResponse, error) {
	body, err := json.Marshal(payload)
	if err != nil {
		return nil, fmt.Errorf("encode body: %w", err)
	}
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewReader(body))
	if err != nil {
		return nil, err
	}
	httpReq.Header.Set("Content-Type", "application/json")
	httpResp, err := sharedHTTPClient.Do(httpReq)
	if err != nil {
		return nil, fmt.Errorf("POST %s: %w", url, err)
	}
	defer httpResp.Body.Close()
	respBody, err := io.ReadAll(httpResp.Body)
	if err != nil {
		return nil, err
	}
	if httpResp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("HTTP %d: %s", httpResp.StatusCode, string(respBody))
	}
	var out httpAssignGateResponse
	if err := json.Unmarshal(respBody, &out); err != nil {
		return nil, fmt.Errorf("decode resp: %w (body=%s)", err, string(respBody))
	}
	return &out, nil
}

// AssignGateWithQueue is the robot-side "queue-aware" loop. It calls
// /api/assign-gate, then polls /api/queue-status until one of:
//   - admitted (code=0): returns the gate token bundle
//   - non-queue error: surfaced via err
//   - maxWait exceeded: returns ctx.DeadlineExceeded
//
// Bucket4j-source queueing (code=100, queueSource="ratelimit") is also
// honored: we sleep retry_after_ms and re-call /assign-gate (NOT
// /queue-status — Bucket4j doesn't have a token).
//
// Real-queue (queueSource="login") polls /queue-status with the same
// queue_token until promoted.
func AssignGateWithQueue(
	ctx context.Context,
	gatewayAddr string,
	req *httpAssignGateRequest,
	perCallTimeout time.Duration,
) (*httpAssignGateResponse, error) {
	resp, err := httpAssignGate(gatewayAddr, req, perCallTimeout)
	if err != nil {
		return nil, err
	}
	for {
		switch resp.Code {
		case 0:
			return resp, nil
		case 100:
			// Queueing — figure out whether it's Bucket4j retry or real queue.
			wait := time.Duration(resp.RetryAfterMs) * time.Millisecond
			if wait <= 0 {
				wait = 2 * time.Second
			}
			select {
			case <-ctx.Done():
				return nil, ctx.Err()
			case <-time.After(wait):
			}

			if resp.QueueSource == "login" && resp.QueueToken != "" {
				resp, err = httpQueueStatus(gatewayAddr, resp.QueueToken, req.ZoneID, perCallTimeout)
			} else {
				// ratelimit (or unspecified): re-call /assign-gate.
				resp, err = httpAssignGate(gatewayAddr, req, perCallTimeout)
			}
			if err != nil {
				return nil, err
			}
		case 410:
			// Queue token expired — restart from /assign-gate without the token.
			req.QueueToken = ""
			resp, err = httpAssignGate(gatewayAddr, req, perCallTimeout)
			if err != nil {
				return nil, err
			}
		default:
			return resp, fmt.Errorf("assign-gate code=%d err=%q", resp.Code, resp.Error)
		}
	}
}
