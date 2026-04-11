package main

import (
	"bytes"
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"sync"
	"syscall"
	"time"

	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	"proto/common/base"
	"proto/login"
	"robot/config"
	"robot/generated/pb/game"
	"robot/logic/ai"
	"robot/logic/handler"
	"robot/metrics"
	"robot/pkg"

	// Blank-import proto packages so they register with protoregistry.
	_ "proto/chat"
	_ "proto/scene"
)

func main() {
	cfgPath := flag.String("c", "etc/robot.yaml", "config file path")
	flag.Parse()

	initLogger()

	cfg, err := config.Load(*cfgPath)
	if err != nil {
		zap.L().Fatal("load config", zap.Error(err))
	}

	host, portStr, tokenPayload, tokenSig, err := resolveGateAddrLocal(cfg)
	if err != nil {
		zap.L().Fatal("resolve gate address", zap.Error(err))
	}
	port, _ := strconv.Atoi(portStr)

	stats := metrics.NewStats()

	// Login-test mode: run the test suite and exit.
	if cfg.Mode == "login-test" {
		runLoginTests(host, port, cfg, stats, tokenPayload, tokenSig)
		return
	}

	stopReport := make(chan struct{})
	reportInterval := time.Duration(cfg.ReportInterval) * time.Second
	if reportInterval <= 0 {
		reportInterval = 5 * time.Second
	}
	stats.StartReporter(reportInterval, stopReport)

	zap.L().Info("starting robots",
		zap.String("gate", cfg.GateAddr),
		zap.Int("count", cfg.RobotCount),
	)

	stopAll := make(chan struct{})

	var wg sync.WaitGroup
	for i := 1; i <= cfg.RobotCount; i++ {
		account := fmt.Sprintf(cfg.AccountFmt, i)
		wg.Add(1)
		go func(account string) {
			defer func() {
				if r := recover(); r != nil {
					zap.L().Error("robot panic", zap.String("account", account), zap.Any("panic", r))
				}
				wg.Done()
			}()
			runRobot(host, port, account, cfg, stats, stopAll, tokenPayload, tokenSig)
		}(account)
		time.Sleep(50 * time.Millisecond) // stagger to avoid thundering herd
	}

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	<-sig

	zap.L().Info("shutting down...")
	close(stopAll)
	close(stopReport)
	wg.Wait()
	zap.L().Info("all robots stopped")
}

// runRobot is the full lifecycle for one robot (one goroutine).
func runRobot(host string, port int, account string, cfg *config.Config, stats *metrics.Stats, stop <-chan struct{}, tokenPayload, tokenSig []byte) {
	const maxRetries = 5
	backoff := 3 * time.Second

	for attempt := 1; attempt <= maxRetries; attempt++ {
		select {
		case <-stop:
			return
		default:
		}

		if attempt > 1 {
			zap.L().Info("retrying login", zap.String("account", account), zap.Int("attempt", attempt), zap.Duration("backoff", backoff))
			select {
			case <-stop:
				return
			case <-time.After(backoff):
			}
			backoff = backoff * 2
			if backoff > 30*time.Second {
				backoff = 30 * time.Second
			}
		}

		if runRobotOnce(host, port, account, cfg, stats, stop, tokenPayload, tokenSig) {
			return // played successfully (or stop signal)
		}
	}
	zap.L().Error("robot gave up after retries", zap.String("account", account), zap.Int("maxRetries", maxRetries))
}

// runRobotOnce attempts a single connect→login→play cycle. Returns true if it should not retry.
func runRobotOnce(host string, port int, account string, cfg *config.Config, stats *metrics.Stats, stop <-chan struct{}, tokenPayload, tokenSig []byte) bool {
	gc, err := pkg.NewGameClient(host, port)
	if err != nil {
		zap.L().Error("connect failed", zap.String("account", account), zap.Error(err))
		stats.LoginFail()
		return false // retry
	}
	defer gc.Close()
	stats.Connected()
	defer stats.Disconnected()

	gc.Account = account
	time.Sleep(500 * time.Millisecond)

	if len(tokenPayload) > 0 {
		if err := gc.VerifyGateToken(tokenPayload, tokenSig); err != nil {
			zap.L().Error("gate token failed", zap.String("account", account), zap.Error(err))
			stats.LoginFail()
			return false
		}
	}

	loginStart := time.Now()
	if err := loginAndEnter(gc, cfg.Password, stats); err != nil {
		zap.L().Error("login flow failed", zap.String("account", account), zap.Error(err))
		return false
	}
	stats.LoginOK(time.Since(loginStart))
	stats.EnterOK()

	zap.L().Info("entered game",
		zap.String("account", account),
		zap.Uint64("player_id", gc.PlayerId),
	)

	robotAI := ai.NewRobotAI(gc, stats)
	if len(cfg.SkillIDs) > 0 {
		robotAI.SetSkillIDs(cfg.SkillIDs)
	}
	if cfg.ActionInterval > 0 {
		robotAI.SetInterval(time.Duration(cfg.ActionInterval) * time.Second)
	}
	profile := cfg.ResolveProfile()
	if profile != nil {
		robotAI.SetProfile(profile)
	}
	if cfg.LLM.Enabled {
		robotAI.SetLLM(ai.NewLLMAdvisor(cfg.LLM.Endpoint, cfg.LLM.APIKey, cfg.LLM.Model))
	}
	go robotAI.RunLoop(stop)

	gc.RecvLoop(func(client *pkg.GameClient, msg *base.MessageContent) {
		stats.MsgRecv()
		handler.MessageBodyHandler(client, msg)
	})

	// Graceful shutdown: send LeaveGame so server cleans up session.
	_ = gc.SendRequest(game.ClientPlayerLoginLeaveGameMessageId, &login.LeaveGameRequest{})
	return true
}

func initLogger() {
	cfg := zap.NewDevelopmentConfig()
	cfg.EncoderConfig.EncodeLevel = zapcore.CapitalColorLevelEncoder
	cfg.Level.SetLevel(zap.InfoLevel)
	logger, err := cfg.Build()
	if err != nil {
		panic(err)
	}
	zap.ReplaceGlobals(logger)
}

type gateAssignmentLocal struct {
	addr      string
	payload   []byte
	signature []byte
}

// resolveGateAddrLocal keeps main.go runnable as a single-file command.
func resolveGateAddrLocal(cfg *config.Config) (host, port string, payload, signature []byte, err error) {
	mode := cfg.GateMode
	if mode == "" {
		if cfg.GatewayAddr != "" {
			mode = "http"
		} else {
			mode = "grpc"
		}
	}

	if mode == "http" {
		return resolveGateViaHTTPLocal(cfg)
	}

	// grpc mode
	if cfg.LoginAddr != "" {
		const maxRetries = 30
		backoff := 2 * time.Second
		for attempt := 1; attempt <= maxRetries; attempt++ {
			result, assignErr := assignGateLocal(cfg.LoginAddr, cfg.ZoneID)
			if assignErr == nil {
				h, p, splitErr := net.SplitHostPort(result.addr)
				return h, p, result.payload, result.signature, splitErr
			}
			if attempt < maxRetries {
				time.Sleep(backoff)
				if backoff < 10*time.Second {
					backoff = backoff * 3 / 2
				}
			}
		}
		if cfg.GateAddr == "" {
			return "", "", nil, nil, fmt.Errorf("AssignGate failed after retries and no static gate_addr configured")
		}
	}
	h, p, splitErr := net.SplitHostPort(cfg.GateAddr)
	return h, p, nil, nil, splitErr
}

func assignGateLocal(loginAddr string, zoneId uint32) (*gateAssignmentLocal, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, err := grpc.NewClient(loginAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return nil, fmt.Errorf("grpc dial %s: %w", loginAddr, err)
	}
	defer func() { _ = conn.Close() }()

	resp, err := login.NewLoginPreGateClient(conn).AssignGate(ctx, &login.AssignGateRequest{ZoneId: zoneId})
	if err != nil {
		return nil, fmt.Errorf("AssignGate: %w", err)
	}
	if resp.Error != "" {
		return nil, fmt.Errorf("AssignGate: %s", resp.Error)
	}
	if resp.Ip == "" || resp.Port == 0 {
		return nil, fmt.Errorf("AssignGate returned empty address")
	}

	return &gateAssignmentLocal{
		addr:      fmt.Sprintf("%s:%d", resp.Ip, resp.Port),
		payload:   resp.TokenPayload,
		signature: resp.TokenSignature,
	}, nil
}

// resolveGateViaHTTPLocal calls POST /api/assign-gate (HTTP mode) with retry.
func resolveGateViaHTTPLocal(cfg *config.Config) (host, port string, payload, signature []byte, err error) {
	if cfg.GatewayAddr == "" {
		h, p, splitErr := net.SplitHostPort(cfg.GateAddr)
		return h, p, nil, nil, splitErr
	}

	zoneID := cfg.ZoneID
	if zoneID == 0 {
		zoneID, err = resolveZoneIDLocal(cfg.GatewayAddr)
		if err != nil {
			zoneID = 0
		}
	}

	const maxRetries = 30
	backoff := 2 * time.Second
	for attempt := 1; attempt <= maxRetries; attempt++ {
		result, assignErr := assignGateHTTPLocal(cfg.GatewayAddr, zoneID)
		if assignErr == nil {
			h, p, splitErr := net.SplitHostPort(result.addr)
			return h, p, result.payload, result.signature, splitErr
		}
		if attempt < maxRetries {
			time.Sleep(backoff)
			if backoff < 10*time.Second {
				backoff = backoff * 3 / 2
			}
		}
	}
	if cfg.GateAddr == "" {
		return "", "", nil, nil, fmt.Errorf("HTTP AssignGate failed after retries and no static gate_addr configured")
	}
	h, p, splitErr := net.SplitHostPort(cfg.GateAddr)
	return h, p, nil, nil, splitErr
}

func resolveZoneIDLocal(gatewayAddr string) (uint32, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	req, err := http.NewRequestWithContext(ctx, http.MethodGet, gatewayAddr+"/api/server-list", nil)
	if err != nil {
		return 0, err
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return 0, err
	}

	var result struct {
		Zones []struct {
			ZoneID      uint32 `json:"zone_id"`
			Recommended bool   `json:"recommended"`
		} `json:"zones"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		return 0, err
	}
	if len(result.Zones) == 0 {
		return 0, fmt.Errorf("no zones")
	}
	for _, z := range result.Zones {
		if z.Recommended {
			return z.ZoneID, nil
		}
	}
	return result.Zones[0].ZoneID, nil
}

func assignGateHTTPLocal(gatewayAddr string, zoneId uint32) (*gateAssignmentLocal, error) {
	reqBody, _ := json.Marshal(map[string]uint32{"zone_id": zoneId})

	url := gatewayAddr + "/api/assign-gate"
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, url, bytes.NewReader(reqBody))
	if err != nil {
		return nil, fmt.Errorf("build request: %w", err)
	}
	httpReq.Header.Set("Content-Type", "application/json")

	httpResp, err := http.DefaultClient.Do(httpReq)
	if err != nil {
		return nil, fmt.Errorf("HTTP POST %s: %w", url, err)
	}
	defer httpResp.Body.Close()

	body, err := io.ReadAll(httpResp.Body)
	if err != nil {
		return nil, fmt.Errorf("read response: %w", err)
	}
	if httpResp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("HTTP %d: %s", httpResp.StatusCode, string(body))
	}

	var resp struct {
		GateIP         string `json:"gate_ip"`
		GatePort       uint32 `json:"gate_port"`
		TokenPayload   []byte `json:"token_payload"`
		TokenSignature []byte `json:"token_signature"`
		Error          string `json:"error,omitempty"`
	}
	if err := json.Unmarshal(body, &resp); err != nil {
		return nil, fmt.Errorf("decode response: %w", err)
	}
	if resp.Error != "" {
		return nil, fmt.Errorf("AssignGate: %s", resp.Error)
	}
	if resp.GateIP == "" || resp.GatePort == 0 {
		return nil, fmt.Errorf("AssignGate returned empty address")
	}

	return &gateAssignmentLocal{
		addr:      fmt.Sprintf("%s:%d", resp.GateIP, resp.GatePort),
		payload:   resp.TokenPayload,
		signature: resp.TokenSignature,
	}, nil
}


