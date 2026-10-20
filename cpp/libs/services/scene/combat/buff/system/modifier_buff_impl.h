#pragma once
#include <entt/src/entt/entity/entity.hpp>

class BuffTable;
struct BuffEntry;

class ModifierBuffImplSystem {
public:
	// On buff start
	static void OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	// On buff remove
	static void OnBuffRemove(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	// On buff destroy
	static void OnBuffDestroy(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);
	
	static void OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);

	static void OnSkillHit(entt::entity caster, entt::entity target);
};


