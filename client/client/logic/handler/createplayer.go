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
	player := logic.NewPlayer(response.Players[0].Player.PlayerId, client)
	player.EnterGame()
}
