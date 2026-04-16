#pragma once
#include <entt/src/entt/entity/entity.hpp>

class CombatStateAddedEvent;
class CombatStateRemovedEvent;

class CombatStateSystem
{
public:
    static void AddCombatState(const CombatStateAddedEvent& event);
    
    static void RemoveCombatState(const CombatStateRemovedEvent& event);

    static uint32_t ValidateSkillUsage(entt::entity entityId, uint32_t combatAction);
};