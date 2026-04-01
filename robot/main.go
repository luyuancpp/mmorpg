package main

import (
	"context"
	"flag"
	"fmt"
	"net"
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
	"google.golang.org/protobuf/proto"

	"robot/config"
	"robot/generated/pb/game"
	"robot/logic/ai"
	"robot/logic/handler"
	"robot/metrics"
	"robot/pkg"
	"robot/proto/common/base"
	"robot/proto/login"

	// Blank-import proto packages so they register with protoregistry.
	_ "robot/proto/chat"
	_ "robot/proto/scene"
)

func main() {
	cfgPath := flag.String("c", "etc/robot.yaml", "config file path")
	flag.Parse()

	initLogger()

	cfg, err := config.Load(*cfgPath)
	if err != nil {
		zap.L().Fatal("load config", zap.Error(err))
	}

	host, portStr, tokenPayload, tokenSig, err := resolveGateAddr(cfg)
	if err != nil {
		zap.L().Fatal("resolve gate address", zap.Error(err))
	}
	port, _ := strconv.Atoi(portStr)

	// Metrics reporter.
	stats := metrics.NewStats()
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

	// Global stop channel — closed on SIGINT/SIGTERM.
	stopAll := make(chan struct{})

	var wg sync.WaitGroup
	for i := 1; i <= cfg.RobotCount; i++ {
		account := fmt.Sprintf(cfg.AccountFmt, i)
		wg.Add(1)
		go func(account string) {
			defer wg.Done()
			runRobot(host, port, account, cfg, stats, stopAll, tokenPayload, tokenSig)
		}(account)
		// Stagger connections to avoid thundering herd.
		time.Sleep(50 * time.Millisecond)
	}

	// Wait for interrupt signal.
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	<-sig

	zap.L().Info("shutting down...")
	close(stopAll)
	close(stopReport)
	wg.Wait()
	zap.L().Info("all robots stopped")
}

// runRobot is the lifecycle for a single robot (one goroutine).
func runRobot(host string, port int, account string, cfg *config.Config, stats *metrics.Stats, stop <-chan struct{}, tokenPayload, tokenSig []byte) {
	gc, err := pkg.NewGameClient(host, port)
	if err != nil {
		zap.L().Error("connect failed", zap.String("account", account), zap.Error(err))
		stats.LoginFail()
		return
	}
	defer gc.Close()
	stats.Connected()
	defer stats.Disconnected()

	gc.Account = account
	time.Sleep(500 * time.Millisecond)

	// Gate token handshake (if token was provided by AssignGate).
	if len(tokenPayload) > 0 {
		if err := gc.VerifyGateToken(tokenPayload, tokenSig); err != nil {
			zap.L().Error("gate token failed", zap.String("account", account), zap.Error(err))
			stats.LoginFail()
			return
		}
	}

	// Login → create player if needed → enter game.
	loginStart := time.Now()
	if err := loginAndEnter(gc, cfg, stats); err != nil {
		zap.L().Error("login flow failed", zap.String("account", account), zap.Error(err))
		return
	}

	loginDuration := time.Since(loginStart)
	stats.LoginOK(loginDuration)
	stats.EnterOK()
	zap.L().Info("entered game",
		zap.String("account", account),
		zap.Uint64("player_id", gc.PlayerId),
		zap.Duration("login_time", loginDuration),
	)

	// AI loop in a separate goroutine.
	robotAI := ai.NewRobotAI(gc, stats)
	if len(cfg.SkillIDs) > 0 {
		robotAI.SetSkillIDs(cfg.SkillIDs)
	}
	if cfg.ActionInterval > 0 {
		robotAI.SetInterval(time.Duration(cfg.ActionInterval) * time.Second)
	}
	go robotAI.RunLoop(stop)

	// Dispatch incoming messages (blocks until connection closes).
	gc.RecvLoop(func(client *pkg.GameClient, msg *base.MessageContent) {
		stats.MsgRecv()
		handler.HandleMessage(client, msg)
	})
}

// loginAndEnter performs: login → create player (if needed) → enter game.
func loginAndEnter(gc *pkg.GameClient, cfg *config.Config, stats *metrics.Stats) error {
	// 1. Login
	var lr login.LoginResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginLoginMessageId,
		&login.LoginRequest{Account: gc.Account, Password: cfg.Password},
		&lr,
	); err != nil {
		stats.LoginFail()
		return fmt.Errorf("login: %w", err)
	}
	if lr.ErrorMessage != nil {
		stats.LoginFail()
		return fmt.Errorf("login: server error %v", lr.ErrorMessage)
	}

	// 2. Create player if needed
	if len(lr.Players) == 0 {
		var cr login.CreatePlayerResponse
		if err := sendAndRecv(gc, stats,
			game.ClientPlayerLoginCreatePlayerMessageId,
			&login.CreatePlayerRequest{},
			&cr,
		); err != nil {
			stats.LoginFail()
			return fmt.Errorf("create player: %w", err)
		}
		if cr.ErrorMessage != nil {
			stats.LoginFail()
			return fmt.Errorf("create player: server error %v", cr.ErrorMessage)
		}
		lr.Players = cr.Players
	}
	if len(lr.Players) == 0 {
		stats.LoginFail()
		return fmt.Errorf("no players after create")
	}

	// 3. Enter game
	playerId := lr.Players[0].GetPlayer().GetPlayerId()
	var er login.EnterGameResponse
	if err := sendAndRecv(gc, stats,
		game.ClientPlayerLoginEnterGameMessageId,
		&login.EnterGameRequest{PlayerId: playerId},
		&er,
	); err != nil {
		stats.EnterFail()
		return fmt.Errorf("enter game: %w", err)
	}
	if er.ErrorMessage != nil {
		stats.EnterFail()
		return fmt.Errorf("enter game: server error %v", er.ErrorMessage)
	}

	gc.PlayerId = er.PlayerId
	return nil
}

// sendAndRecv sends a request and blocks until the matching response arrives,
// then unmarshals it into resp.
func sendAndRecv(gc *pkg.GameClient, stats *metrics.Stats, msgId uint32, req, resp proto.Message) error {
	if err := gc.SendRequest(msgId, req); err != nil {
		return err
	}
	stats.MsgSent()

	for {
		raw, err := gc.RecvOne()
		if err != nil {
			return err
		}
		stats.MsgRecv()
		if raw.MessageId == msgId {
			return proto.Unmarshal(raw.SerializedMessage, resp)
		}
		zap.L().Debug("skipping msg during login",
			zap.Uint32("got", raw.MessageId),
			zap.Uint32("want", msgId),
		)
	}
}

// resolveGateAddr picks the best gate endpoint via Login's AssignGate RPC.
// Returns gate address and the HMAC connection token.
// If login_addr is configured, calls LoginPreGate.AssignGate to get the
// assigned gate + signed token. Otherwise, falls back to static gate_addr
// with nil token (dev mode, relies on Gate skipping verification).
func resolveGateAddr(cfg *config.Config) (host string, port string, payload []byte, signature []byte, err error) {
	if cfg.LoginAddr != "" {
		result, assignErr := assignGate(cfg.LoginAddr, 0) // zone_id=0 means any zone
		if assignErr != nil {
			zap.L().Warn("AssignGate failed, falling back to gate_addr",
				zap.String("login_addr", cfg.LoginAddr),
				zap.Error(assignErr),
			)
		} else {
			zap.L().Info("assigned gate via AssignGate",
				zap.String("gate", result.addr),
			)
			h, p, splitErr := net.SplitHostPort(result.addr)
			return h, p, result.payload, result.signature, splitErr
		}
	}
	h, p, splitErr := net.SplitHostPort(cfg.GateAddr)
	return h, p, nil, nil, splitErr
}

type gateAssignment struct {
	addr      string
	payload   []byte
	signature []byte
}

// assignGate calls LoginPreGate.AssignGate and returns the assigned gate
// address plus the HMAC token to send during the Gate handshake.
func assignGate(loginAddr string, zoneId uint32) (*gateAssignment, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, err := grpc.NewClient(loginAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return nil, fmt.Errorf("grpc dial login %s: %w", loginAddr, err)
	}
	defer conn.Close()

	client := login.NewLoginPreGateClient(conn)
	resp, err := client.AssignGate(ctx, &login.AssignGateRequest{ZoneId: zoneId})
	if err != nil {
		return nil, fmt.Errorf("AssignGate: %w", err)
	}
	if resp.Error != "" {
		return nil, fmt.Errorf("AssignGate: %s", resp.Error)
	}
	if resp.Ip == "" || resp.Port == 0 {
		return nil, fmt.Errorf("AssignGate returned empty address")
	}

	addr := fmt.Sprintf("%s:%d", resp.Ip, resp.Port)
	zap.L().Info("gate assigned",
		zap.String("addr", addr),
		zap.Int64("deadline", resp.TokenDeadline),
	)
	return &gateAssignment{
		addr:      addr,
		payload:   resp.TokenPayload,
		signature: resp.TokenSignature,
	}, nil
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
