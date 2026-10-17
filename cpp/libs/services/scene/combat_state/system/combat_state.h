#pragma once
#include <cstdint>
#include <entt/src/entt/entity/entity.hpp>

class CombatStateSystem
{
public:
    static void AddCombatState(entt::entity actor, uint64_t sourceBuffId, uint32_t stateType);
    
    static void RemoveCombatState(entt::entity actor, uint64_t sourceBuffId, uint32_t stateType);

    static uint32_t ValidateSkillUsage(entt::entity entityId, uint32_t combatAction);
};
