package handler

import (
	"robot/logic"
	"robot/pb/game"
	"robot/pkg"
)

func ClientPlayerSceneServiceNotifySceneInfoHandler(player *logic.Player, response *game.SceneInfoS2C) {
	client := player.Client.(*pkg.GameClient)
	client.Blackboard.SetMem(behaviortree.SceneInformationKey, response.SceneInfo)
}
