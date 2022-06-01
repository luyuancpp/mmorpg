#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerCommonSystem
{
public:
    void OnLogin(entt::entity player);
};

extern PlayerCommonSystem g_player_common_sys;
