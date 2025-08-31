#pragma once
#include "table/buff_table.h"
#include <entt/src/entt/entity/entity.hpp>

#include "scene/combat/skill/comp/skill_comp.h"

struct BuffComp;

class MotionModifierBuffImplSystem{
public:
    static bool OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable);

	static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffComp& buffComp);

	static bool OnBuffRemove(entt::entity parent, BuffComp& buff, const BuffTable* buffTable);

	static bool OnBuffDestroy(entt::entity parent, BuffComp& buff, const BuffTable* buffTable);

	static bool OnIntervalThink(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable);
	
	static void OnSkillHit(entt::entity caster, entt::entity target);

};