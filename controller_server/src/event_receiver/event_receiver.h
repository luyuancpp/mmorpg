#pragma once
#include "src/util/game_registry.h"

class EventReceiverEvent
{
public:
    static void Register(entt::dispatcher& dispatcher);
    static void UnRegister(entt::dispatcher& dispatcher);
};
