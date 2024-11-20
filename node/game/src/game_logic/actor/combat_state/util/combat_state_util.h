#pragma once
#include <entt/src/entt/entity/entity.hpp>

class CombatStateAddedPbEvent;
class CombatStateRemovedPbEvent;

class CombatStateUtil
{
public:
    static void InitializeActorComponents(entt::entity entity);

    static void AddCombatState(const CombatStateAddedPbEvent& event);
    
    static void RemoveCombatState(const CombatStateRemovedPbEvent& event);
};