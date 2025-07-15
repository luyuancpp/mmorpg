package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func SceneSceneClientPlayerNotifyActorDestroyHandler(player *gameobject.Player, response *game.ActorDestroyS2C) {
	client := player.GetClient().(*pkg.GameClient)
	actorListFromBlackboard := client.Blackboard.GetMem(behaviortree.ActorListBoardKey)

	rawActorList, ok := actorListFromBlackboard.(*gameobject.ActorList)
	if !ok {
		return
	}

	rawActorList.RemoveActor(response.Entity)
}
