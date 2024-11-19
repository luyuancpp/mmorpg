#pragma once
#include <entt/src/entt/entity/entity.hpp>

class SilenceAddedPbEvent;
class SilenceRemovedPbEvent;

class CombatStateUtil
{
public:
    static void InitializeActorComponents(entt::entity entity);

    static void AddSilence(const SilenceAddedPbEvent& event);
    
    static void RemoveSilence(const SilenceRemovedPbEvent& event);
};