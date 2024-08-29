package handler

import (
	"client/logic/behaviortree"
	"client/pb/game"
	"client/pkg"
)

func LoginHandler(client *pkg.GameClient, response *game.LoginResponse) {
	client.Blackboard.SetMem(behaviortree.PlayerListIdentifier, response.Players)
}
