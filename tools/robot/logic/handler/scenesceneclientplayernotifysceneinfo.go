package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/proto/service/cpp/rpc/scene"
)

func SceneSceneClientPlayerNotifySceneInfoHandler(player *gameobject.Player, response *scene.SceneInfoS2C) {
	player.Blackboard.SetMem(behaviortree.SceneInformationBoardKey, response.SceneInfo)
}
