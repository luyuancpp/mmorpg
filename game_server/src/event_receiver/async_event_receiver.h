#pragma once
#include "src/game_logic/game_registry.h"

class AsyncEventReceiverEvent
{
public:
    static void Register(entt::dispatcher& dispatcher);
    static void UnRegister(entt::dispatcher& dispatcher);
};
