#include "server_global_entity.hpp"

entt::entity& global_entity()
{
    thread_local entt::entity singleton;
    return singleton;
}

