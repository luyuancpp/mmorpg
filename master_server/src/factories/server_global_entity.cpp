#include "server_global_entity.hpp"


entt::entity& global_entity()
{
    thread_local entt::entity g_global_entity;
    return g_global_entity;
}


