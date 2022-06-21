#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerSceneSystem
{
public:
    static void OnEnterScene(entt::entity player);
    static void LeaveScene(entt::entity player, bool change_gs);

    static void SendEnterGs(entt::entity player);
};
