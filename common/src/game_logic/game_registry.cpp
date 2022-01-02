#include "game_registry.h"

namespace common
{
    thread_local entt::registry reg;

    entt::entity& error_entity()
    {
        thread_local entt::entity singleton = reg.create();
        return singleton;
    }

    entt::entity& op_enitty()
    {
        thread_local entt::entity singleton = reg.create();
        return singleton;
    }

}//namespace common
