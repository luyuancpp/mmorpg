#pragma once
#include <entt/src/entt/entity/entity.hpp>

#include "combat/skill/comp/skill_comp.h"

struct BuffEntry;
class BuffTable;

class MotionModifierBuffImplSystem{
public:
    static bool OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffEntry& buffComp);

	static bool OnBuffRemove(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	static bool OnBuffDestroy(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	static bool OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);
	
	static void OnSkillHit(entt::entity caster, entt::entity target);

};