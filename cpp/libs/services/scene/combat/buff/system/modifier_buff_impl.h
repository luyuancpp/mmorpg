#pragma once
#include <entt/src/entt/entity/entity.hpp>

#include "combat/skill/comp/skill_comp.h"

class BuffTable;
struct BuffEntry;

class ModifierBuffImplSystem {
public:
	// On buff start
	static void OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	// On buff refresh
	static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffEntry& buffComp);

	// On buff remove
	static void OnBuffRemove(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	// On buff destroy
	static void OnBuffDestroy(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);
	
	static void OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);

	static void OnSkillHit(entt::entity caster, entt::entity target);
private:
	// Check if buff affects movement speed
	static bool IsMovementSpeedBuff(const BuffTable* buffTable);
};


