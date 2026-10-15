#include "game_registry.h"

#include <muduo/base/Logging.h>

static_assert(sizeof(uint64_t) == sizeof(entt::entity), "sizeof(uint64_t) == sizeof(entt::entity)");


void DestroyEntity(entt::registry& registry, entt::entity entity)
{
    if (!registry.valid(entity))
    {
        LOG_TRACE << "DestroyEntity: entity already invalid, id=" << entt::to_integral(entity);
        return;
    }
    registry.destroy(entity);
}
