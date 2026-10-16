package main

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"time"

	"robot/metrics"
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

// gatewayQueueAPI 是排队状态机用到的两个网关端点。生产实现是 httpGatewayQueueAPI;
// 用例注入按脚本应答的假实现,不起 HTTP 服务。
type gatewayQueueAPI interface {
	AssignGate(req *httpAssignGateRequest) (*httpAssignGateResponse, error)
	QueueStatus(queueToken string, zoneID uint32) (*httpAssignGateResponse, error)
}

// httpGatewayQueueAPI 把 gatewayQueueAPI 落到 /api/assign-gate 与 /api/queue-status,每次调用各带 perCallTimeout。
type httpGatewayQueueAPI struct {
	gatewayAddr    string
	perCallTimeout time.Duration
}

func (a httpGatewayQueueAPI) AssignGate(req *httpAssignGateRequest) (*httpAssignGateResponse, error) {
	return httpAssignGate(a.gatewayAddr, req, a.perCallTimeout)
}

func (a httpGatewayQueueAPI) QueueStatus(queueToken string, zoneID uint32) (*httpAssignGateResponse, error) {
	return httpQueueStatus(a.gatewayAddr, queueToken, zoneID, a.perCallTimeout)
}

// sleepCtx 等待 d;ctx 先结束则返回 ctx.Err()。AssignGateWithQueue 的生产用 sleep。
func sleepCtx(ctx context.Context, d time.Duration) error {
	timer := time.NewTimer(d)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-timer.C:
		return nil
	}
}

// queuePollInterval 是发下一次请求前的等待:取 retry_after_ms,为 0 时取 2s
// (code=429 的回包不带 retry_after_ms,也落到 2s)。
func queuePollInterval(rsp *httpAssignGateResponse) time.Duration {
	wait := time.Duration(rsp.RetryAfterMs) * time.Millisecond
	if wait <= 0 {
		wait = 2 * time.Second
	}
	return wait
}

// AssignGateWithQueue 是 robot 的排队状态机:先调 /api/assign-gate,再按回包推进,直到放行(code=0)、
// 遇到不可恢复的回包或网络错误,或 ctx 预算用完。按负责人 2026-10-15 定的客户端契约
// (docs/design/open-server-rate-limit-design.md §11.5 的 2026-10-15 更正):
//   - 持有 queue_token 时一律轮询 /api/queue-status 查排位、取放行票。该端点不过限流器。
//   - /api/queue-status 本身失败(网络错误或 code>=500)时,带同一 token 重调一次 /api/assign-gate 作恢复。
//     重入回 code=100 且 queue_source="ratelimit",或回 code=429,都保留 token、回到 /api/queue-status 轮询;
//     不按 429 的 30/60/120s 退避重调 assign-gate,否则已被放行的玩家会错过 login 的放行窗口(AdmitTTL 60s)。
//   - 收到 410 丢掉 token,不带 token 重调 /api/assign-gate。
//   - 不持有 token 时,code=100(ratelimit)按间隔重调 /api/assign-gate;code=429 直接返回错误,
//     交给调用方的外层重试(robot 把 429 当失败,设计稿 §11.7 R2)。
//
// 每次等待的间隔见 queuePollInterval。恢复用的重入也失败时返回错误,外层重试从头开始,
// token 随之丢弃(压测可接受)。
//
// req 被就地更新(QueueToken 随回包变化)。stats 可为 nil,口径:首次回 code=100 记 entered,
// 放行时记等待时长与最大名次,丢弃 token 时记 expired。sleep 由调用方注入,生产用 sleepCtx。
func AssignGateWithQueue(
	ctx context.Context,
	api gatewayQueueAPI,
	req *httpAssignGateRequest,
	stats *metrics.Stats,
	sleep func(context.Context, time.Duration) error,
) (*httpAssignGateResponse, error) {
	var firstQueueAt time.Time
	var maxRankSeen int64 = -1
	enteredQueue := false

	rsp, err := api.AssignGate(req)
	if err != nil {
		return nil, err
	}
	for {
		switch rsp.Code {
		case 0:
			// 放行。排过队的话记等待时长与最大名次。
			if enteredQueue && stats != nil {
				stats.QueueAdmittedAfterWait()
				stats.QueueWait(time.Since(firstQueueAt))
				if maxRankSeen >= 0 {
					stats.QueueRankSeen(maxRankSeen)
				}
			}
			return rsp, nil

		case 100:
			if !enteredQueue {
				enteredQueue = true
				firstQueueAt = time.Now()
				if stats != nil {
					stats.QueueEntered()
				}
			}
			if rsp.QueueRank > maxRankSeen {
				maxRankSeen = rsp.QueueRank
			}
			// 真排队的回包带 token(queue-status 回显同一个);ratelimit 的回包不带,保留手里原有的 token。
			if rsp.QueueSource == "login" && rsp.QueueToken != "" {
				req.QueueToken = rsp.QueueToken
			}
			if err := sleep(ctx, queuePollInterval(rsp)); err != nil {
				return nil, fmt.Errorf("queue budget exceeded (rank=%d, source=%s)", rsp.QueueRank, rsp.QueueSource)
			}
			if req.QueueToken != "" {
				rsp, err = pollQueueStatus(api, req)
			} else {
				// 还没有 token(首调就被波次或 zone 桶挡住):重调 assign-gate,让限流器重新判定。
				rsp, err = api.AssignGate(req)
			}
			if err != nil {
				return nil, err
			}

		case 429:
			if req.QueueToken == "" {
				return nil, fmt.Errorf("rate-limited: %s", rsp.Error)
			}
			// 恢复用的重入被 IP 桶挡住:回到 queue-status 轮询,不退避重调 assign-gate。
			if err := sleep(ctx, queuePollInterval(rsp)); err != nil {
				return nil, fmt.Errorf("queue budget exceeded (rate-limited: %s)", rsp.Error)
			}
			rsp, err = pollQueueStatus(api, req)
			if err != nil {
				return nil, err
			}

		case 410:
			if req.QueueToken == "" {
				// 手里没有 token 却回 410,不在排队契约里;直接报错,免得不等待地反复重调 assign-gate。
				return nil, fmt.Errorf("AssignGate code=410 without queue_token err=%q", rsp.Error)
			}
			// token 失效(排队已关、token 过期,或放行窗口已过):丢掉 token,从 assign-gate 重来。
			if stats != nil {
				stats.QueueExpiredToken()
			}
			req.QueueToken = ""
			rsp, err = api.AssignGate(req)
			if err != nil {
				return nil, err
			}

		default:
			return nil, fmt.Errorf("AssignGate code=%d err=%q", rsp.Code, rsp.Error)
		}
	}
}

// pollQueueStatus 用 req 里的 token 查一次 /api/queue-status。它本身失败(网络错误或 code>=500)时,
// 带同一 token 重调一次 /api/assign-gate 作恢复,把重入的回包交回状态机;重入也是网络错误才返回错误。
func pollQueueStatus(api gatewayQueueAPI, req *httpAssignGateRequest) (*httpAssignGateResponse, error) {
	rsp, err := api.QueueStatus(req.QueueToken, req.ZoneID)
	if err == nil && rsp.Code < 500 {
		return rsp, nil
	}
	if err == nil {
		err = fmt.Errorf("code=%d err=%q", rsp.Code, rsp.Error)
	}
	reentry, reentryErr := api.AssignGate(req)
	if reentryErr != nil {
		return nil, fmt.Errorf("queue-status failed (%v) and re-entry failed: %w", err, reentryErr)
	}
	return reentry, nil
}
