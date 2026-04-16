#pragma once

#include "proto/common/event/actor_combat_state_event.pb.h"

class ActorCombatStateEventHandler
{
public:
    static void Register();

    static void UnRegister();
    static void CombatStateAddedEventHandler(const CombatStateAddedEvent& event);
    static void CombatStateRemovedEventHandler(const CombatStateRemovedEvent& event);
};
