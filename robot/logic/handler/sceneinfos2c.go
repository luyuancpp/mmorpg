package handler

import (
	"robot/logic"
	"robot/logic/behaviortree"
	"robot/pb/game"
	"robot/pkg"
)

func SceneInfoS2CHandler(player *logic.Player, response *game.SceneInfoS2C) {
	client := player.Client.(*pkg.GameClient)
	client.Blackboard.SetMem(behaviortree.SceneInformationKey, response.SceneInfo)
}
