package handler

import (
	"robot/logic/behaviortree"
	"robot/pb/game"
	"robot/pkg"
)

func ClientPlayerLoginLoginHandler(client *pkg.GameClient, response *game.LoginResponse) {
	client.Blackboard.SetMem(behaviortree.PlayerListBoardKey, response.Players)
}
