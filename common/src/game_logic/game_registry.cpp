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
        static entt::entity singleton = reg().create();
        return singleton;
    }

    entt::entity& op_enitty()
    {
        static entt::entity singleton = reg().create();
        return singleton;
    }

}//namespace common
