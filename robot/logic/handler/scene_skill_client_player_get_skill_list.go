package handler

import (
	"go.uber.org/zap"

	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSkillClientPlayerGetSkillListHandler(player *gameobject.Player, response *scene.GetSkillListResponse) {
	if response.SkillList == nil {
		return
	}
	var ids []uint32
	for _, s := range response.SkillList.SkillList {
		ids = append(ids, s.SkillTableId)
	}
	player.SetOwnedSkillIDs(ids)
	zap.L().Info("received skill list", zap.Uint64("player", player.ID), zap.Uint32s("skills", ids))
}
