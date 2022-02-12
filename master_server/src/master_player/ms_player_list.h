#ifndef MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_
#define MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"

namespace master
{
    class PlayerList
    {
    public:
        using GameGuidEntityIdMap = std::unordered_map<common::Guid, entt::entity>;
        static PlayerList& GetSingleton()
        {
            thread_local PlayerList singleton;
            return singleton;
        }

        std::size_t player_size()const { return player_list_.size(); }
        bool empty()const { return player_list_.empty(); }
        entt::entity GetPlayer(common::Guid guid);
        bool HasPlayer(common::Guid guid) const { return player_list_.find(guid) != player_list_.end(); }

        void EnterGame(common::Guid guid, entt::entity entity_id){  player_list_.emplace(guid, entity_id);       }
        void LeaveGame(common::Guid guid){ player_list_.erase(guid); }
        
    private:
        GameGuidEntityIdMap player_list_;
    };
}//namespace master

#endif//MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_
