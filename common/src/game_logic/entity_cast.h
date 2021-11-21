#ifndef COMMON_SRC_GAME_ECS_ENTITY_CAST_H_
#define COMMON_SRC_GAME_ECS_ENTITY_CAST_H_

#include "entt/src/entt/fwd.hpp"

namespace entt
{
    [[nodiscard]] static constexpr entt::entity to_entity(uint64_t value) ENTT_NOEXCEPT {
        return static_cast<entity>(value);
    }
}//namespace entt

#endif//COMMON_SRC_GAME_ECS_ENTITY_CAST_H_
