#pragma once

#include "src/type_define/type_define.h"
#include "src/util/game_registry.h"

class ControllerPlayerSystem
{
public:
    static entt::entity  GetPlayer(Guid guid);

    static void LeaveGame(Guid guid);
};
