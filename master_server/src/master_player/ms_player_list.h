#ifndef MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_
#define MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_

#include "src/common_type/common_type.h"
#include "src/game_logic/game_registry.h"


using PlayerListMap = std::unordered_map<common::Guid, common::EntityPtr>;
extern thread_local PlayerListMap  g_players;
class PlayerList
{
public:
    static PlayerList& GetSingleton()
    {
        thread_local PlayerList singleton;
        return singleton;
    }

    std::size_t player_size()const { return g_players.size(); }
    bool empty()const { return g_players.empty(); }
    entt::entity GetPlayer(common::Guid guid);
    bool HasPlayer(common::Guid guid) const { return g_players.find(guid) != g_players.end(); }

    void EnterGame(common::Guid guid, const common::EntityPtr&& entity_id) { g_players.emplace(guid, entity_id); }
    void LeaveGame(common::Guid guid);
};


#endif//MASTER_SERVER_SRC_MASTER_PLAYER_MASTER_PLAYER_LIST_H_
