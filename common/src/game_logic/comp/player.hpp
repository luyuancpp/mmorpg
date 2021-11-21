#ifndef COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_COMPONENT_H_
#define COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_COMPONENT_H_

#include <memory>
#include <string>

#include "src/common_type/common_type.h"

namespace common
{
using SharedAccountString = std::shared_ptr<std::string>;

struct GatewayConnectionId
{
    uint64_t connection_id_{UINT64_MAX};
};

struct TryEnterGamePlayerId
{
    GameGuid guid_{ 0 };
};

struct PlayerId
{
    GameGuid guid_{0};
};
}//namespace common

#endif//COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_COMPONENT_H_
