package handler

import (
	"client/logic"
	"client/pb/game"
	"client/pkg"
)

func LoginResponseHandler(client *pkg.GameClient, response *game.LoginResponse) {
	var player *logic.Player
	if response.Players == nil || len(response.Players) <= 0 {
		rq := &game.CreatePlayerRequest{}
		client.Send(rq, 33)
		msg := <-client.Client.Conn.InMsgList
		createPlayer := msg.(*game.CreatePlayerResponse)
		if createPlayer.Error.Id > 0 {
			return
		}
		player = logic.NewPlayer(createPlayer.Players[0].Player.PlayerId, client)
	} else {
		player = logic.NewPlayer(response.Players[0].Player.PlayerId, client)
	}
	player.EnterGame()
}
