package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
	"log"
)

func CreatePlayerHandler(client *pkg.GameClient, response *game.CreatePlayerResponse) {
	if response.Error.Id > 0 {
		return
	}
	player := logic.NewMainPlayer(response.Players[0].Player.PlayerId, client)
	log.Println(player.Client.PlayerId)
	rq := &game.EnterGameRequest{PlayerId: player.Client.PlayerId}
	client.Send(rq, 52)
}
