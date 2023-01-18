#pragma once
#include "src/game_logic/thread_local/game_registry.h"

class EventReceiver
{
public:
    static void Register(entt::dispatcher& dispatcher);
    static void UnRegister(entt::dispatcher& dispatcher);
};
