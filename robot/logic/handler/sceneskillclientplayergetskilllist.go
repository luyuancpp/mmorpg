package handler

import (
	"go.uber.org/zap"
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func SceneSkillClientPlayerGetSkillListHandler(player *gameobject.Player, response *game.GetSkillListResponse) {
	client := player.Client.(*pkg.GameClient)

	actorListFromBlackboard := client.Blackboard.GetMem(behaviortree.SkillListBoardKey)

	playerSkillListPBComp, ok := actorListFromBlackboard.(*game.PlayerSkillListPBComponent)
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
