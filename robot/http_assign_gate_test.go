package main

import (
	"context"
	"errors"
	"slices"
	"strings"
	"testing"
	"time"
)

// queueCall 是脚本里的一步:期望状态机调哪个端点、带哪个 queue_token,以及假网关怎么应答。
type queueCall struct {
	endpoint string // "assign-gate" 或 "queue-status"
	token    string // 期望请求携带的 queue_token,空串表示不带
	rsp      *httpAssignGateResponse
	err      error
}

// scriptedGateway 按脚本逐步应答。调用的端点或 token 与脚本不符,或脚本用完后还被调用,立即判失败;
// 状态机在测试 goroutine 里同步运行,所以可以直接 Fatalf。
type scriptedGateway struct {
	t     *testing.T
	steps []queueCall
}

func (g *scriptedGateway) AssignGate(req *httpAssignGateRequest) (*httpAssignGateResponse, error) {
	return g.next("assign-gate", req.QueueToken)
}

func (g *scriptedGateway) QueueStatus(queueToken string, zoneID uint32) (*httpAssignGateResponse, error) {
	return g.next("queue-status", queueToken)
}

func (g *scriptedGateway) next(endpoint, token string) (*httpAssignGateResponse, error) {
	g.t.Helper()
	if len(g.steps) == 0 {
		g.t.Fatalf("unexpected %s(token=%q): script exhausted", endpoint, token)
	}
	step := g.steps[0]
	g.steps = g.steps[1:]
	if step.endpoint != endpoint || step.token != token {
		g.t.Fatalf("call = %s(token=%q), want %s(token=%q)", endpoint, token, step.endpoint, step.token)
	}
	return step.rsp, step.err
}

// recordSleeps 返回一个不真等待的 sleep,只记下每次的等待时长;ctx 已结束时与 sleepCtx 一样返回 ctx.Err()。
func recordSleeps(waits *[]time.Duration) func(context.Context, time.Duration) error {
	return func(ctx context.Context, d time.Duration) error {
		*waits = append(*waits, d)
		return ctx.Err()
	}
}

func loginQueued(token string, rank, retryAfterMs int64) *httpAssignGateResponse {
	return &httpAssignGateResponse{Code: 100, QueueSource: "login", QueueToken: token, QueueRank: rank, RetryAfterMs: retryAfterMs}
}

func rateLimitQueued(retryAfterMs int64) *httpAssignGateResponse {
	return &httpAssignGateResponse{Code: 100, QueueSource: "ratelimit", RetryAfterMs: retryAfterMs}
}

func admittedResponse() *httpAssignGateResponse {
	return &httpAssignGateResponse{Code: 0, GateIP: "10.0.0.7", GatePort: 9000}
}

func codeResponse(code int, errTag string) *httpAssignGateResponse {
	return &httpAssignGateResponse{Code: code, Error: errTag}
}

// TestAssignGateWithQueue 钉住 2026-10-15 的客户端契约(设计稿 open-server-rate-limit-design.md §11.5 更正):
// 持有 queue_token 时轮询 /api/queue-status;它失败才带 token 重入 assign-gate;重入被限流回到轮询;
// 410 丢 token 重来。用例只看调用序列与等待时长,不依赖墙钟。
func TestAssignGateWithQueue(t *testing.T) {
	errNet := errors.New("dial tcp: connection refused")
	cases := []struct {
		name      string
		steps     []queueCall
		wantWaits []time.Duration
		wantErr   string // 空串表示期望放行
	}{
		{
			// 修复前红:queue-status 回 5xx 落进 default 直接返回错误,token 随之丢弃,外层重试从头排队。
			name: "queue-status 5xx, re-entry held by zone bucket, back to queue-status",
			steps: []queueCall{
				{"assign-gate", "", loginQueued("T1", 3, 1000), nil},
				{"queue-status", "T1", codeResponse(500, "login_unavailable"), nil},
				{"assign-gate", "T1", rateLimitQueued(3000), nil},
				{"queue-status", "T1", admittedResponse(), nil},
			},
			wantWaits: []time.Duration{time.Second, 3 * time.Second},
		},
		{
			// 修复前红:queue-status 网络错误直接返回;且 code=429 一律当失败返回,不回到轮询。
			name: "queue-status network error, re-entry hits IP bucket 429, back to queue-status",
			steps: []queueCall{
				{"assign-gate", "", loginQueued("T1", 0, 0), nil},
				{"queue-status", "T1", nil, errNet},
				{"assign-gate", "T1", codeResponse(429, "IP_RATE_LIMIT"), nil},
				{"queue-status", "T1", loginQueued("T1", 0, 2000), nil},
				{"queue-status", "T1", admittedResponse(), nil},
			},
			wantWaits: []time.Duration{2 * time.Second, 2 * time.Second, 2 * time.Second},
		},
		{
			// 修复前红:同第一例,queue-status 的 5xx 直接返回,恢复用的重入根本不会发出。
			name: "queue-status 5xx, re-entry resumes the login queue entry",
			steps: []queueCall{
				{"assign-gate", "", loginQueued("T1", 9, 2000), nil},
				{"queue-status", "T1", codeResponse(503, "login_unavailable"), nil},
				{"assign-gate", "T1", loginQueued("T1", 4, 2000), nil},
				{"queue-status", "T1", admittedResponse(), nil},
			},
			wantWaits: []time.Duration{2 * time.Second, 2 * time.Second},
		},
		{
			name: "410 drops the token and restarts from assign-gate",
			steps: []queueCall{
				{"assign-gate", "", loginQueued("T1", 5, 2000), nil},
				{"queue-status", "T1", codeResponse(410, "queue_token_expired"), nil},
				{"assign-gate", "", loginQueued("T2", 1, 2000), nil},
				{"queue-status", "T2", admittedResponse(), nil},
			},
			wantWaits: []time.Duration{2 * time.Second, 2 * time.Second},
		},
		{
			name: "without a token, ratelimit queueing re-calls assign-gate",
			steps: []queueCall{
				{"assign-gate", "", rateLimitQueued(500), nil},
				{"assign-gate", "", admittedResponse(), nil},
			},
			wantWaits: []time.Duration{500 * time.Millisecond},
		},
		{
			name: "without a token, 429 is left to the outer retry loop",
			steps: []queueCall{
				{"assign-gate", "", codeResponse(429, "IP_RATE_LIMIT"), nil},
			},
			wantErr: "rate-limited: IP_RATE_LIMIT",
		},
		{
			// 修复前红:没有 token 也照样「丢 token」重调 assign-gate,且中间不等待,脚本用完即失败。
			name: "410 without a token fails instead of spinning",
			steps: []queueCall{
				{"assign-gate", "", codeResponse(410, "missing_queue_token"), nil},
			},
			wantErr: "code=410",
		},
		{
			name: "queue-status and the re-entry both fail",
			steps: []queueCall{
				{"assign-gate", "", loginQueued("T1", 0, 2000), nil},
				{"queue-status", "T1", nil, errNet},
				{"assign-gate", "T1", nil, errNet},
			},
			wantWaits: []time.Duration{2 * time.Second},
			wantErr:   "re-entry failed",
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			gw := &scriptedGateway{t: t, steps: tc.steps}
			var waits []time.Duration
			rsp, err := AssignGateWithQueue(context.Background(), gw, &httpAssignGateRequest{ZoneID: 1}, nil, recordSleeps(&waits))
			if tc.wantErr == "" {
				if err != nil {
					t.Fatalf("err = %v, want admitted", err)
				}
				if rsp.Code != 0 {
					t.Fatalf("code = %d, want 0", rsp.Code)
				}
			} else if err == nil || !strings.Contains(err.Error(), tc.wantErr) {
				t.Fatalf("err = %v, want one containing %q", err, tc.wantErr)
			}
			if len(gw.steps) != 0 {
				t.Fatalf("%d scripted calls never made, next %s(token=%q)", len(gw.steps), gw.steps[0].endpoint, gw.steps[0].token)
			}
			if !slices.Equal(waits, tc.wantWaits) {
				t.Fatalf("waits = %v, want %v", waits, tc.wantWaits)
			}
		})
	}
}

// TestAssignGateWithQueue_StopsWhenBudgetExhausted:排队预算(ctx)用完时停在等待处返回,不再发请求。
func TestAssignGateWithQueue_StopsWhenBudgetExhausted(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	gw := &scriptedGateway{t: t, steps: []queueCall{
		{"assign-gate", "", loginQueued("T1", 7, 2000), nil},
	}}
	var waits []time.Duration
	_, err := AssignGateWithQueue(ctx, gw, &httpAssignGateRequest{ZoneID: 1}, nil, recordSleeps(&waits))
	if err == nil || !strings.Contains(err.Error(), "queue budget exceeded") {
		t.Fatalf("err = %v, want queue budget exceeded", err)
	}
	if len(gw.steps) != 0 {
		t.Fatalf("%d scripted calls never made", len(gw.steps))
	}
}
