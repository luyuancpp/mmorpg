#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerCommonSys
{
public:
    void OnLogin(entt::entity player);
};

extern PlayerCommonSys g_player_common_sys;
