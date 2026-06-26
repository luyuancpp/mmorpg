#pragma once
#include <entt/src/entt/entity/entity.hpp>

#include "combat/skill/comp/skill_comp.h"

struct BuffEntry;
class BuffTable;

class MotionModifierBuffImplSystem{
public:
    static void OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffEntry& buffComp);

	static void OnBuffRemove(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	static void OnBuffDestroy(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	static void OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);
	
	static void OnSkillHit(entt::entity caster, entt::entity target);

};