package handler

import (
	"robot/logic/behaviortree"
	"robot/pb/game"
	"robot/pkg"
)

func CreatePlayerHandler(client *pkg.GameClient, response *game.CreatePlayerResponse) {
	if response.ErrorMessage.Id > 0 {
		return
	}
	client.Blackboard.SetMem(behaviortree.PlayerListBoardKey, response.Players)
}
