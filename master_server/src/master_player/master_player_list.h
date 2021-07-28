#ifndef MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_
#define MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

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

        entt::entity GetPlayer(common::GameGuid player_id);
        bool HasPlayer(common::GameGuid player_id) const { return player_list_.find(player_id) != player_list_.end(); }

        void EnterGame(common::GameGuid player_id, entt::entity entity_id){  player_list_.emplace(player_id, entity_id);       }
        void LeaveGame(common::GameGuid player_id){ player_list_.erase(player_id); }
        
    private:
        GameGuidEntityIdMap player_list_;
    };
}//namespace master

#endif//MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_
