#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "src/common_type/common_type.h"

class PlayerCommonSystem
{
public:
    static void InitPlayerComponent(entt::entity player, Guid player_id);
    static void OnGateUpdateGameNodeSucceed(entt::entity player);
    static void OnLogin(entt::entity player);
};
