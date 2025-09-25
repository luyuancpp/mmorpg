package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pkg"
	"robot/proto/service/cpp/rpc/scene"
)

func SceneSceneClientPlayerNotifyActorDestroyHandler(player *gameobject.Player, response *scene.ActorDestroyS2C) {
	client := player.GetClient().(*pkg.GameClient)
	actorListFromBlackboard := client.Blackboard.GetMem(behaviortree.ActorListBoardKey)

	rawActorList, ok := actorListFromBlackboard.(*gameobject.ActorList)
	if !ok {
		return
	}

	rawActorList.RemoveActor(response.Entity)
}
