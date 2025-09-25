package handler

import (
	"go.uber.org/zap"
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func SceneSceneClientPlayerNotifyActorCreateHandler(player *gameobject.Player, response *game.ActorCreateS2C) {
	client := player.GetClient().(*pkg.GameClient)

	actorListFromBlackboard := client.Blackboard.GetMem(behaviortree.ActorListBoardKey)

	rawActorList, ok := actorListFromBlackboard.(*gameobject.ActorList)
	if !ok {
		zap.L().Error("Failed to cast actor list from blackboard",
			zap.String("Key", behaviortree.ActorListBoardKey),
			zap.Any("Value", actorListFromBlackboard))
		return
	}

	// 添加演员到列表中
	rawActorList.AddActor(response.Entity)
}
