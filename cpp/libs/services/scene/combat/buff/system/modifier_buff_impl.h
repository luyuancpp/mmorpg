#pragma once
#include "table/code/buff_table.h"
#include <entt/src/entt/entity/entity.hpp>

#include "combat/skill/comp/skill.h"

struct BuffEntry;

class ModifierBuffImplSystem {
public:
	// On buff start
	static bool OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	// On buff refresh
	static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffEntry& buffComp);

	// On buff remove
	static bool OnBuffRemove(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	// On buff destroy
	static bool OnBuffDestroy(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);
	
	static bool OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);

	static void OnSkillHit(entt::entity caster, entt::entity target);
private:
	// Check if buff affects movement speed
	static bool IsMovementSpeedBuff(const BuffTable* buffTable);
};


