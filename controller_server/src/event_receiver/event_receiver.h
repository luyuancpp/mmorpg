#pragma once
#include "src/game_logic/thread_local/thread_local_storage.h"

class EventReceiver
{
public:
    static void Register(entt::dispatcher& dispatcher);
    static void UnRegister(entt::dispatcher& dispatcher);
};
