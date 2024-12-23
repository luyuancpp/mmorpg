#include "player_skill_util.h"

#include "class_config.h"
#include "proto/logic/component/player_skill_comp.pb.h"
#include "thread_local/storage.h"

void PlayerSkillUtil::InitializePlayerComponentsHandler(entt::entity player)
{

}

void PlayerSkillUtil::RegisterPlayer(entt::entity player)
{
	for (const auto& tableClass : GetClassAllTable().data())
	{
		auto pbSkill = tls.registry.get<PlayerSkillListPBComponent>(player).add_skill_list();

		for (auto& skillId : tableClass.skill())
		{
            pbSkill->set_skill_table_id(skillId);
		}
	}
}
