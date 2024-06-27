package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
)

func LoginHandler(client *pkg.GameClient, response *game.LoginResponse) {
	if response.Players == nil || len(response.Players) <= 0 {
		rq := &game.CreatePlayerRequest{}
		client.Send(rq, 33)
	} else {
		logic.MainPlayer = logic.NewPlayer(response.Players[0].Player.PlayerId, client)
		rq := &game.EnterGameRequest{PlayerId: logic.MainPlayer.PlayerId}
		client.Send(rq, 52)
	}
}
