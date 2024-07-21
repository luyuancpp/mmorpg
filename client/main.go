package main

import (
	"client/logic/handler"
	"client/pb/game"
	"client/pkg"
	"go.uber.org/zap"

	"github.com/luyuancpp/muduoclient/muduo"
	"log"
	"strconv"
	"time"
)

func main() {

	logger, err := zap.NewProduction()
	if err != nil {
		panic(err)
	}
	lvl := zap.NewAtomicLevel()
	lvl.SetLevel(zap.ErrorLevel)
	defer logger.Sync()
	zap.ReplaceGlobals(logger)

	for i := 1; i < 6; i++ {
		go func(i int) {
			client, err := muduo.NewClient("127.0.0.1", 8000, &muduo.TcpCodec{})
			if err != nil {
				log.Fatalln(err)
			}
			gameClient := pkg.NewGameClient(client)
			defer gameClient.Close()

			{
				rq := &game.LoginRequest{Account: "luhailong" + strconv.Itoa(i), Password: "luhailong"}
				gameClient.Send(rq, 34)
			}
			for {
				msg := <-gameClient.Client.Conn.InMsgList
				d := muduo.GetDescriptor(&msg)
				if d.Name() == "LoginResponse" {
					resp := msg.(*game.LoginResponse)
					handler.LoginHandler(gameClient, resp)
				} else if d.Name() == "CreatePlayerResponse" {
					resp := msg.(*game.CreatePlayerResponse)
					handler.CreatePlayerHandler(gameClient, resp)
				} else if d.Name() == "MessageBody" {
					resp := msg.(*game.MessageBody)
					handler.MessageBodyHandler(gameClient, resp)
				}
			}

		}(i)

	}

	time.Sleep(10000000 * time.Second)
}
