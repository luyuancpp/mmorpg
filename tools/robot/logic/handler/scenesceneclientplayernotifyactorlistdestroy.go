package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pkg"
	"robot/proto/service/cpp/rpc/scene"
)

func SceneSceneClientPlayerNotifyActorListDestroyHandler(player *gameobject.Player, response *scene.ActorListDestroyS2C) {
	client := player.GetClient().(*pkg.GameClient)
	actorListFromBlackboard := client.Blackboard.GetMem(behaviortree.ActorListBoardKey)

	rawActorList, ok := actorListFromBlackboard.(*gameobject.ActorList)
	if !ok {
		return
	}

	for _, value := range response.Entity {
		rawActorList.RemoveActor(value)
	}
}
