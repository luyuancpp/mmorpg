#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerSceneSystem
{
public:
    static void OnEnterScene(entt::entity player);
    static void OnLeaveScene(entt::entity player, bool change_gs);
    static void SendEnterGs(entt::entity player);
    static uint32_t ChangeScene(entt::entity player, entt::entity scene);
};
