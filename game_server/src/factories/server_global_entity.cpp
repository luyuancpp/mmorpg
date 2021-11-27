#include "server_global_entity.hpp"

namespace game
{
entt::entity& global_entity()
{
    thread_local entt::entity singleton;
    return singleton;
}
}//namespace game

