#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerCommonSystem
{
public:
    static void InitPlayerCompnent(entt::entity player);
    static void OnLogin(entt::entity player);
};
