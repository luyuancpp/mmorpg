#pragma once

#include "entt/src/entt/entity/registry.hpp"

class PlayerCommonSystem
{
public:
    static void InitPlayerComponent(entt::entity player);
    static void OnEnterGateSucceed(entt::entity player);
    static void OnLogin(entt::entity player);
};
