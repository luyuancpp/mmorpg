#include "player_skill.h"

#include "table/code/class_table.h"
#include "proto/common/component/player_skill_comp.pb.h"

#include <thread_context/registry_manager.h>

void PlayerSkillSystem::RegisterPlayer(entt::entity player)
{
	for (const auto& tableClass : GetClassAllTable().data())
	{
		for (auto& skillId : tableClass.skill())
		{
			auto pbSkill = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSkillListComp>(player).add_skill_list();
			pbSkill->set_skill_table_id(skillId);
		}
	}
}

