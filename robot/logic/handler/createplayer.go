package handler

import (
	"client/logic/behaviortree"
	"client/pb/game"
	"client/pkg"
)

func CreatePlayerHandler(client *pkg.GameClient, response *game.CreatePlayerResponse) {
	if response.ErrorMessage.Id > 0 {
		return
	}
	client.Blackboard.SetMem(behaviortree.PlayerListIdentifier, response.Players)
}
