#pragma once
#include <any>
#include "entt/src/entt/entity/registry.hpp"
#include "engine/core/type_define/type_define.h"

class player_centre_database;

class PlayerLifecycleSystem
{
public:
    static void HandlePlayerAsyncLoaded(Guid player_id, const player_centre_database& message, const std::any& extra);
    static void HandlePlayerAsyncSaved(Guid player_id, player_centre_database& message);
    static void ProcessPlayerSessionState(entt::entity player);
    static void HandlePlayerLogin(entt::entity player);
    static void HandlePlayerReconnection(entt::entity player);
    static void AddGameNodePlayerToGateNode(entt::entity player);
    static void HandleRoomNodePlayerRegisteredAtGate(entt::entity player);
    static void HandleNormalExit(Guid playerID);
    static void HandleAbnormalExit(Guid playerID);
    static void Logout(Guid playerID);
};
