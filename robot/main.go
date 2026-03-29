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

	"github.com/golang/protobuf/proto"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

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

	host, portStr, err := resolveGateAddr(cfg)
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
			runRobot(host, port, account, cfg, stats, stopAll)
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
func runRobot(host string, port int, account string, cfg *config.Config, stats *metrics.Stats, stop <-chan struct{}) {
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

	// Give the connection time to establish.
	time.Sleep(500 * time.Millisecond)

	// --- Login flow ---
	loginStart := time.Now()

	if err := gc.SendRequest(game.ClientPlayerLoginLoginMessageId, &login.LoginRequest{
		Account:  account,
		Password: cfg.Password,
	}); err != nil {
		zap.L().Error("send login", zap.String("account", account), zap.Error(err))
		stats.LoginFail()
		return
	}
	stats.MsgSent()

	loginResp, err := waitForMessage(gc, game.ClientPlayerLoginLoginMessageId, stats)
	if err != nil {
		zap.L().Error("login response", zap.String("account", account), zap.Error(err))
		stats.LoginFail()
		return
	}
	var lr login.LoginResponse
	if err := proto.Unmarshal(loginResp.SerializedMessage, &lr); err != nil {
		zap.L().Error("unmarshal login response", zap.Error(err))
		stats.LoginFail()
		return
	}
	if lr.ErrorMessage != nil {
		zap.L().Error("login error", zap.String("account", account), zap.Any("tip", lr.ErrorMessage))
		stats.LoginFail()
		return
	}

	// --- Create player if needed ---
	if len(lr.Players) == 0 {
		if err := gc.SendRequest(game.ClientPlayerLoginCreatePlayerMessageId, &login.CreatePlayerRequest{}); err != nil {
			zap.L().Error("send create player", zap.Error(err))
			stats.LoginFail()
			return
		}
		stats.MsgSent()
		createResp, err := waitForMessage(gc, game.ClientPlayerLoginCreatePlayerMessageId, stats)
		if err != nil {
			zap.L().Error("create player response", zap.Error(err))
			stats.LoginFail()
			return
		}
		var cr login.CreatePlayerResponse
		if err := proto.Unmarshal(createResp.SerializedMessage, &cr); err != nil {
			zap.L().Error("unmarshal create response", zap.Error(err))
			stats.LoginFail()
			return
		}
		if cr.ErrorMessage != nil {
			zap.L().Error("create error", zap.Any("tip", cr.ErrorMessage))
			stats.LoginFail()
			return
		}
		lr.Players = cr.Players
	}

	if len(lr.Players) == 0 {
		zap.L().Error("no players after create", zap.String("account", account))
		stats.LoginFail()
		return
	}

	playerId := lr.Players[0].GetPlayer().GetPlayerId()

	// --- Enter game ---
	if err := gc.SendRequest(game.ClientPlayerLoginEnterGameMessageId, &login.EnterGameRequest{
		PlayerId: playerId,
	}); err != nil {
		zap.L().Error("send enter game", zap.Error(err))
		stats.EnterFail()
		return
	}
	stats.MsgSent()
	enterResp, err := waitForMessage(gc, game.ClientPlayerLoginEnterGameMessageId, stats)
	if err != nil {
		zap.L().Error("enter game response", zap.Error(err))
		stats.EnterFail()
		return
	}
	var er login.EnterGameResponse
	if err := proto.Unmarshal(enterResp.SerializedMessage, &er); err != nil {
		zap.L().Error("unmarshal enter response", zap.Error(err))
		stats.EnterFail()
		return
	}
	if er.ErrorMessage != nil {
		zap.L().Error("enter game error", zap.Any("tip", er.ErrorMessage))
		stats.EnterFail()
		return
	}

	gc.PlayerId = er.PlayerId
	loginDuration := time.Since(loginStart)
	stats.LoginOK(loginDuration)
	stats.EnterOK()

	zap.L().Info("entered game",
		zap.String("account", account),
		zap.Uint64("player_id", gc.PlayerId),
		zap.Duration("login_time", loginDuration),
	)

	handler.RegisterPlayer(gc)

	// --- Start AI loop in a separate goroutine ---
	robotAI := ai.NewRobotAI(gc, stats)
	if len(cfg.SkillIDs) > 0 {
		robotAI.SetSkillIDs(cfg.SkillIDs)
	}
	if cfg.ActionInterval > 0 {
		robotAI.SetInterval(time.Duration(cfg.ActionInterval) * time.Second)
	}
	go robotAI.RunLoop(stop)

	// --- Dispatch incoming messages ---
	gc.RecvLoop(func(client *pkg.GameClient, msg *base.MessageContent) {
		stats.MsgRecv()
		handler.MessageBodyHandler(client, msg)
	})
}

// waitForMessage blocks until a MessageContent with the given messageId arrives.
func waitForMessage(gc *pkg.GameClient, messageId uint32, stats *metrics.Stats) (*base.MessageContent, error) {
	for {
		raw, err := gc.RecvOne()
		if err != nil {
			return nil, err
		}
		stats.MsgRecv()
		if raw.MessageId == messageId {
			return raw, nil
		}
		zap.L().Debug("skipping unexpected msg during login",
			zap.Uint32("got", raw.MessageId),
			zap.Uint32("want", messageId),
		)
	}
}

// resolveGateAddr picks the best gate endpoint.
// If login_addr is configured, it calls LoginPreGate.GetGateList to select the
// least-loaded gate. Otherwise, falls back to the static gate_addr config.
func resolveGateAddr(cfg *config.Config) (host string, port string, err error) {
	if cfg.LoginAddr != "" {
		gateAddr, err := selectLeastLoadedGate(cfg.LoginAddr)
		if err != nil {
			zap.L().Warn("GetGateList failed, falling back to gate_addr",
				zap.String("login_addr", cfg.LoginAddr),
				zap.Error(err),
			)
		} else {
			zap.L().Info("auto-selected gate via GetGateList",
				zap.String("gate", gateAddr),
			)
			return net.SplitHostPort(gateAddr)
		}
	}
	return net.SplitHostPort(cfg.GateAddr)
}

// selectLeastLoadedGate calls LoginPreGate.GetGateList and returns the address
// of the gate with the lowest player_count (first in the sorted response).
func selectLeastLoadedGate(loginAddr string) (string, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, err := grpc.NewClient(loginAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return "", fmt.Errorf("grpc dial login %s: %w", loginAddr, err)
	}
	defer conn.Close()

	client := login.NewLoginPreGateClient(conn)
	resp, err := client.GetGateList(ctx, &login.GetGateListRequest{})
	if err != nil {
		return "", fmt.Errorf("GetGateList: %w", err)
	}
	if len(resp.Gates) == 0 {
		return "", fmt.Errorf("GetGateList returned empty list")
	}

	// Response is sorted by player_count ascending; pick the first (least loaded)
	best := resp.Gates[0]
	zap.L().Info("gate list received",
		zap.Int("count", len(resp.Gates)),
		zap.Uint32("selected_node_id", best.NodeId),
		zap.Uint32("player_count", best.PlayerCount),
	)
	return fmt.Sprintf("%s:%d", best.Ip, best.Port), nil
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
