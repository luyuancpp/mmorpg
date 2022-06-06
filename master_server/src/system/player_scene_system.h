#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerSceneSystem
{
public:
    static void EnterScene(entt::entity player);
    static void LeaveScene(entt::entity player, bool change_gs);
};
