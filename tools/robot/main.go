package main

import (
	"github.com/luyuancpp/muduoclient-new/muduo"
	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"google.golang.org/protobuf/proto"
	"log"
	"net/http"
	_ "net/http/pprof"
	"robot/config"
	"robot/logic/gameobject"
	"robot/logic/handler"
	"robot/pkg"
	"robot/proto/common"
	"robot/proto/service/go/grpc/login"
	"strconv"
	"sync"
	"time"
)

func runClientLoop(gameClient *pkg.GameClient) {
	msgCh := make(chan proto.Message)
	errCh := make(chan error)

	go func() {
		defer func() {
			zap.L().Info("Recv goroutine is closing")
			close(msgCh)
		}()
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
			d := msg.ProtoReflect().Descriptor()
			switch d.Name() {
			case "LoginResponse":
				resp := msg.(*login.LoginResponse)
				handler.ClientPlayerLoginLoginHandler(gameClient, resp)
				gameClient.TickBehaviorTree()
			case "CreatePlayerResponse":
				resp := msg.(*login.CreatePlayerResponse)
				handler.ClientPlayerLoginCreatePlayerHandler(gameClient, resp)
				gameClient.TickBehaviorTree()
			case "EnterGameResponse":
				resp := msg.(*login.EnterGameResponse)
				handler.ClientPlayerLoginEnterGameHandler(gameClient, resp)
			case "MessageContent":
				resp := msg.(*common.MessageContent)
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
		case <-time.After(time.Duration(config.AppConfig.Robots.Tick) * time.Millisecond):
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
		time.Sleep(time.Duration(config.AppConfig.Robots.LoginInterval) * time.Millisecond)
		wg.Add(1)

		go func(i int) {
			defer wg.Done()

			// 每个机器人分配一个服务器（轮询）
			serverIndex := i % len(config.AppConfig.Servers)
			server := config.AppConfig.Servers[serverIndex]

			client := muduo.NewTcpClient(server.Address, &muduo.TcpCodec{})
			gameClient := pkg.NewGameClient(client)
			defer gameClient.Close()

			gameClient.Account = "luhailong" + strconv.Itoa(i)

			runClientLoop(gameClient)
		}(i)
	}

	wg.Wait()
}
