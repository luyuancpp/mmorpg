package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func SceneSceneClientPlayerNotifyActorListDestroyHandler(player *gameobject.Player, response *game.ActorListDestroyS2C) {
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
