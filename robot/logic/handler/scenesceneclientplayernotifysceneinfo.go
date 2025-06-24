package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pb/game"
)

func SceneSceneClientPlayerNotifySceneInfoHandler(player *gameobject.Player, response *game.SceneInfoS2C) {
	player.Blackboard.SetMem(behaviortree.SceneInformationBoardKey, response.SceneInfo)
}
