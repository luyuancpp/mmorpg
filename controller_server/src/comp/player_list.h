#pragma once

#include "src/common_type/common_type.h"
#include "src/util/game_registry.h"

class ControllerPlayerSystem
{
public:
    static entt::entity  GetPlayer(Guid guid);

    static void LeaveGame(Guid guid);
};
