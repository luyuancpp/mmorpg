package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func SceneSceneClientPlayerNotifySceneInfoHandler(player *gameobject.Player, response *game.SceneInfoS2C) {
	client := player.Client.(*pkg.GameClient)
	client.Blackboard.SetMem(behaviortree.SceneInformationBoardKey, response.SceneInfo)
}
