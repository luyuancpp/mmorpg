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
		logic.GMainPlayer = logic.NewMainPlayer(response.Players[0].Player.PlayerId, client)
		rq := &game.EnterGameRequest{PlayerId: logic.GMainPlayer.PlayerId}
		client.Send(rq, 52)
	}
}
