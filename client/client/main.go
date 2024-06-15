package main

import (
	"client/pb/game"
	"client/pkg"
	muduo "github.com/luyuancpp/muduoclient/muduo"
	"log"
	"time"
)

func main() {
	client, err := muduo.NewClient("127.0.0.1", 8000)
	if err != nil {
		log.Fatalln(err)
	}
	gameClient := pkg.NewGameClient(client)

	defer gameClient.Close()

	rq := &game.LoginRequest{Account: "luhailong", Password: "luhailong"}

	gameClient.Send(rq)
	for {
		time.Sleep(1 * time.Second)
	}
}
