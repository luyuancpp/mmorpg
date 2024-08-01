#pragma once

#include "entt/src/entt/entity/registry.hpp"
#include "type_define/type_define.h"

class player_centre_database;

class PlayerNodeUtil
{
public:
    static void HandlePlayerAsyncLoaded(Guid player_id, const player_centre_database& message);
    static void HandlePlayerAsyncSaved(Guid player_id, player_centre_database& message);
    static void HandlePlayerLogin(entt::entity player);
    static void RegisterPlayerToGateNode(entt::entity player);
    static void OnPlayerRegisteredToGateNode(entt::entity player);
    static void HandlePlayerLeave(Guid player_uid);
};
