#include "game_registry.h"

namespace common
{
    entt::registry& reg()
    {
        static entt::registry singleton;
        return singleton;
    }

    entt::entity& error_entity()
    {
        static entt::entity error_entity = reg().create();
        return error_entity;
    }

}//namespace common
