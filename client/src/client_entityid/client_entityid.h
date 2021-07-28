#ifndef CLIENT_SRC_CLIENT_ENTITYID_CLIENT_ENTITY_ID_H_
#define CLIENT_SRC_CLIENT_ENTITYID_CLIENT_ENTITY_ID_H_

#include "src/game_logic/game_registry.h"

namespace client
{
    class ClientEntityId
    {
    public:
        static entt::registry::entity_type gAllLeaveGame;
        static entt::registry::entity_type gAllFinish;
        static entt::registry::entity_type gAllConnected;
    };
}//namespace client

#endif//CLIENT_SRC_CLIENT_ENTITYID_CLIENT_ENTITY_ID_H_
