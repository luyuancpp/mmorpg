#ifndef COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_COMPONENT_H_
#define COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_COMPONENT_H_

#include <memory>
#include <string>

#include "src/common_type/common_type.h"

#include "entt/src/entt/entity/entity.hpp"

namespace common
{

struct GateConnId
{
     uint64_t conn_id_{UINT64_MAX};
};

using ConnectionPlayerEnitiesMap = std::unordered_map<uint64_t, entt::entity>;

struct PlayerId 
{
    Guid guid_{0};
};
}//namespace common

#endif//COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_COMPONENT_H_
