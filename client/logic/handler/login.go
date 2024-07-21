package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
	"log"
)

func LoginHandler(client *pkg.GameClient, response *game.LoginResponse) {
	if response.Players == nil || len(response.Players) <= 0 {
		rq := &game.CreatePlayerRequest{}
		client.Send(rq, 33)
	} else {
		playerId := response.Players[0].Player.PlayerId
		log.Println(playerId)
		logic.PlayerList.Set(playerId, logic.NewMainPlayer(playerId, client))
		rq := &game.EnterGameRequest{PlayerId: playerId}
		client.Send(rq, 52)
	}
}
