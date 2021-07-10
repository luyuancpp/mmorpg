#ifndef MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_
#define MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_

#include "src/common_type/common_type.h"
#include "src/server_registry/server_registry.h"

namespace master
{
    class MasterPlayerList
    {
    public:
        using GameGuidEntityIdMap = std::unordered_map<common::GameGuid, entt::entity>;
        static MasterPlayerList& GetSingleton()
        {
            static MasterPlayerList singleton;
            return singleton;
        }

        void EnterGame(common::GameGuid player_id, entt::entity entity_id);

    private:
        GameGuidEntityIdMap player_list_;
    };
}//namespace master

#endif//MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_
