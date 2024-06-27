package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
)

func CreatePlayerHandler(client *pkg.GameClient, response *game.CreatePlayerResponse) {
	if response.Error.Id > 0 {
		return
	}
	logic.MainPlayer = logic.NewPlayer(response.Players[0].Player.PlayerId, client)
	rq := &game.EnterGameRequest{PlayerId: logic.MainPlayer.PlayerId}
	client.Send(rq, 52)
}
