#include "player_skill.h"

#include <unordered_set>

#include "muduo/base/Logging.h"
#include "table/code/class_table.h"
#include "table/code/skill_table.h"
#include "proto/common/component/player_skill_comp.pb.h"
#include <thread_context/ecs_context.h>

namespace
{
	void AddSkillIfMissing(PlayerSkillListComp &skillList,
						   std::unordered_set<uint32_t> &seenSkillIds,
						   const uint32_t skillId)
	{
		if (skillId == 0)
		{
			return;
		}

		if (!SkillTableManager::Instance().Exists(skillId))
		{
			LOG_WARN << "Skipping unknown class skill_table_id=" << skillId;
			return;
		}

		if (!seenSkillIds.emplace(skillId).second)
		{
			return;
		}

		auto *pbSkill = skillList.add_skill_list();
		pbSkill->set_skill_table_id(skillId);
	}
}

bool PlayerSkillSystem::IsActiveSkill(const uint32_t skillTableId)
{
	return skillTableId != 0 && SkillTableManager::Instance().Exists(skillTableId);
}

void PlayerSkillSystem::RegisterPlayer(entt::entity player)
{
	auto* skillList = tlsEcs.actorRegistry.try_get<PlayerSkillListComp>(player);
	if (skillList == nullptr)
	{
		LOG_ERROR << "[PlayerSkill] 缺少持久 PlayerSkillListComp，无法登记默认技能，entity="
				  << entt::to_integral(player);
		return;
	}
	std::unordered_set<uint32_t> seenSkillIds;
	for (const auto &skill : skillList->skill_list())
	{
		if (skill.skill_table_id() != 0)
		{
			seenSkillIds.emplace(skill.skill_table_id());
		}
	}

	for (const auto &tableClass : FindAllClassTable().data())
	{
		for (const auto skillId : tableClass.skill())
		{
			AddSkillIfMissing(*skillList, seenSkillIds, skillId);
		}
	}
}

bool PlayerSkillSystem::HasSkill(entt::entity player, const uint32_t skillTableId)
{
	if (skillTableId == 0 || !tlsEcs.actorRegistry.valid(player))
	{
		return false;
	}
	// 本版本技能表查不到的技能留在列表里但不生效(见 SanitizeSkillList):对本版本来说就是"没有"。
	if (!IsActiveSkill(skillTableId))
	{
		return false;
	}

	const auto *skillList = tlsEcs.actorRegistry.try_get<PlayerSkillListComp>(player);
	if (skillList == nullptr)
	{
		return false;
	}

	for (const auto &skill : skillList->skill_list())
	{
		if (skill.skill_table_id() == skillTableId)
		{
			return true;
		}
	}

	return false;
}

void PlayerSkillSystem::SanitizeSkillList(entt::entity player)
{
	auto *skillList = tlsEcs.actorRegistry.try_get<PlayerSkillListComp>(player);
	if (skillList == nullptr)
	{
		return;
	}

	PlayerSkillListComp sanitizedSkillList;
	std::unordered_set<uint32_t> seenSkillIds;
	bool changed = false;
	uint32_t inactiveSkills = 0;

	for (const auto &skill : skillList->skill_list())
	{
		const auto skillId = skill.skill_table_id();
		if (skillId == 0)
		{
			LOG_WARN << "Removing player skill with skill_table_id=0 entity=" << entt::to_integral(player);
			changed = true;
			continue;
		}

		if (!seenSkillIds.emplace(skillId).second)
		{
			LOG_WARN << "Removing duplicate player skill_table_id=" << skillId
					 << " entity=" << entt::to_integral(player);
			changed = true;
			continue;
		}

		// 本版本技能表查不到的技能**保留但不生效**(docs/design/mixed-version-persist-field-protection.md §12,第 5 批):
		// 新版本加了技能后回滚 scene,旧表里没有这一行;以前这里直接删掉,旧版本一存盘玩家就永久失去这个技能。
		// 生效与否由消费侧按表判定:HasSkill、ListSkills、战斗快照都跳过查不到表的 id(IsActiveSkill)。
		if (!IsActiveSkill(skillId))
		{
			++inactiveSkills;
		}

		sanitizedSkillList.add_skill_list()->CopyFrom(skill);
	}

	if (inactiveSkills > 0)
	{
		// 每次加载只打一行汇总:混版本窗口里这是正常现象;表被裁掉技能时同一行也足够定位。
		LOG_WARN << "Keeping " << inactiveSkills
				 << " player skill(s) unknown to this version's SkillTable, inactive until the table knows them, entity="
				 << entt::to_integral(player);
	}

	if (changed)
	{
		// 整体替换会丢掉组件本层的未知字段(新版本给 PlayerSkillListComp 加的),先并过来再换
		// (docs/design/mixed-version-persist-field-protection.md §4.2 L5)。逐条技能经 CopyFrom 已各自带着。
		sanitizedSkillList.mutable_unknown_fields()->MergeFrom(skillList->unknown_fields());
		skillList->Swap(&sanitizedSkillList);
	}
}
