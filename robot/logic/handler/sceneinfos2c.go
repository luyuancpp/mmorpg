package handler

import (
	"client/logic"
	"client/logic/behaviortree"
	"client/pb/game"
	"client/pkg"
	"github.com/mohae/deepcopy"
)

func SceneInfoS2CHandler(player *logic.Player, response *game.SceneInfoS2C) {
	client := player.Client.(*pkg.GameClient)
	sceneInfo := deepcopy.Copy(response.SceneInfo).([]*game.SceneInfoPBComp)
	client.Blackboard.SetMem(behaviortree.SceneInfo, sceneInfo)
}
