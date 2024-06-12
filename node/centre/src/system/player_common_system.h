#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "type_define/type_define.h"

class PlayerCommonSystem
{
public:
    static void InitPlayerComponent(entt::entity eid, Guid player_id);
    static void OnGateUpdateGameNodeSucceed(entt::entity player);
    static void OnLogin(entt::entity player);
};
