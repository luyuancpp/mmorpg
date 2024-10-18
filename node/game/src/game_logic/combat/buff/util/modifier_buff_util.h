#pragma once
#include <entt/src/entt/entity/entity.hpp>

#include "game_logic/combat/skill/comp/skill_comp.h"

struct BuffComp;
class BuffTable;

class ModifierBuffUtil {
public:
	static bool OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable);
	
	static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffComp& buffComp);

	static bool OnBuffRemove(entt::entity parent, uint64_t buffId);

	static bool OnBuffDestroy(entt::entity parent, uint32_t buffTableId);
	
	static void ModifyState(entt::entity parent, uint64_t buffId);
	
	static void ModifyAttributes(entt::entity parent, uint64_t buffId);
};


