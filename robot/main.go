package main

import (
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

func main() {
	go func() {
		log.Fatal(http.ListenAndServe("localhost:6060", nil))
	}()
	// 初始化日志记录器
	logger, err := zap.NewProduction()
	if err != nil {
		panic(err)
	}
	defer logger.Sync()
	zap.ReplaceGlobals(logger)

	// 配置日志级别
	lvl := zap.NewAtomicLevel()
	lvl.SetLevel(zapcore.Level(config.AppConfig.LogLevel)) // 根据需要调整日志级别

	var wg sync.WaitGroup

	for i := 0; i < config.AppConfig.Robots.Count; i++ {
		time.Sleep(100 * time.Millisecond)
		wg.Add(1)
		go func(i int) {
			defer wg.Done()

			client, err := muduo.NewClient(config.AppConfig.Server.IP, config.AppConfig.Server.Port, &muduo.TcpCodec{})
			if err != nil {
				zap.L().Error("Failed to create client", zap.Error(err))
				return
			}
			gameClient := pkg.NewGameClient(client)
			defer gameClient.Close()

			// 登录请求
			rq := &game.LoginRequest{Account: "luhailong" + strconv.Itoa(i), Password: "luhailong"}
			gameClient.Send(rq, game.ClientPlayerLoginLoginMessageId)

			// 处理消息
			for {
				select {
				case msg := <-gameClient.Client.Conn.InMsgList:
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
				case <-time.After(20 * time.Millisecond):
					gameClient.TickBehaviorTree()
				case <-time.After(20 * time.Millisecond):
					player, ok := gameobject.PlayerList.Get(gameClient.PlayerId)
					if ok {
						player.TickBehaviorTree()
					}
				}
			}
		}(i)
	}

	// 等待所有 goroutine 完成
	wg.Wait()
}
