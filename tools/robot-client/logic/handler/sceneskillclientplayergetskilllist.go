package handler

import (
	"go.uber.org/zap"
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pkg"
	"robot/proto/logic/component"
	"robot/proto/service/rpc/scene"
)

func SceneSkillClientPlayerGetSkillListHandler(player *gameobject.Player, response *scene.GetSkillListResponse) {
	client := player.Client.(*pkg.GameClient)

	actorListFromBlackboard := client.Blackboard.GetMem(behaviortree.SkillListBoardKey)

	playerSkillListPBComp, ok := actorListFromBlackboard.(*component.PlayerSkillListPBComponent)
	if !ok {
		zap.L().Error("Failed to cast skill list from blackboard",
			zap.String("Key", behaviortree.SkillListBoardKey),
			zap.Any("Value", playerSkillListPBComp))
		return
	}

	playerSkillListPBComp.SkillList = playerSkillListPBComp.SkillList[:0]

	for _, v := range response.SkillList.SkillList {
		playerSkillListPBComp.SkillList = append(playerSkillListPBComp.SkillList, v)
	}
}
