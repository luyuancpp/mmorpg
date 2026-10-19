package handler

import (
	"go.uber.org/zap"

	"proto/scene"
	"robot/logic/gameobject"
)

func SceneSkillClientPlayerListSkillsHandler(player *gameobject.Player, response *scene.ListSkillsResponse) {
	if response.SkillList == nil {
		player.SetOwnedSkillIDs(nil)
		return
	}
	// 职业被动(catalog 里 passive = true)也在已拥有列表里,但没有施放动作,服务端会拒绝 ReleaseSkill;
	// robot 把这张表当"可施放技能"随机挑,所以先把被动剔掉(docs/design/class-skills.md §4)。
	passive := make(map[uint32]bool, len(response.Catalog))
	for _, entry := range response.Catalog {
		if entry.Passive {
			passive[entry.SkillTableId] = true
		}
	}
	var ids []uint32
	for _, s := range response.SkillList.SkillList {
		if passive[s.SkillTableId] {
			continue
		}
		ids = append(ids, s.SkillTableId)
	}
	player.SetOwnedSkillIDs(ids)
	zap.L().Info("received skill list", zap.Uint64("player", player.ID), zap.Uint32s("skills", ids))
}
