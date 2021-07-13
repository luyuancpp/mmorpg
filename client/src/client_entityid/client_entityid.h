#ifndef CLIENT_SRC_CLIENT_ENTITYID_CLIENT_ENTITY_ID_H_
#define CLIENT_SRC_CLIENT_ENTITYID_CLIENT_ENTITY_ID_H_

#include "src/game_ecs/game_registry.h"

namespace client
{
    static entt::registry::entity_type gAllLeaveGame;
    static entt::registry::entity_type gAllFinish;

}//namespace client

#endif//CLIENT_SRC_CLIENT_ENTITYID_CLIENT_ENTITY_ID_H_
