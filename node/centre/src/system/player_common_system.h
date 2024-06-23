#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "type_define/type_define.h"

class player_centre_database;

class PlayerCommonSystem
{
public:
    static void OnPlayerAsyncLoaded(Guid player_id, const player_centre_database& message);

    static void OnPlayerAsyncSaved(Guid player_id, player_centre_database& message);

    static void OnGateUpdateGameNodeSucceed(entt::entity player);
    static void OnLogin(entt::entity player);

    static void RegisterGatePlayerGameNode(entt::entity player);
};
