#pragma once
#include <entt/src/entt/entity/entity.hpp>

class CombatStateAddedPbEvent;
class CombatStateRemovedPbEvent;

class CombatStateUtil
{
public:
    static void InitializeCombatStateComponent(entt::entity entity);

    static void AddCombatState(const CombatStateAddedPbEvent& event);
    
    static void RemoveCombatState(const CombatStateRemovedPbEvent& event);

    static uint32_t TryUseSkill(entt::entity entityId, uint32_t combatAction);
};