package main

import (
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

	"robot/config"
	"robot/logic/handler"
	"robot/pkg"

	"robot/generated/pb/game"
	"robot/proto/common/base"
	"robot/proto/login"

	"github.com/golang/protobuf/proto"

	// Blank-import all proto packages so they register with protoregistry.
	// The muduo TcpCodec.Decode uses protoregistry to resolve incoming types.
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

	host, portStr, err := net.SplitHostPort(cfg.GateAddr)
	if err != nil {
		zap.L().Fatal("invalid gate_addr", zap.String("addr", cfg.GateAddr), zap.Error(err))
	}
	port, _ := strconv.Atoi(portStr)

	zap.L().Info("starting robots",
		zap.String("gate", cfg.GateAddr),
		zap.Int("count", cfg.RobotCount),
	)

	var wg sync.WaitGroup
	for i := 1; i <= cfg.RobotCount; i++ {
		account := fmt.Sprintf(cfg.AccountFmt, i)
		wg.Add(1)
		go func(account string) {
			defer wg.Done()
			runRobot(host, port, account, cfg.Password)
		}(account)
		// Stagger connections to avoid thundering herd.
		time.Sleep(50 * time.Millisecond)
	}

	// Wait for interrupt signal.
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	<-sig
	zap.L().Info("shutting down")
}

// runRobot is the lifecycle for a single robot (one goroutine).
func runRobot(host string, port int, account, password string) {
	gc, err := pkg.NewGameClient(host, port)
	if err != nil {
		zap.L().Error("connect failed", zap.String("account", account), zap.Error(err))
		return
	}
	defer gc.Close()

	gc.Account = account

	// Give the connection time to establish.
	time.Sleep(500 * time.Millisecond)

	// Step 1: Login
	if err := gc.SendRequest(game.ClientPlayerLoginLoginMessageId, &login.LoginRequest{
		Account:  account,
		Password: password,
	}); err != nil {
		zap.L().Error("send login", zap.String("account", account), zap.Error(err))
		return
	}

	// Step 2: Read the LoginResponse inline (before entering the generic recv loop).
	loginResp, err := waitForMessage(gc, game.ClientPlayerLoginLoginMessageId)
	if err != nil {
		zap.L().Error("login response", zap.String("account", account), zap.Error(err))
		return
	}
	var lr login.LoginResponse
	if err := proto.Unmarshal(loginResp.SerializedMessage, &lr); err != nil {
		zap.L().Error("unmarshal login response", zap.Error(err))
		return
	}
	if lr.ErrorMessage != nil {
		zap.L().Error("login error", zap.String("account", account), zap.Any("err", lr.ErrorMessage))
		return
	}

	// Step 3: Create player if none exist.
	if len(lr.Players) == 0 {
		if err := gc.SendRequest(game.ClientPlayerLoginCreatePlayerMessageId, &login.CreatePlayerRequest{}); err != nil {
			zap.L().Error("send create player", zap.Error(err))
			return
		}
		createResp, err := waitForMessage(gc, game.ClientPlayerLoginCreatePlayerMessageId)
		if err != nil {
			zap.L().Error("create player response", zap.Error(err))
			return
		}
		var cr login.CreatePlayerResponse
		if err := proto.Unmarshal(createResp.SerializedMessage, &cr); err != nil {
			zap.L().Error("unmarshal create response", zap.Error(err))
			return
		}
		if cr.ErrorMessage != nil {
			zap.L().Error("create error", zap.Any("err", cr.ErrorMessage))
			return
		}
		lr.Players = cr.Players
	}

	if len(lr.Players) == 0 {
		zap.L().Error("no players after create", zap.String("account", account))
		return
	}

	playerId := lr.Players[0].GetPlayer().GetPlayerId()

	// Step 4: Enter game.
	if err := gc.SendRequest(game.ClientPlayerLoginEnterGameMessageId, &login.EnterGameRequest{
		PlayerId: playerId,
	}); err != nil {
		zap.L().Error("send enter game", zap.Error(err))
		return
	}
	enterResp, err := waitForMessage(gc, game.ClientPlayerLoginEnterGameMessageId)
	if err != nil {
		zap.L().Error("enter game response", zap.Error(err))
		return
	}
	var er login.EnterGameResponse
	if err := proto.Unmarshal(enterResp.SerializedMessage, &er); err != nil {
		zap.L().Error("unmarshal enter response", zap.Error(err))
		return
	}
	if er.ErrorMessage != nil {
		zap.L().Error("enter game error", zap.Any("err", er.ErrorMessage))
		return
	}

	gc.PlayerId = er.PlayerId
	zap.L().Info("entered game", zap.String("account", account), zap.Uint64("player_id", gc.PlayerId))

	// Register in global player list so handlers can look up the player.
	handler.RegisterPlayer(gc)

	// Step 5: Dispatch remaining messages through the handler table.
	gc.RecvLoop(func(client *pkg.GameClient, msg *base.MessageContent) {
		handler.MessageBodyHandler(client, msg)
	})
}

// waitForMessage blocks until a MessageContent with the given messageId arrives.
func waitForMessage(gc *pkg.GameClient, messageId uint32) (*base.MessageContent, error) {
	for {
		raw, err := gc.RecvOne()
		if err != nil {
			return nil, err
		}
		if raw.MessageId == messageId {
			return raw, nil
		}
		// Discard or log server-push messages that arrive before our expected response.
		zap.L().Debug("skipping unexpected msg during login", zap.Uint32("got", raw.MessageId), zap.Uint32("want", messageId))
	}
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
