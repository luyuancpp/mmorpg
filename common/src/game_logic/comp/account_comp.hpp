#ifndef COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_ACCOUNT_COMP_H_
#define COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_ACCOUNT_COMP_H_

#include <memory>
#include <string>

#include "src/common_type/common_type.h"

#include "entt/src/entt/entity/entity.hpp"

namespace common
{
    using SharedAccountString = std::shared_ptr<std::string>;

}//namespace common

#endif//COMMON_SRC_GAME_ECS_GAME_COMPONENT_PLAYER_COMMON_ACCOUNT_COMP_H_
