package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strconv"
	"sync"
	"syscall"
	"time"

	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"

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

	if len(tokenPayload) > 0 {
		if err := gc.VerifyGateToken(tokenPayload, tokenSig); err != nil {
			zap.L().Error("gate token failed", zap.String("account", account), zap.Error(err))
			stats.LoginFail()
			return
		}
	}

	loginStart := time.Now()
	if err := loginAndEnter(gc, cfg.Password, stats); err != nil {
		zap.L().Error("login flow failed", zap.String("account", account), zap.Error(err))
		return
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
		handler.HandleMessage(client, msg)
	})

	// Graceful shutdown: send LeaveGame so server cleans up session.
	_ = gc.SendRequest(game.ClientPlayerLoginLeaveGameMessageId, &login.LeaveGameRequest{})
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
