#pragma once
#include "table/code/buff_table.h"
#include <entt/src/entt/entity/entity.hpp>

#include "scene/combat/skill/comp/skill_comp.h"

struct BuffComp;

class ModifierBuffImplSystem {
public:
	// Buff 启动时的处理
	static bool OnBuffStart(entt::entity parent, BuffComp& buff, const BuffTable* buffTable);

	// Buff 刷新时的处理
	static void OnBuffRefresh(entt::entity parent, uint32_t buffTableId, const SkillContextPtrComp& abilityContext, BuffComp& buffComp);

	// Buff 移除时的处理
	static bool OnBuffRemove(entt::entity parent, BuffComp& buff, const BuffTable* buffTable);

	// Buff 销毁时的处理
	static bool OnBuffDestroy(entt::entity parent, BuffComp& buff, const BuffTable* buffTable);
	
	static bool OnIntervalThink(entt::entity parent, BuffComp& buffComp, const BuffTable* buffTable);

	static void OnSkillHit(entt::entity caster, entt::entity target);
private:
	// 检查是否为速度相关的 Buff
	static bool IsMovementSpeedBuff(const BuffTable* buffTable);
};


