package main

import (
	"github.com/golang/protobuf/proto"
	"github.com/luyuancpp/muduoclient/muduo"
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"log"
	"net/http"
	_ "net/http/pprof"
	"robot/config"
	"robot/logic/gameobject"
	"robot/logic/handler"
	"robot/pb/game"
	"robot/pkg"
	"strconv"
	"sync"
	"time"
)

func runClientLoop(gameClient *pkg.GameClient) {
	msgCh := make(chan proto.Message)
	errCh := make(chan error)

	go func() {
		defer close(msgCh)
		for {
			msg, err := gameClient.Client.Recv()
			if err != nil {
				errCh <- err
				return
			}
			msgCh <- msg
		}
	}()

	for {
		select {
		case msg, ok := <-msgCh:
			if !ok {
				zap.L().Info("Message channel closed, exiting runClientLoop")
				return
			}
			d := muduo.GetDescriptor(&msg)
			switch d.Name() {
			case "LoginResponse":
				resp := msg.(*game.LoginResponse)
				handler.ClientPlayerLoginLoginHandler(gameClient, resp)
				gameClient.TickBehaviorTree()
			case "CreatePlayerResponse":
				resp := msg.(*game.CreatePlayerResponse)
				handler.ClientPlayerLoginCreatePlayerHandler(gameClient, resp)
				gameClient.TickBehaviorTree()
			case "EnterGameResponse":
				resp := msg.(*game.EnterGameResponse)
				handler.ClientPlayerLoginEnterGameHandler(gameClient, resp)
			case "MessageContent":
				resp := msg.(*game.MessageContent)
				handler.MessageBodyHandler(gameClient, resp)
				player, ok := gameobject.PlayerList.Get(gameClient.PlayerId)
				if ok {
					player.TickBehaviorTree()
				}
			default:
				zap.L().Warn("Unhandled message type", zap.String("message_type", string(d.Name())))
			}
		case err := <-errCh:
			zap.L().Error("Recv error, exiting runClientLoop", zap.Error(err))
			return
		case <-time.After(20 * time.Millisecond):
			player, ok := gameobject.PlayerList.Get(gameClient.PlayerId)
			if ok {
				player.TickBehaviorTree()
			} else {
				gameClient.TickBehaviorTree()
			}
		}
	}
}

func main() {
	go func() {
		log.Fatal(http.ListenAndServe("localhost:6060", nil))
	}()

	logger, err := zap.NewProduction()
	if err != nil {
		panic(err)
	}
	defer logger.Sync()
	zap.ReplaceGlobals(logger)

	lvl := zap.NewAtomicLevel()
	lvl.SetLevel(zapcore.Level(config.AppConfig.LogLevel))

	var wg sync.WaitGroup

	for i := 0; i < config.AppConfig.Robots.Count; i++ {
		time.Sleep(200 * time.Millisecond)
		wg.Add(1)

		go func(i int) {
			defer wg.Done()

			// 每个机器人分配一个服务器（轮询）
			serverIndex := i % len(config.AppConfig.Servers)
			server := config.AppConfig.Servers[serverIndex]

			client, err := muduo.NewClient(server.IP, server.Port, &muduo.TcpCodec{})
			if err != nil {
				zap.L().Error("Failed to create client", zap.String("ip", server.IP), zap.Int("port", server.Port), zap.Error(err))
				return
			}
			gameClient := pkg.NewGameClient(client)
			defer gameClient.Close()

			gameClient.Account = "luhailong" + strconv.Itoa(i)

			runClientLoop(gameClient)
		}(i)
	}

	wg.Wait()
}
