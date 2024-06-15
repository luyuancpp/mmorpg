package main

import (
	"client/pb/game"
	"client/pkg"
	"github.com/luyuancpp/muduoclient/muduo"
	"log"
	"time"
)

func main() {

	for i := 0; i < 10000; i++ {
		client, err := muduo.NewClient("127.0.0.1", 8000)
		if err != nil {
			log.Fatalln(err)
		}
		gameClient := pkg.NewGameClient(client)

		defer gameClient.Close()

		rq := &game.LoginRequest{Account: "luhailong", Password: "luhailong"}

		gameClient.Send(rq)
		time.Sleep(50 * time.Millisecond)
	}
}
