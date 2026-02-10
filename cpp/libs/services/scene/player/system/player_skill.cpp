#include "player_skill.h"

#include "table/code/class_table.h"
#include "proto/common/component/player_skill_comp.pb.h"

#include <threading/registry_manager.h>

void PlayerSkillSystem::InitializePlayerComponentsHandler(entt::entity player)
{

}

void PlayerSkillSystem::RegisterPlayer(entt::entity player)
{
	for (const auto& tableClass : GetClassAllTable().data())
	{
		auto pbSkill = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerSkillListPBComponent>(player).add_skill_list();

		for (auto& skillId : tableClass.skill())
		{
            pbSkill->set_skill_table_id(skillId);
		}
	}
}
