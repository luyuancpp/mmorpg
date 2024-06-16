package main

import (
	"client/pb/game"
	"client/pkg"
	"github.com/luyuancpp/muduoclient/muduo"
	"log"
	"strconv"
	"time"
)

func main() {

	for i := 0; i < 4; i++ {
		go func(i int) {
			client, err := muduo.NewClient("127.0.0.1", 8000)
			if err != nil {
				log.Fatalln(err)
			}
			gameClient := pkg.NewGameClient(client)

			defer gameClient.Close()

			{
				rq := &game.LoginRequest{Account: "luhailong" + strconv.Itoa(i), Password: "luhailong"}
				gameClient.Send(rq, 34)
			}
			msg := <-gameClient.Client.Conn.InMsgList

			loginResp := msg.(*game.LoginResponse)
			if loginResp.Players == nil || len(loginResp.Players) <= 0 {
				rq := &game.CreatePlayerRequest{}
				gameClient.Send(rq, 33)
				msg = <-gameClient.Client.Conn.InMsgList
				createPlayer := msg.(*game.CreatePlayerResponse)
				if nil != createPlayer.Error {
					return
				}
				log.Println(createPlayer)
			}
		}(i)

	}

	time.Sleep(1000 * time.Second)
}
