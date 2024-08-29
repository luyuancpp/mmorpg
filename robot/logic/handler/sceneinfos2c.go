package handler

import (
	"client/logic"
	"client/logic/behaviortree"
	"client/pb/game"
	"client/pkg"
)

func SceneInfoS2CHandler(player *logic.Player, response *game.SceneInfoS2C) {
	client := player.Client.(*pkg.GameClient)
	client.Blackboard.SetMem(behaviortree.SceneInformationKey, response.SceneInfo)
}
