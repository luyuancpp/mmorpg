#include "server_global_entity.hpp"

namespace game
{
entt::entity& global_entity()
{
    static entt::entity g_global_entity;
    return g_global_entity;
}
}//namespace game

