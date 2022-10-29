#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerSceneSystem
{
public:
    static void Send2GsEnterScene(entt::entity player);
    static void SendEnterGs(entt::entity player);
};
