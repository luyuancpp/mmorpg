package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func ClientPlayerSceneServiceNotifyActorCreateHandler(player *gameobject.Player, response *game.ActorCreateS2C) {
	client := player.GetClient().(*pkg.GameClient)
	actorListFromBlackboard := client.Blackboard.GetMem(behaviortree.ActorListBoardKey)

	rawActorList, ok := actorListFromBlackboard.(*gameobject.ActorList)
	if !ok {
		rawActorList = gameobject.NewActorList()
		client.Blackboard.SetMem(behaviortree.ActorListBoardKey, rawActorList)
	}

	rawActorList.AddActor(response.Entity)
}
