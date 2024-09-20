package handler

import (
	"robot/logic/behaviortree"
	"robot/logic/gameobject"
	"robot/pb/game"
	"robot/pkg"
)

func PlayerSkillServiceGetSkillListHandler(player *gameobject.Player, response *game.GetSkillListResponse) {
	client := player.Client.(*pkg.GameClient)

	playerSkillListPBComp := &game.PlayerSkillListPBComp{}

	for _, v := range response.SkillList.SkillList {
		playerSkillListPBComp.SkillList = append(playerSkillListPBComp.SkillList, v)
	}

	client.Blackboard.SetMem(behaviortree.SkillListBoardKey, playerSkillListPBComp)
}
