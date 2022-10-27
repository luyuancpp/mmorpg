#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerSceneSystem
{
public:
    static void Send2GsEnterScene(entt::entity player);
    static void SendEnterGs(entt::entity player);
    static uint32_t ChangeScene(entt::entity player, entt::entity to_scene);
};
