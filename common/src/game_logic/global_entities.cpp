#include "global_entities.h"

namespace common
{

entt::entity& scenes_entity()
{
    static entt::entity scenes_entity = reg().create();
    return scenes_entity;
}

entt::entity& gameserver_entity()
{
    static entt::entity gameserver_entity = reg().create();
    return gameserver_entity;
}

}//namespace common