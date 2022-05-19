#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerSceneSystem
{
public:
    void OnEnterScene(entt::entity player);
};

extern PlayerSceneSystem g_player_scene_system;