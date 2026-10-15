#pragma once
#include <cstdint>
#include "entt/src/entt/entity/entity.hpp"

// player 属于 actorRegistry;enterGsType 是本次已提交登录的类型,不是外部实体标识。
struct RegisterPlayerEvent { entt::entity player{entt::null}; };
struct PlayerUpgradeEvent { entt::entity player{entt::null}; uint32_t newLevel{0}; };
struct InitializePlayerCompsEvent { entt::entity player{entt::null}; };
struct PlayerLoginEvent { entt::entity player{entt::null}; uint32_t enterGsType{0}; };
