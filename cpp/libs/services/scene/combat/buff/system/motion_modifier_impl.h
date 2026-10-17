#pragma once
#include <entt/src/entt/entity/entity.hpp>

struct BuffEntry;
class BuffTable;

class MotionModifierBuffImplSystem{
public:
    static void OnBuffStart(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	static void OnBuffRemove(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	static void OnBuffDestroy(entt::entity parent, BuffEntry& buff, const BuffTable* buffTable);

	static void OnIntervalThink(entt::entity parent, BuffEntry& buffComp, const BuffTable* buffTable);
	
	static void OnSkillHit(entt::entity caster, entt::entity target);

};
