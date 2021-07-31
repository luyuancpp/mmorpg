#ifndef COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_COMPONENT_H_
#define COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_COMPONENT_H_

#include <memory>
#include <string>

namespace common
{
using SharedAccountString = std::shared_ptr<std::string>;

struct PlayerGatewayId
{
    uint64_t gate_way_id{UINT64_MAX};
};
}//namespace common

#endif//COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_COMPONENT_H_
